/*
 * Copyright (C) 2016 Red Hat
 * Copyright (C) 2017-2019 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Adapted from mutter:
 * - src/backends/meta-monitor.c
 */

#include "config.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <math.h>
#include <string.h>

#include "gf-crtc-private.h"
#include "gf-gpu-private.h"
#include "gf-monitor-manager-private.h"
#include "gf-monitor-private.h"
#include "gf-monitor-spec-private.h"
#include "gf-output-private.h"
#include "gf-settings-private.h"

#define SCALE_FACTORS_PER_INTEGER 4
#define SCALE_FACTORS_STEPS (1.0f / (float) SCALE_FACTORS_PER_INTEGER)
#define MINIMUM_SCALE_FACTOR 1.0f
#define MAXIMUM_SCALE_FACTOR 4.0f
#define MINIMUM_LOGICAL_AREA (800 * 480)
#define MAXIMUM_REFRESH_RATE_DIFF 0.001f

/* The minimum screen height at which we turn on a window-scale of 2;
 * below this there just isn't enough vertical real estate for GNOME
 * apps to work, and it's better to just be tiny
 */
#define HIDPI_MIN_HEIGHT 1200

/* From http://en.wikipedia.org/wiki/4K_resolution#Resolutions_of_common_formats */
#define SMALLEST_4K_WIDTH 3656

/* The minimum resolution at which we turn on a window-scale of 2 */
#define HIDPI_LIMIT 192

typedef struct
{
  GfBackend        *backend;

  GList            *outputs;
  GList            *modes;
  GHashTable       *mode_ids;

  GfMonitorMode    *preferred_mode;
  GfMonitorMode    *current_mode;

  GfMonitorSpec    *spec;

  GfLogicalMonitor *logical_monitor;

  /*
   * The primary or first output for this monitor, 0 if we can't figure out.
   * It can be matched to a winsys_id of a GfOutput.
   *
   * This is used as an opaque token on reconfiguration when switching from
   * clone to extened, to decide on what output the windows should go next
   * (it's an attempt to keep windows on the same monitor, and preferably on
   * the primary one).
   */
  glong             winsys_id;

  char             *display_name;
} GfMonitorPrivate;

enum
{
  PROP_0,

  PROP_BACKEND,

  LAST_PROP
};

static GParamSpec *monitor_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE_WITH_PRIVATE (GfMonitor, gf_monitor, G_TYPE_OBJECT)

static const gdouble known_diagonals[] =
  {
    12.1,
    13.3,
    15.6
  };

static gchar *
diagonal_to_str (gdouble d)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (known_diagonals); i++)
    {
      gdouble delta;

      delta = fabs(known_diagonals[i] - d);

      if (delta < 0.1)
        return g_strdup_printf ("%0.1lf\"", known_diagonals[i]);
    }

  return g_strdup_printf ("%d\"", (int) (d + 0.5));
}

static gchar *
make_display_name (GfMonitor        *monitor,
                   GfMonitorManager *manager)
{
  gchar *inches;
  gchar *vendor_name;
  const char *vendor;
  const char *product_name;
  int width_mm;
  int height_mm;

  if (gf_monitor_is_laptop_panel (monitor))
    return g_strdup (_("Built-in display"));

  inches = NULL;
  vendor_name = NULL;
  vendor = gf_monitor_get_vendor (monitor);
  product_name = NULL;

  gf_monitor_get_physical_dimensions (monitor, &width_mm, &height_mm);

  if (width_mm > 0 && height_mm > 0)
    {
      if (!gf_monitor_has_aspect_as_size (monitor))
        {
          double d;

          d = sqrt (width_mm * width_mm + height_mm * height_mm);
          inches = diagonal_to_str (d / 25.4);
        }
      else
        {
          product_name = gf_monitor_get_product (monitor);
        }
    }

  if (g_strcmp0 (vendor, "unknown") != 0)
    {
      vendor_name = gf_monitor_manager_get_vendor_name (manager, vendor);

      if (!vendor_name)
        vendor_name = g_strdup (vendor);
    }
  else
    {
      if (inches != NULL)
        vendor_name = g_strdup (_("Unknown"));
      else
        vendor_name = g_strdup (_("Unknown Display"));
    }

  if (inches != NULL)
    {
      gchar *display_name;

      display_name = g_strdup_printf (C_("This is a monitor vendor name, followed by a "
                                         "size in inches, like 'Dell 15\"'",
                                         "%s %s"), vendor_name, inches);

      g_free (vendor_name);
      g_free (inches);

      return display_name;
    }
  else if (product_name != NULL)
    {
      gchar *display_name;

      display_name =  g_strdup_printf (C_("This is a monitor vendor name followed by "
                                          "product/model name where size in inches "
                                          "could not be calculated, e.g. Dell U2414H",
                                          "%s %s"), vendor_name, product_name);

      g_free (vendor_name);

      return display_name;
    }

  return vendor_name;
}

static gboolean
is_current_mode_known (GfMonitor *monitor)
{
  GfOutput *output;
  GfCrtc *crtc;

  output = gf_monitor_get_main_output (monitor);
  crtc = gf_output_get_assigned_crtc (output);

  return gf_monitor_is_active (monitor) == (crtc && gf_crtc_get_config (crtc));
}

static gboolean
gf_monitor_mode_spec_equals (GfMonitorModeSpec *spec,
                             GfMonitorModeSpec *other_spec)
{
  gfloat refresh_rate_diff;

  refresh_rate_diff = ABS (spec->refresh_rate - other_spec->refresh_rate);

  return (spec->width == other_spec->width &&
          spec->height == other_spec->height &&
          refresh_rate_diff < MAXIMUM_REFRESH_RATE_DIFF &&
          spec->flags == other_spec->flags);
}

static float
calculate_scale (GfMonitor                 *monitor,
                 GfMonitorMode             *monitor_mode,
                 GfMonitorScalesConstraint  constraints)
{
  gint resolution_width, resolution_height;
  gint width_mm, height_mm;
  gint scale;

  scale = 1.0;

  gf_monitor_mode_get_resolution (monitor_mode,
                                  &resolution_width,
                                  &resolution_height);

  if (resolution_height < HIDPI_MIN_HEIGHT)
    return scale;

  /* 4K TV */
  switch (gf_monitor_get_connector_type (monitor))
    {
      case GF_CONNECTOR_TYPE_HDMIA:
      case GF_CONNECTOR_TYPE_HDMIB:
        if (resolution_width < SMALLEST_4K_WIDTH)
          return scale;
        break;

      case GF_CONNECTOR_TYPE_Unknown:
      case GF_CONNECTOR_TYPE_VGA:
      case GF_CONNECTOR_TYPE_DVII:
      case GF_CONNECTOR_TYPE_DVID:
      case GF_CONNECTOR_TYPE_DVIA:
      case GF_CONNECTOR_TYPE_Composite:
      case GF_CONNECTOR_TYPE_SVIDEO:
      case GF_CONNECTOR_TYPE_LVDS:
      case GF_CONNECTOR_TYPE_Component:
      case GF_CONNECTOR_TYPE_9PinDIN:
      case GF_CONNECTOR_TYPE_DisplayPort:
      case GF_CONNECTOR_TYPE_TV:
      case GF_CONNECTOR_TYPE_eDP:
      case GF_CONNECTOR_TYPE_VIRTUAL:
      case GF_CONNECTOR_TYPE_DSI:
      case GF_CONNECTOR_TYPE_DPI:
      case GF_CONNECTOR_TYPE_WRITEBACK:
      case GF_CONNECTOR_TYPE_SPI:
      case GF_CONNECTOR_TYPE_USB:
      default:
        break;
    }

  gf_monitor_get_physical_dimensions (monitor, &width_mm, &height_mm);

  /* Somebody encoded the aspect ratio (16/9 or 16/10) instead of the
   * physical size.
   */
  if (gf_monitor_has_aspect_as_size (monitor))
    return scale;

  if (width_mm > 0 && height_mm > 0)
    {
      gdouble dpi_x, dpi_y;

      dpi_x = (gdouble) resolution_width / (width_mm / 25.4);
      dpi_y = (gdouble) resolution_height / (height_mm / 25.4);

      /*
       * We don't completely trust these values so both must be high, and never
       * pick higher ratio than 2 automatically.
       */
      if (dpi_x > HIDPI_LIMIT && dpi_y > HIDPI_LIMIT)
        scale = 2.0;
    }

  return scale;
}

static gboolean
is_logical_size_large_enough (gint width,
                              gint height)
{
  return width * height >= MINIMUM_LOGICAL_AREA;
}

static gboolean
is_scale_valid_for_size (float width,
                         float height,
                         float scale)
{
  if (scale < MINIMUM_SCALE_FACTOR || scale > MAXIMUM_SCALE_FACTOR)
    return FALSE;

  return is_logical_size_large_enough (floorf (width / scale),
                                       floorf (height / scale));
}

float
gf_get_closest_monitor_scale_factor_for_resolution (float width,
                                                    float height,
                                                    float scale,
                                                    float threshold)
{
  guint i, j;
  gfloat scaled_h;
  gfloat scaled_w;
  gfloat best_scale;
  gint base_scaled_w;
  gboolean found_one;

  best_scale = 0;

  if (fmodf (width, scale) == 0.0f && fmodf (height, scale) == 0.0f)
    return scale;

  i = 0;
  found_one = FALSE;
  base_scaled_w = floorf (width / scale);

  do
    {
      for (j = 0; j < 2; j++)
        {
          gfloat current_scale;
          gint offset = i * (j ? 1 : -1);

          scaled_w = base_scaled_w + offset;
          current_scale = width / scaled_w;
          scaled_h = height / current_scale;

          if (current_scale >= scale + threshold ||
              current_scale <= scale - threshold ||
              current_scale < MINIMUM_SCALE_FACTOR ||
              current_scale > MAXIMUM_SCALE_FACTOR)
            {
              return best_scale;
            }

          if (floorf (scaled_h) == scaled_h)
            {
              found_one = TRUE;

              if (fabsf (current_scale - scale) < fabsf (best_scale - scale))
                best_scale = current_scale;
            }
        }

      i++;
    }
  while (!found_one);

  return best_scale;
}

static void
unset_monitor (gpointer data,
               gpointer user_data)
{
  GfOutput *output;

  output = GF_OUTPUT (data);

  gf_output_unset_monitor (output);
}

static void
gf_monitor_dispose (GObject *object)
{
  GfMonitor *monitor;
  GfMonitorPrivate *priv;

  monitor = GF_MONITOR (object);
  priv = gf_monitor_get_instance_private (monitor);

  if (priv->outputs)
    {
      g_list_foreach (priv->outputs, unset_monitor, NULL);
      g_list_free_full (priv->outputs, g_object_unref);
      priv->outputs = NULL;
    }

  G_OBJECT_CLASS (gf_monitor_parent_class)->dispose (object);
}

static void
gf_monitor_finalize (GObject *object)
{
  GfMonitor *monitor;
  GfMonitorPrivate *priv;

  monitor = GF_MONITOR (object);
  priv = gf_monitor_get_instance_private (monitor);

  g_hash_table_destroy (priv->mode_ids);
  g_list_free_full (priv->modes, (GDestroyNotify) gf_monitor_mode_free);
  gf_monitor_spec_free (priv->spec);
  g_free (priv->display_name);

  G_OBJECT_CLASS (gf_monitor_parent_class)->finalize (object);
}

static void
gf_monitor_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GfMonitor *monitor;
  GfMonitorPrivate *priv;

  monitor = GF_MONITOR (object);
  priv = gf_monitor_get_instance_private (monitor);

  switch (property_id)
    {
      case PROP_BACKEND:
        g_value_set_object (value, priv->backend);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_monitor_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GfMonitor *monitor;
  GfMonitorPrivate *priv;

  monitor = GF_MONITOR (object);
  priv = gf_monitor_get_instance_private (monitor);

  switch (property_id)
    {
      case PROP_BACKEND:
        priv->backend = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_monitor_install_properties (GObjectClass *object_class)
{
  monitor_properties[PROP_BACKEND] =
    g_param_spec_object ("backend",
                         "GfBackend",
                         "GfBackend",
                         GF_TYPE_BACKEND,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     monitor_properties);
}

static void
gf_monitor_class_init (GfMonitorClass *monitor_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (monitor_class);

  object_class->dispose = gf_monitor_dispose;
  object_class->finalize = gf_monitor_finalize;
  object_class->get_property = gf_monitor_get_property;
  object_class->set_property = gf_monitor_set_property;

  gf_monitor_install_properties (object_class);
}

static void
gf_monitor_init (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  priv->mode_ids = g_hash_table_new (g_str_hash, g_str_equal);
}

GfBackend *
gf_monitor_get_backend (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  return priv->backend;
}

void
gf_monitor_make_display_name (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;
  GfMonitorManager *manager;

  priv = gf_monitor_get_instance_private (monitor);

  manager = gf_backend_get_monitor_manager (priv->backend);

  g_free (priv->display_name);
  priv->display_name = make_display_name (monitor, manager);
}

const char *
gf_monitor_get_display_name (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  return priv->display_name;
}

gboolean
gf_monitor_is_mode_assigned (GfMonitor     *monitor,
                             GfMonitorMode *mode)
{
  GfMonitorPrivate *priv;
  GList *l;
  gint i;

  priv = gf_monitor_get_instance_private (monitor);

  for (l = priv->outputs, i = 0; l; l = l->next, i++)
    {
      GfOutput *output;
      GfMonitorCrtcMode *monitor_crtc_mode;
      GfCrtc *crtc;
      const GfCrtcConfig *crtc_config;

      output = l->data;
      monitor_crtc_mode = &mode->crtc_modes[i];
      crtc = gf_output_get_assigned_crtc (output);
      crtc_config = crtc ? gf_crtc_get_config (crtc) : NULL;

      if (monitor_crtc_mode->crtc_mode &&
          (!crtc || !crtc_config ||
           crtc_config->mode != monitor_crtc_mode->crtc_mode))
        return FALSE;
      else if (!monitor_crtc_mode->crtc_mode && crtc)
        return FALSE;
    }

  return TRUE;
}

void
gf_monitor_append_output (GfMonitor *monitor,
                          GfOutput  *output)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  priv->outputs = g_list_append (priv->outputs, g_object_ref (output));
}

void
gf_monitor_set_winsys_id (GfMonitor *monitor,
                          glong      winsys_id)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  priv->winsys_id = winsys_id;
}

void
gf_monitor_set_preferred_mode (GfMonitor     *monitor,
                               GfMonitorMode *mode)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  priv->preferred_mode = mode;
}

GfMonitorModeSpec
gf_monitor_create_spec (GfMonitor  *monitor,
                        int         width,
                        int         height,
                        GfCrtcMode *crtc_mode)
{
  const GfOutputInfo *output_info;
  const GfCrtcModeInfo *crtc_mode_info;
  GfMonitorModeSpec spec;

  output_info = gf_monitor_get_main_output_info (monitor);
  crtc_mode_info = gf_crtc_mode_get_info (crtc_mode);

  if (gf_monitor_transform_is_rotated (output_info->panel_orientation_transform))
    {
      int temp;

      temp = width;
      width = height;
      height = temp;
    }

  spec.width = width;
  spec.height = height;
  spec.refresh_rate = crtc_mode_info->refresh_rate;
  spec.flags = crtc_mode_info->flags & HANDLED_CRTC_MODE_FLAGS;

  return spec;
}

const GfOutputInfo *
gf_monitor_get_main_output_info (GfMonitor *self)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (self);

  return gf_output_get_info (output);
}

void
gf_monitor_generate_spec (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;
  const GfOutputInfo *output_info;
  GfMonitorSpec *monitor_spec;

  priv = gf_monitor_get_instance_private (monitor);
  output_info = gf_monitor_get_main_output_info (monitor);

  monitor_spec = g_new0 (GfMonitorSpec, 1);

  monitor_spec->connector = g_strdup (output_info->name);
  monitor_spec->vendor = g_strdup (output_info->vendor);
  monitor_spec->product = g_strdup (output_info->product);
  monitor_spec->serial = g_strdup (output_info->serial);

  priv->spec = monitor_spec;
}

gboolean
gf_monitor_add_mode (GfMonitor     *monitor,
                     GfMonitorMode *monitor_mode,
                     gboolean       replace)
{
  GfMonitorPrivate *priv;
  GfMonitorMode *existing_mode;

  priv = gf_monitor_get_instance_private (monitor);

  existing_mode = g_hash_table_lookup (priv->mode_ids,
                                       gf_monitor_mode_get_id (monitor_mode));

  if (existing_mode && !replace)
    return FALSE;

  if (existing_mode)
    priv->modes = g_list_remove (priv->modes, existing_mode);

  priv->modes = g_list_append (priv->modes, monitor_mode);
  g_hash_table_replace (priv->mode_ids, monitor_mode->id, monitor_mode);

  return TRUE;
}

void
gf_monitor_mode_free (GfMonitorMode *monitor_mode)
{
  g_free (monitor_mode->id);
  g_free (monitor_mode->crtc_modes);
  g_free (monitor_mode);
}

gchar *
gf_monitor_mode_spec_generate_id (GfMonitorModeSpec *spec)
{
  gboolean is_interlaced;

  is_interlaced = !!(spec->flags & GF_CRTC_MODE_FLAG_INTERLACE);

  return g_strdup_printf ("%dx%d%s@%.3f",
                          spec->width,
                          spec->height,
                          is_interlaced ? "i" : "",
                          (double) spec->refresh_rate);
}

GfMonitorSpec *
gf_monitor_get_spec (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  return priv->spec;
}

gboolean
gf_monitor_is_active (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  return !!priv->current_mode;
}

GfOutput *
gf_monitor_get_main_output (GfMonitor *monitor)
{
  return GF_MONITOR_GET_CLASS (monitor)->get_main_output (monitor);
}

gboolean
gf_monitor_is_primary (GfMonitor *monitor)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);

  return gf_output_is_primary (output);
}

gboolean
gf_monitor_supports_underscanning (GfMonitor *monitor)
{
  const GfOutputInfo *output_info;

  output_info = gf_monitor_get_main_output_info (monitor);

  return output_info->supports_underscanning;
}

gboolean
gf_monitor_is_underscanning (GfMonitor *monitor)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);

  return gf_output_is_underscanning (output);
}

gboolean
gf_monitor_get_max_bpc (GfMonitor    *self,
                        unsigned int *max_bpc)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (self);

  return gf_output_get_max_bpc (output, max_bpc);
}

gboolean
gf_monitor_is_laptop_panel (GfMonitor *monitor)
{
  const GfOutputInfo *output_info;

  output_info = gf_monitor_get_main_output_info (monitor);

  switch (output_info->connector_type)
    {
      case GF_CONNECTOR_TYPE_eDP:
      case GF_CONNECTOR_TYPE_LVDS:
      case GF_CONNECTOR_TYPE_DSI:
        return TRUE;

      case GF_CONNECTOR_TYPE_HDMIA:
      case GF_CONNECTOR_TYPE_HDMIB:
      case GF_CONNECTOR_TYPE_Unknown:
      case GF_CONNECTOR_TYPE_VGA:
      case GF_CONNECTOR_TYPE_DVII:
      case GF_CONNECTOR_TYPE_DVID:
      case GF_CONNECTOR_TYPE_DVIA:
      case GF_CONNECTOR_TYPE_Composite:
      case GF_CONNECTOR_TYPE_SVIDEO:
      case GF_CONNECTOR_TYPE_Component:
      case GF_CONNECTOR_TYPE_9PinDIN:
      case GF_CONNECTOR_TYPE_DisplayPort:
      case GF_CONNECTOR_TYPE_TV:
      case GF_CONNECTOR_TYPE_VIRTUAL:
      case GF_CONNECTOR_TYPE_DPI:
      case GF_CONNECTOR_TYPE_WRITEBACK:
      case GF_CONNECTOR_TYPE_SPI:
      case GF_CONNECTOR_TYPE_USB:
      default:
        break;
    }

  return FALSE;
}

gboolean
gf_monitor_is_same_as (GfMonitor *monitor,
                       GfMonitor *other_monitor)
{
  GfMonitorPrivate *priv;
  GfMonitorPrivate *other_priv;

  priv = gf_monitor_get_instance_private (monitor);
  other_priv = gf_monitor_get_instance_private (other_monitor);

  return priv->winsys_id == other_priv->winsys_id;
}

GList *
gf_monitor_get_outputs (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  return priv->outputs;
}

void
gf_monitor_get_current_resolution (GfMonitor *monitor,
                                   gint      *width,
                                   gint      *height)
{
  GfMonitorMode *mode = gf_monitor_get_current_mode (monitor);

  *width = mode->spec.width;
  *height = mode->spec.height;
}

void
gf_monitor_derive_layout (GfMonitor   *monitor,
                          GfRectangle *layout)
{
  GF_MONITOR_GET_CLASS (monitor)->derive_layout (monitor, layout);
}

void
gf_monitor_get_physical_dimensions (GfMonitor *monitor,
                                    gint      *width_mm,
                                    gint      *height_mm)
{
  const GfOutputInfo *output_info;

  output_info = gf_monitor_get_main_output_info (monitor);

  *width_mm = output_info->width_mm;
  *height_mm = output_info->height_mm;
}

const gchar *
gf_monitor_get_connector (GfMonitor *monitor)
{
  const GfOutputInfo *output_info;

  output_info = gf_monitor_get_main_output_info (monitor);

  return output_info->name;
}

const gchar *
gf_monitor_get_vendor (GfMonitor *monitor)
{
  const GfOutputInfo *output_info;

  output_info = gf_monitor_get_main_output_info (monitor);

  return output_info->vendor;
}

const gchar *
gf_monitor_get_product (GfMonitor *monitor)
{
  const GfOutputInfo *output_info;

  output_info = gf_monitor_get_main_output_info (monitor);

  return output_info->product;
}

const gchar *
gf_monitor_get_serial (GfMonitor *monitor)
{
  const GfOutputInfo *output_info;

  output_info = gf_monitor_get_main_output_info (monitor);

  return output_info->serial;
}

GfConnectorType
gf_monitor_get_connector_type (GfMonitor *monitor)
{
  const GfOutputInfo *output_info;

  output_info = gf_monitor_get_main_output_info (monitor);

  return output_info->connector_type;
}

GfMonitorTransform
gf_monitor_logical_to_crtc_transform (GfMonitor          *monitor,
                                      GfMonitorTransform  transform)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);

  return gf_output_logical_to_crtc_transform (output, transform);
}

GfMonitorTransform
gf_monitor_crtc_to_logical_transform (GfMonitor          *monitor,
                                      GfMonitorTransform  transform)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);

  return gf_output_crtc_to_logical_transform (output, transform);
}

gboolean
gf_monitor_get_suggested_position (GfMonitor *monitor,
                                   gint      *x,
                                   gint      *y)
{
  return GF_MONITOR_GET_CLASS (monitor)->get_suggested_position (monitor, x, y);
}

GfLogicalMonitor *
gf_monitor_get_logical_monitor (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  return priv->logical_monitor;
}

GfMonitorMode *
gf_monitor_get_mode_from_id (GfMonitor   *monitor,
                             const gchar *monitor_mode_id)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  return g_hash_table_lookup (priv->mode_ids, monitor_mode_id);
}

gboolean
gf_monitor_mode_spec_has_similar_size (GfMonitorModeSpec *monitor_mode_spec,
                                       GfMonitorModeSpec *other_monitor_mode_spec)
{
  const float target_ratio = 1.0;
  /* The a size difference of 15% means e.g. 4K modes matches other 4K modes,
   * FHD (2K) modes other FHD modes, and HD modes other HD modes, but not each
   * other.
   */
  const float epsilon = 0.15;

  return G_APPROX_VALUE (((float) monitor_mode_spec->width /
                          other_monitor_mode_spec->width) *
                         ((float) monitor_mode_spec->height /
                          other_monitor_mode_spec->height),
                         target_ratio,
                         epsilon);
}

GfMonitorMode *
gf_monitor_get_mode_from_spec (GfMonitor         *monitor,
                               GfMonitorModeSpec *monitor_mode_spec)
{
  GfMonitorPrivate *priv;
  GList *l;

  priv = gf_monitor_get_instance_private (monitor);

  for (l = priv->modes; l; l = l->next)
    {
      GfMonitorMode *monitor_mode = l->data;

      if (gf_monitor_mode_spec_equals (monitor_mode_spec, &monitor_mode->spec))
        return monitor_mode;
    }

  return NULL;
}

GfMonitorMode *
gf_monitor_get_preferred_mode (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  return priv->preferred_mode;
}

GfMonitorMode *
gf_monitor_get_current_mode (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  return priv->current_mode;
}

void
gf_monitor_derive_current_mode (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;
  GfMonitorMode *current_mode;
  GList *l;

  priv = gf_monitor_get_instance_private (monitor);
  current_mode = NULL;

  for (l = priv->modes; l; l = l->next)
    {
      GfMonitorMode *mode = l->data;

      if (gf_monitor_is_mode_assigned (monitor, mode))
        {
          current_mode = mode;
          break;
        }
    }

  priv->current_mode = current_mode;

  g_warn_if_fail (is_current_mode_known (monitor));
}

void
gf_monitor_set_current_mode (GfMonitor     *monitor,
                             GfMonitorMode *mode)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  priv->current_mode = mode;
}

GList *
gf_monitor_get_modes (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  return priv->modes;
}

void
gf_monitor_calculate_crtc_pos (GfMonitor          *monitor,
                               GfMonitorMode      *monitor_mode,
                               GfOutput           *output,
                               GfMonitorTransform  crtc_transform,
                               int                *out_x,
                               int                *out_y)
{
  GF_MONITOR_GET_CLASS (monitor)->calculate_crtc_pos (monitor, monitor_mode,
                                                      output, crtc_transform,
                                                      out_x, out_y);
}

gfloat
gf_monitor_calculate_mode_scale (GfMonitor                 *monitor,
                                 GfMonitorMode             *monitor_mode,
                                 GfMonitorScalesConstraint  constraints)
{
  GfMonitorPrivate *priv;
  GfSettings *settings;
  gint global_scaling_factor;

  priv = gf_monitor_get_instance_private (monitor);
  settings = gf_backend_get_settings (priv->backend);

  if (gf_settings_get_global_scaling_factor (settings, &global_scaling_factor))
    return global_scaling_factor;

  return calculate_scale (monitor, monitor_mode, constraints);
}

gfloat *
gf_monitor_calculate_supported_scales (GfMonitor                 *monitor,
                                       GfMonitorMode             *monitor_mode,
                                       GfMonitorScalesConstraint  constraints,
                                       int                       *n_supported_scales)
{
  guint i, j;
  gint width, height;
  GArray *supported_scales;

  supported_scales = g_array_new (FALSE, FALSE, sizeof (gfloat));

  gf_monitor_mode_get_resolution (monitor_mode, &width, &height);

  for (i = floorf (MINIMUM_SCALE_FACTOR);
       i <= ceilf (MAXIMUM_SCALE_FACTOR);
       i++)
    {
      if (constraints & GF_MONITOR_SCALES_CONSTRAINT_NO_FRAC)
        {
          if (is_scale_valid_for_size (width, height, i))
            {
              float scale = i;
              g_array_append_val (supported_scales, scale);
            }
        }
      else
        {
          float max_bound;

          if (i == floorf (MINIMUM_SCALE_FACTOR) ||
              i == ceilf (MAXIMUM_SCALE_FACTOR))
            max_bound = SCALE_FACTORS_STEPS;
          else
            max_bound = SCALE_FACTORS_STEPS / 2.0f;

          for (j = 0; j < SCALE_FACTORS_PER_INTEGER; j++)
            {
              gfloat scale;
              gfloat scale_value = i + j * SCALE_FACTORS_STEPS;

              if (!is_scale_valid_for_size (width, height, scale_value))
                continue;

              scale = gf_get_closest_monitor_scale_factor_for_resolution (width,
                                                                          height,
                                                                          scale_value,
                                                                          max_bound);

              if (scale > 0.0f)
                g_array_append_val (supported_scales, scale);
            }
        }
    }

  if (supported_scales->len == 0)
    {
      gfloat fallback_scale;

      fallback_scale = 1.0;

      g_array_append_val (supported_scales, fallback_scale);
    }

  *n_supported_scales = supported_scales->len;
  return (gfloat *) g_array_free (supported_scales, FALSE);
}

const gchar *
gf_monitor_mode_get_id (GfMonitorMode *monitor_mode)
{
  return monitor_mode->id;
}

GfMonitorModeSpec *
gf_monitor_mode_get_spec (GfMonitorMode *monitor_mode)
{
  return &monitor_mode->spec;
}

void
gf_monitor_mode_get_resolution (GfMonitorMode *monitor_mode,
                                gint          *width,
                                gint          *height)
{
  *width = monitor_mode->spec.width;
  *height = monitor_mode->spec.height;
}

gfloat
gf_monitor_mode_get_refresh_rate (GfMonitorMode *monitor_mode)
{
  return monitor_mode->spec.refresh_rate;
}

GfCrtcModeFlag
gf_monitor_mode_get_flags (GfMonitorMode *monitor_mode)
{
  return monitor_mode->spec.flags;
}

gboolean
gf_monitor_mode_foreach_crtc (GfMonitor          *monitor,
                              GfMonitorMode      *mode,
                              GfMonitorModeFunc   func,
                              gpointer            user_data,
                              GError            **error)
{
  GfMonitorPrivate *priv;
  GList *l;
  gint i;

  priv = gf_monitor_get_instance_private (monitor);

  for (l = priv->outputs, i = 0; l; l = l->next, i++)
    {
      GfMonitorCrtcMode *monitor_crtc_mode = &mode->crtc_modes[i];

      if (!monitor_crtc_mode->crtc_mode)
        continue;

      if (!func (monitor, mode, monitor_crtc_mode, user_data, error))
        return FALSE;
    }

  return TRUE;
}

gboolean
gf_monitor_mode_foreach_output (GfMonitor          *monitor,
                                GfMonitorMode      *mode,
                                GfMonitorModeFunc   func,
                                gpointer            user_data,
                                GError            **error)
{
  GfMonitorPrivate *priv;
  GList *l;
  gint i;

  priv = gf_monitor_get_instance_private (monitor);

  for (l = priv->outputs, i = 0; l; l = l->next, i++)
    {
      GfMonitorCrtcMode *monitor_crtc_mode = &mode->crtc_modes[i];

      if (!func (monitor, mode, monitor_crtc_mode, user_data, error))
        return FALSE;
    }

  return TRUE;
}

gboolean
gf_monitor_mode_should_be_advertised (GfMonitorMode *monitor_mode)
{
  GfMonitorMode *preferred_mode;

  g_return_val_if_fail (monitor_mode != NULL, FALSE);

  preferred_mode = gf_monitor_get_preferred_mode (monitor_mode->monitor);
  if (monitor_mode->spec.width == preferred_mode->spec.width &&
      monitor_mode->spec.height == preferred_mode->spec.height)
    return TRUE;

  return is_logical_size_large_enough (monitor_mode->spec.width,
                                       monitor_mode->spec.height);
}

gboolean
gf_verify_monitor_mode_spec (GfMonitorModeSpec  *mode_spec,
                             GError            **error)
{
  if (mode_spec->width > 0 &&
      mode_spec->height > 0 &&
      mode_spec->refresh_rate > 0.0f)
    {
      return TRUE;
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Monitor mode invalid");

      return FALSE;
    }
}

gboolean
gf_monitor_has_aspect_as_size (GfMonitor *monitor)
{
  int width_mm;
  int height_mm;

  gf_monitor_get_physical_dimensions (monitor, &width_mm, &height_mm);

  return (width_mm == 1600 && height_mm == 900) ||
         (width_mm == 1600 && height_mm == 1000) ||
         (width_mm == 160 && height_mm == 90) ||
         (width_mm == 160 && height_mm == 100) ||
         (width_mm == 16 && height_mm == 9) ||
         (width_mm == 16 && height_mm == 10);
}

void
gf_monitor_set_logical_monitor (GfMonitor        *monitor,
                                GfLogicalMonitor *logical_monitor)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  priv->logical_monitor = logical_monitor;
}

gboolean
gf_monitor_get_backlight_info (GfMonitor *self,
                               int       *backlight_min,
                               int       *backlight_max)
{
  GfOutput *main_output;
  int value;

  main_output = gf_monitor_get_main_output (self);
  value = gf_output_get_backlight (main_output);

  if (value >= 0)
    {
      const GfOutputInfo *output_info;

      output_info = gf_output_get_info (main_output);

      if (backlight_min)
        *backlight_min = output_info->backlight_min;

      if (backlight_max)
        *backlight_max = output_info->backlight_max;

      return TRUE;
    }

  return FALSE;
}

void
gf_monitor_set_backlight (GfMonitor *self,
                          int        value)
{
  GfMonitorPrivate *priv;
  GList *l;

  priv = gf_monitor_get_instance_private (self);

  for (l = priv->outputs; l; l = l->next)
    {
      GfOutput *output;

      output = l->data;

      gf_output_set_backlight (output, value);
    }
}

gboolean
gf_monitor_get_backlight (GfMonitor *self,
                          int       *value)
{
  if (gf_monitor_get_backlight_info (self, NULL, NULL))
    {
      GfOutput *main_output;

      main_output = gf_monitor_get_main_output (self);

      *value = gf_output_get_backlight (main_output);

      return TRUE;
    }

  return FALSE;
}
