/*
 * Copyright (C) 2016 Red Hat
 * Copyright (C) 2017 Alberts Muktupāvels
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

#include <math.h>
#include <string.h>

#include "gf-crtc-private.h"
#include "gf-monitor-manager-private.h"
#include "gf-monitor-private.h"
#include "gf-monitor-spec-private.h"
#include "gf-output-private.h"
#include "gf-settings-private.h"

#define SCALE_FACTORS_PER_INTEGER 4
#define MINIMUM_SCALE_FACTOR 1.0f
#define MAXIMUM_SCALE_FACTOR 4.0f
#define MINIMUM_LOGICAL_WIDTH 800
#define MINIMUM_LOGICAL_HEIGHT 600
#define MAXIMUM_REFRESH_RATE_DIFF 0.001

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
  GfMonitorManager *monitor_manager;

  GList            *outputs;
  GList            *modes;
  GHashTable       *mode_ids;

  GfMonitorMode    *preferred_mode;
  GfMonitorMode    *current_mode;

  GfMonitorSpec    *spec;

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
} GfMonitorPrivate;

enum
{
  PROP_0,

  PROP_MONITOR_MANAGER,

  LAST_PROP
};

static GParamSpec *monitor_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE_WITH_PRIVATE (GfMonitor, gf_monitor, G_TYPE_OBJECT)

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
calculate_scale (GfMonitor     *monitor,
                 GfMonitorMode *monitor_mode)
{
  gint resolution_width, resolution_height;
  gint width_mm, height_mm;
  gint scale;

  scale = 1.0;

  gf_monitor_mode_get_resolution (monitor_mode,
                                  &resolution_width,
                                  &resolution_height);

  if (resolution_height < HIDPI_MIN_HEIGHT)
    goto out;

  /* 4K TV */
  switch (gf_monitor_get_connector_type (monitor))
    {
      case GF_CONNECTOR_TYPE_HDMIA:
      case GF_CONNECTOR_TYPE_HDMIB:
        if (resolution_width < SMALLEST_4K_WIDTH)
          goto out;
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
      default:
        break;
    }

  gf_monitor_get_physical_dimensions (monitor, &width_mm, &height_mm);

  /* Somebody encoded the aspect ratio (16/9 or 16/10) instead of the
   * physical size.
   */
  if ((width_mm == 160 && height_mm == 90) ||
      (width_mm == 160 && height_mm == 100) ||
      (width_mm == 16 && height_mm == 9) ||
      (width_mm == 16 && height_mm == 10))
    goto out;

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

out:
  return scale;
}

static gfloat
get_closest_scale_factor_for_resolution (gfloat width,
                                         gfloat height,
                                         gfloat scale,
                                         gfloat scale_step)
{
  guint i, j;
  gfloat scaled_h;
  gfloat scaled_w;
  gfloat best_scale;
  gint base_scaled_w;
  gboolean limit_exceeded;
  gboolean found_one;

  best_scale = 0;
  scaled_w = width / scale;
  scaled_h = height / scale;

  if (scale < MINIMUM_SCALE_FACTOR ||
      scale > MAXIMUM_SCALE_FACTOR ||
      floorf (scaled_w) < MINIMUM_LOGICAL_WIDTH ||
      floorf (scaled_h) < MINIMUM_LOGICAL_HEIGHT)
    goto out;

  if (floorf (scaled_w) == scaled_w && floorf (scaled_h) == scaled_h)
    return scale;

  i = 0;
  found_one = FALSE;
  limit_exceeded = FALSE;
  base_scaled_w = floorf (scaled_w);

  do
    {

      for (j = 0; j < 2; j++)
        {
          gfloat current_scale;
          gint offset = i * (j ? 1 : -1);

          scaled_w = base_scaled_w + offset;
          current_scale = width / scaled_w;
          scaled_h = height / current_scale;

          if (current_scale >= scale + scale_step ||
              current_scale <= scale - scale_step ||
              current_scale < MINIMUM_SCALE_FACTOR ||
              current_scale > MAXIMUM_SCALE_FACTOR)
            {
              limit_exceeded = TRUE;
              continue;
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
  while (!found_one && !limit_exceeded);

out:
  return best_scale;
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
  g_clear_pointer (&priv->outputs, g_list_free);
  gf_monitor_spec_free (priv->spec);

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
      case PROP_MONITOR_MANAGER:
        g_value_set_object (value, priv->monitor_manager);
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
      case PROP_MONITOR_MANAGER:
        priv->monitor_manager = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_monitor_install_properties (GObjectClass *object_class)
{
  monitor_properties[PROP_MONITOR_MANAGER] =
    g_param_spec_object ("monitor-manager",
                         "GfMonitorManager",
                         "GfMonitorManager",
                         GF_TYPE_MONITOR_MANAGER,
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

GfMonitorManager *
gf_monitor_get_monitor_manager (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  return priv->monitor_manager;
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
      GfOutput *output = l->data;
      GfMonitorCrtcMode *monitor_crtc_mode = &mode->crtc_modes[i];

      if (monitor_crtc_mode->crtc_mode &&
          (!output->crtc ||
           output->crtc->current_mode != monitor_crtc_mode->crtc_mode))
        return FALSE;
      else if (!monitor_crtc_mode->crtc_mode && output->crtc)
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

  priv->outputs = g_list_append (priv->outputs, output);
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

void
gf_monitor_generate_spec (GfMonitor *monitor)
{
  GfMonitorPrivate *priv;
  GfOutput *output;
  GfMonitorSpec *monitor_spec;

  priv = gf_monitor_get_instance_private (monitor);
  output = gf_monitor_get_main_output (monitor);

  monitor_spec = g_new0 (GfMonitorSpec, 1);

  monitor_spec->connector = g_strdup (output->name);
  monitor_spec->vendor = g_strdup (output->vendor);
  monitor_spec->product = g_strdup (output->product);
  monitor_spec->serial = g_strdup (output->serial);

  priv->spec = monitor_spec;
}

gboolean
gf_monitor_add_mode (GfMonitor     *monitor,
                     GfMonitorMode *monitor_mode)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  if (g_hash_table_lookup (priv->mode_ids,
                           gf_monitor_mode_get_id (monitor_mode)))
    return FALSE;

  priv->modes = g_list_append (priv->modes, monitor_mode);
  g_hash_table_insert (priv->mode_ids, monitor_mode->id, monitor_mode);

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
  gchar refresh_rate[G_ASCII_DTOSTR_BUF_SIZE];

  is_interlaced = !!(spec->flags & GF_CRTC_MODE_FLAG_INTERLACE);
  g_ascii_dtostr (refresh_rate, G_ASCII_DTOSTR_BUF_SIZE, spec->refresh_rate);

  return g_strdup_printf ("%dx%d%s@%s", spec->width, spec->height,
                          is_interlaced ? "i" : "", refresh_rate);
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
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);

  return output->crtc && output->crtc->current_mode;
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

  return output->is_primary;
}

gboolean
gf_monitor_supports_underscanning (GfMonitor *monitor)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);

  return output->supports_underscanning;
}

gboolean
gf_monitor_is_underscanning (GfMonitor *monitor)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);

  return output->is_underscanning;
}

gboolean
gf_monitor_is_laptop_panel (GfMonitor *monitor)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);

  switch (output->connector_type)
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
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);

  *width_mm = output->width_mm;
  *height_mm = output->height_mm;
}

const gchar *
gf_monitor_get_connector (GfMonitor *monitor)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);
  return output->name;
}

const gchar *
gf_monitor_get_vendor (GfMonitor *monitor)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);
  return output->vendor;
}

const gchar *
gf_monitor_get_product (GfMonitor *monitor)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);
  return output->product;
}

const gchar *
gf_monitor_get_serial (GfMonitor *monitor)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);
  return output->serial;
}

GfConnectorType
gf_monitor_get_connector_type (GfMonitor *monitor)
{
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);
  return output->connector_type;
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
  GfOutput *output = gf_monitor_get_main_output (monitor);

  if (output->crtc)
    return output->crtc->logical_monitor;
  else
    return NULL;
}

GfMonitorMode *
gf_monitor_get_mode_from_id (GfMonitor   *monitor,
                             const gchar *monitor_mode_id)
{
  GfMonitorPrivate *priv;

  priv = gf_monitor_get_instance_private (monitor);

  return g_hash_table_lookup (priv->mode_ids, monitor_mode_id);
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
gf_monitor_calculate_mode_scale (GfMonitor     *monitor,
                                 GfMonitorMode *monitor_mode)
{
  GfMonitorPrivate *priv;
  GfBackend *backend;
  GfSettings *settings;
  gint global_scaling_factor;

  priv = gf_monitor_get_instance_private (monitor);
  backend = gf_monitor_manager_get_backend (priv->monitor_manager);
  settings = gf_backend_get_settings (backend);

  if (gf_settings_get_global_scaling_factor (settings, &global_scaling_factor))
    return global_scaling_factor;

  return calculate_scale (monitor, monitor_mode);
}

gfloat *
gf_monitor_calculate_supported_scales (GfMonitor                 *monitor,
                                       GfMonitorMode             *monitor_mode,
                                       GfMonitorScalesConstraint  constraints,
                                       int                       *n_supported_scales)
{
  guint i, j;
  gint width, height;
  gfloat scale_steps;
  GArray *supported_scales;

  scale_steps = 1.0 / (gfloat) SCALE_FACTORS_PER_INTEGER;
  supported_scales = g_array_new (FALSE, FALSE, sizeof (float));

  gf_monitor_mode_get_resolution (monitor_mode, &width, &height);

  for (i = floorf (MINIMUM_SCALE_FACTOR);
       i <= ceilf (MAXIMUM_SCALE_FACTOR);
       i++)
    {
      for (j = 0; j < SCALE_FACTORS_PER_INTEGER; j++)
        {
          gfloat scale;
          gfloat scale_value = i + j * scale_steps;

          if ((constraints & GF_MONITOR_SCALES_CONSTRAINT_NO_FRAC) &&
              fmodf (scale_value, 1.0) != 0.0)
            {
              continue;
            }

          scale = get_closest_scale_factor_for_resolution (width,
                                                           height,
                                                           scale_value,
                                                           scale_steps);

          if (scale > 0.0f)
            g_array_append_val (supported_scales, scale);
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
