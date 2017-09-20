/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
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
 * - src/backends/x11/meta-monitor-manager-xrandr.c
 */

#include "config.h"

#include <string.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/dpms.h>

#include "gf-backend-x11-private.h"
#include "gf-crtc-private.h"
#include "gf-monitor-manager-xrandr-private.h"
#include "gf-monitor-private.h"
#include "gf-monitor-tiled-private.h"
#include "gf-output-private.h"

struct _GfMonitorManagerXrandr
{
  GfMonitorManager    parent;

  Display            *xdisplay;
  Window              xroot;

  gint                rr_event_base;
  gint                rr_error_base;

  gboolean            has_randr15;
  GHashTable         *tiled_monitor_atoms;

  XRRScreenResources *resources;

  gint                max_screen_width;
  gint                max_screen_height;
};

typedef struct
{
  Atom xrandr_name;
} GfMonitorData;

G_DEFINE_TYPE (GfMonitorManagerXrandr, gf_monitor_manager_xrandr, GF_TYPE_MONITOR_MANAGER)

static guint8 *
get_edid_property (Display  *xdisplay,
                   RROutput  output,
                   Atom      atom,
                   gsize    *len)
{
  guchar *prop;
  gint actual_format;
  gulong nitems, bytes_after;
  Atom actual_type;
  guint8 *result;

  XRRGetOutputProperty (xdisplay, output, atom,
                        0, 100, False, False,
                        AnyPropertyType,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &prop);

  if (actual_type == XA_INTEGER && actual_format == 8)
    {
      result = g_memdup (prop, nitems);
      if (len)
        *len = nitems;
    }
  else
    {
      result = NULL;
    }

  if (prop)
    XFree (prop);

  return result;
}

static GBytes *
read_output_edid (GfMonitorManagerXrandr *xrandr,
                  XID                     winsys_id)
{
  Atom edid_atom;
  guint8 *result;
  gsize len;

  edid_atom = XInternAtom (xrandr->xdisplay, "EDID", FALSE);
  result = get_edid_property (xrandr->xdisplay, winsys_id, edid_atom, &len);

  if (!result)
    {
      edid_atom = XInternAtom (xrandr->xdisplay, "EDID_DATA", FALSE);
      result = get_edid_property (xrandr->xdisplay, winsys_id, edid_atom, &len);
    }

  if (result)
    {
      if (len > 0 && len % 128 == 0)
        return g_bytes_new_take (result, len);
      else
        g_free (result);
    }

  return NULL;
}

static GQuark
gf_monitor_data_quark (void)
{
  static GQuark quark;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("gf-monitor-data-quark");

  return quark;
}

static GfMonitorData *
data_from_monitor (GfMonitor *monitor)
{
  GfMonitorData *data;
  GQuark quark;

  quark = gf_monitor_data_quark ();
  data = g_object_get_qdata (G_OBJECT (monitor), quark);

  if (data)
    return data;

  data = g_new0 (GfMonitorData, 1);
  g_object_set_qdata_full (G_OBJECT (monitor), quark, data, g_free);

  return data;
}

static void
increase_monitor_count (GfMonitorManagerXrandr *xrandr,
                        Atom                    name_atom)
{
  GHashTable *atoms;
  gpointer key;
  gint count;

  atoms = xrandr->tiled_monitor_atoms;
  key = GSIZE_TO_POINTER (name_atom);

  count = GPOINTER_TO_INT (g_hash_table_lookup (atoms, key));
  count++;

  g_hash_table_insert (atoms, key, GINT_TO_POINTER (count));
}

static gint
decrease_monitor_count (GfMonitorManagerXrandr *xrandr,
                        Atom                    name_atom)
{
  GHashTable *atoms;
  gpointer key;
  gint count;

  atoms = xrandr->tiled_monitor_atoms;
  key = GSIZE_TO_POINTER (name_atom);

  count = GPOINTER_TO_SIZE (g_hash_table_lookup (atoms, key));
  count--;

  g_hash_table_insert (atoms, key, GINT_TO_POINTER (count));

  return count;
}

static void
init_monitors (GfMonitorManagerXrandr *xrandr)
{
  XRRMonitorInfo *m;
  gint n, i;

  if (xrandr->has_randr15 == FALSE)
    return;

  /* Delete any tiled monitors setup, as gnome-fashback will want to
   * recreate things in its image.
   */
  m = XRRGetMonitors (xrandr->xdisplay, xrandr->xroot, FALSE, &n);

  if (n == -1)
    return;

  for (i = 0; i < n; i++)
    {
      if (m[i].noutput > 1)
        {
          XRRDeleteMonitor (xrandr->xdisplay, xrandr->xroot, m[i].name);
        }
    }

  XRRFreeMonitors (m);
}

static void
gf_monitor_manager_xrandr_constructed (GObject *object)
{
  GfMonitorManagerXrandr *xrandr;
  GfBackend *backend;
  gint rr_event_base;
  gint rr_error_base;

  xrandr = GF_MONITOR_MANAGER_XRANDR (object);
  backend = gf_monitor_manager_get_backend (GF_MONITOR_MANAGER (xrandr));

  xrandr->xdisplay = gf_backend_x11_get_xdisplay (GF_BACKEND_X11 (backend));
  xrandr->xroot = DefaultRootWindow (xrandr->xdisplay);

  if (XRRQueryExtension (xrandr->xdisplay, &rr_event_base, &rr_error_base))
    {
      gint major_version;
      gint minor_version;

      xrandr->rr_event_base = rr_event_base;
      xrandr->rr_error_base = rr_error_base;

      /* We only use ScreenChangeNotify, but GDK uses the others,
       * and we don't want to step on its toes.
       */
      XRRSelectInput (xrandr->xdisplay, xrandr->xroot,
                      RRScreenChangeNotifyMask |
                      RRCrtcChangeNotifyMask |
                      RROutputPropertyNotifyMask);

      XRRQueryVersion (xrandr->xdisplay, &major_version, &minor_version);

      xrandr->has_randr15 = FALSE;
      if (major_version > 1 || (major_version == 1 && minor_version >= 5))
        {
          xrandr->has_randr15 = TRUE;
          xrandr->tiled_monitor_atoms = g_hash_table_new (NULL, NULL);
        }

      init_monitors (xrandr);
    }

  G_OBJECT_CLASS (gf_monitor_manager_xrandr_parent_class)->constructed (object);
}

static void
gf_monitor_manager_xrandr_dispose (GObject *object)
{
  GfMonitorManagerXrandr *xrandr;

  xrandr = GF_MONITOR_MANAGER_XRANDR (object);

  g_clear_pointer (&xrandr->tiled_monitor_atoms, g_hash_table_destroy);

  G_OBJECT_CLASS (gf_monitor_manager_xrandr_parent_class)->dispose (object);
}

static void
gf_monitor_manager_xrandr_finalize (GObject *object)
{
  GfMonitorManagerXrandr *xrandr;

  xrandr = GF_MONITOR_MANAGER_XRANDR (object);

  g_clear_pointer (&xrandr->resources, XRRFreeScreenResources);

  G_OBJECT_CLASS (gf_monitor_manager_xrandr_parent_class)->finalize (object);
}

static void
gf_monitor_manager_xrandr_read_current (GfMonitorManager *manager)
{
}

static GBytes *
gf_monitor_manager_xrandr_read_edid (GfMonitorManager *manager,
                                     GfOutput         *output)
{
  GfMonitorManagerXrandr *xrandr;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  return read_output_edid (xrandr, output->winsys_id);
}

static void
gf_monitor_manager_xrandr_ensure_initial_config (GfMonitorManager *manager)
{
}

static gboolean
gf_monitor_manager_xrandr_apply_monitors_config (GfMonitorManager        *manager,
                                                 GfMonitorsConfig        *config,
                                                 GfMonitorsConfigMethod   method,
                                                 GError                 **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Not implemented");
  return FALSE;
}

static void
gf_monitor_manager_xrandr_set_power_save_mode (GfMonitorManager *manager,
                                               GfPowerSave       mode)
{
  GfMonitorManagerXrandr *xrandr;
  CARD16 state;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  switch (mode)
    {
      case GF_POWER_SAVE_ON:
        state = DPMSModeOn;
        break;

      case GF_POWER_SAVE_STANDBY:
        state = DPMSModeStandby;
        break;

      case GF_POWER_SAVE_SUSPEND:
        state = DPMSModeSuspend;
        break;

      case GF_POWER_SAVE_OFF:
        state = DPMSModeOff;
        break;

      case GF_POWER_SAVE_UNSUPPORTED:
      default:
        return;
    }

  DPMSForceLevel (xrandr->xdisplay, state);
  DPMSSetTimeouts (xrandr->xdisplay, 0, 0, 0);
}

static void
gf_monitor_manager_xrandr_change_backlight (GfMonitorManager *manager,
                                            GfOutput         *output,
                                            gint              value)
{
}

static void
gf_monitor_manager_xrandr_get_crtc_gamma (GfMonitorManager  *manager,
                                          GfCrtc            *crtc,
                                          gsize             *size,
                                          gushort          **red,
                                          gushort          **green,
                                          gushort          **blue)
{
  GfMonitorManagerXrandr *xrandr;
  XRRCrtcGamma *gamma;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);
  gamma = XRRGetCrtcGamma (xrandr->xdisplay, (XID) crtc->crtc_id);

  *size = gamma->size;
  *red = g_memdup (gamma->red, sizeof (gushort) * gamma->size);
  *green = g_memdup (gamma->green, sizeof (gushort) * gamma->size);
  *blue = g_memdup (gamma->blue, sizeof (gushort) * gamma->size);

  XRRFreeGamma (gamma);
}

static void
gf_monitor_manager_xrandr_set_crtc_gamma (GfMonitorManager *manager,
                                          GfCrtc           *crtc,
                                          gsize             size,
                                          gushort          *red,
                                          gushort          *green,
                                          gushort          *blue)
{
  GfMonitorManagerXrandr *xrandr;
  XRRCrtcGamma *gamma;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  gamma = XRRAllocGamma (size);
  memcpy (gamma->red, red, sizeof (gushort) * size);
  memcpy (gamma->green, green, sizeof (gushort) * size);
  memcpy (gamma->blue, blue, sizeof (gushort) * size);

  XRRSetCrtcGamma (xrandr->xdisplay, (XID) crtc->crtc_id, gamma);
  XRRFreeGamma (gamma);
}

static void
gf_monitor_manager_xrandr_tiled_monitor_added (GfMonitorManager *manager,
                                               GfMonitor        *monitor)
{
  GfMonitorManagerXrandr *xrandr;
  GfMonitorTiled *monitor_tiled;
  const gchar *product;
  uint32_t tile_group_id;
  gchar *name;
  Atom name_atom;
  GfMonitorData *data;
  GList *outputs;
  XRRMonitorInfo *monitor_info;
  GList *l;
  gint i;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  if (xrandr->has_randr15 == FALSE)
    return;

  monitor_tiled = GF_MONITOR_TILED (monitor);
  product = gf_monitor_get_product (monitor);
  tile_group_id = gf_monitor_tiled_get_tile_group_id (monitor_tiled);

  if (product)
    name = g_strdup_printf ("%s-%d", product, tile_group_id);
  else
    name = g_strdup_printf ("Tiled-%d", tile_group_id);

  name_atom = XInternAtom (xrandr->xdisplay, name, False);
  g_free (name);

  data = data_from_monitor (monitor);
  data->xrandr_name = name_atom;

  increase_monitor_count (xrandr, name_atom);

  outputs = gf_monitor_get_outputs (monitor);
  monitor_info = XRRAllocateMonitor (xrandr->xdisplay, g_list_length (outputs));

  monitor_info->name = name_atom;
  monitor_info->primary = gf_monitor_is_primary (monitor);
  monitor_info->automatic = True;

  for (l = outputs, i = 0; l; l = l->next, i++)
    {
      GfOutput *output = l->data;

      monitor_info->outputs[i] = output->winsys_id;
    }

  XRRSetMonitor (xrandr->xdisplay, xrandr->xroot, monitor_info);
  XRRFreeMonitors (monitor_info);
}

static void
gf_monitor_manager_xrandr_tiled_monitor_removed (GfMonitorManager *manager,
                                                 GfMonitor        *monitor)
{
  GfMonitorManagerXrandr *xrandr;
  GfMonitorData *data;
  gint monitor_count;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  if (xrandr->has_randr15 == FALSE)
    return;

  data = data_from_monitor (monitor);
  monitor_count = decrease_monitor_count (xrandr, data->xrandr_name);

  if (monitor_count == 0)
    {
      XRRDeleteMonitor (xrandr->xdisplay, xrandr->xroot, data->xrandr_name);
    }
}

static gboolean
gf_monitor_manager_xrandr_is_transform_handled (GfMonitorManager   *manager,
                                                GfCrtc             *crtc,
                                                GfMonitorTransform  transform)
{
  g_warn_if_fail ((crtc->all_transforms & transform) == transform);

  return TRUE;
}

static gfloat
gf_monitor_manager_xrandr_calculate_monitor_mode_scale (GfMonitorManager *manager,
                                                        GfMonitor        *monitor,
                                                        GfMonitorMode    *monitor_mode)
{
  return gf_monitor_calculate_mode_scale (monitor, monitor_mode);
}

static gfloat *
gf_monitor_manager_xrandr_calculate_supported_scales (GfMonitorManager           *manager,
                                                      GfLogicalMonitorLayoutMode  layout_mode,
                                                      GfMonitor                  *monitor,
                                                      GfMonitorMode              *monitor_mode,
                                                      gint                       *n_supported_scales)
{
  GfMonitorScalesConstraint constraints;

  constraints = GF_MONITOR_SCALES_CONSTRAINT_NO_FRAC;

  return gf_monitor_calculate_supported_scales (monitor, monitor_mode,
                                                constraints,
                                                n_supported_scales);
}

static GfMonitorManagerCapability
gf_monitor_manager_xrandr_get_capabilities (GfMonitorManager *manager)
{
  GfMonitorManagerCapability capabilities;

  capabilities = GF_MONITOR_MANAGER_CAPABILITY_NONE;

  capabilities |= GF_MONITOR_MANAGER_CAPABILITY_MIRRORING;
  capabilities |= GF_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED;

  return capabilities;
}

static gboolean
gf_monitor_manager_xrandr_get_max_screen_size (GfMonitorManager *manager,
                                               gint             *max_width,
                                               gint             *max_height)
{
  GfMonitorManagerXrandr *xrandr;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  *max_width = xrandr->max_screen_width;
  *max_height = xrandr->max_screen_height;

  return TRUE;
}

static GfLogicalMonitorLayoutMode
gf_monitor_manager_xrandr_get_default_layout_mode (GfMonitorManager *manager)
{
  return GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;
}

static void
gf_monitor_manager_xrandr_class_init (GfMonitorManagerXrandrClass *xrandr_class)
{
  GObjectClass *object_class;
  GfMonitorManagerClass *manager_class;

  object_class = G_OBJECT_CLASS (xrandr_class);
  manager_class = GF_MONITOR_MANAGER_CLASS (xrandr_class);

  object_class->constructed = gf_monitor_manager_xrandr_constructed;
  object_class->dispose = gf_monitor_manager_xrandr_dispose;
  object_class->finalize = gf_monitor_manager_xrandr_finalize;

  manager_class->read_current = gf_monitor_manager_xrandr_read_current;
  manager_class->read_edid = gf_monitor_manager_xrandr_read_edid;
  manager_class->ensure_initial_config = gf_monitor_manager_xrandr_ensure_initial_config;
  manager_class->apply_monitors_config = gf_monitor_manager_xrandr_apply_monitors_config;
  manager_class->set_power_save_mode = gf_monitor_manager_xrandr_set_power_save_mode;
  manager_class->change_backlight = gf_monitor_manager_xrandr_change_backlight;
  manager_class->get_crtc_gamma = gf_monitor_manager_xrandr_get_crtc_gamma;
  manager_class->set_crtc_gamma = gf_monitor_manager_xrandr_set_crtc_gamma;
  manager_class->tiled_monitor_added = gf_monitor_manager_xrandr_tiled_monitor_added;
  manager_class->tiled_monitor_removed = gf_monitor_manager_xrandr_tiled_monitor_removed;
  manager_class->is_transform_handled = gf_monitor_manager_xrandr_is_transform_handled;
  manager_class->calculate_monitor_mode_scale = gf_monitor_manager_xrandr_calculate_monitor_mode_scale;
  manager_class->calculate_supported_scales = gf_monitor_manager_xrandr_calculate_supported_scales;
  manager_class->get_capabilities = gf_monitor_manager_xrandr_get_capabilities;
  manager_class->get_max_screen_size = gf_monitor_manager_xrandr_get_max_screen_size;
  manager_class->get_default_layout_mode = gf_monitor_manager_xrandr_get_default_layout_mode;
}

static void
gf_monitor_manager_xrandr_init (GfMonitorManagerXrandr *xrandr)
{
}

gboolean
gf_monitor_manager_xrandr_handle_xevent (GfMonitorManagerXrandr *xrandr,
                                         XEvent                 *event)
{
  return FALSE;
}
