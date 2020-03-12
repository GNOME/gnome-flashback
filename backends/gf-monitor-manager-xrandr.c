/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
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
 * - src/backends/x11/meta-monitor-manager-xrandr.c
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlibint.h>
#include <X11/extensions/dpms.h>
#include <X11/Xlib-xcb.h>
#include <xcb/randr.h>

#include "gf-backend-x11-private.h"
#include "gf-crtc-xrandr-private.h"
#include "gf-gpu-xrandr-private.h"
#include "gf-monitor-config-manager-private.h"
#include "gf-monitor-manager-xrandr-private.h"
#include "gf-monitor-private.h"
#include "gf-monitor-tiled-private.h"
#include "gf-output-xrandr-private.h"

/* Look for DPI_FALLBACK in:
 * http://git.gnome.org/browse/gnome-settings-daemon/tree/plugins/xsettings/gsd-xsettings-manager.c
 * for the reasoning
 */
#define DPI_FALLBACK 96.0

struct _GfMonitorManagerXrandr
{
  GfMonitorManager  parent;

  Display          *xdisplay;
  Window            xroot;

  gint              rr_event_base;
  gint              rr_error_base;

  gboolean          has_randr15;
  GHashTable       *tiled_monitor_atoms;

  /*
   * The X server deals with multiple GPUs for us, soe just see what the X
   * server gives us as one single GPU, even though it may actually be backed
   * by multiple.
   */
  GfGpu            *gpu;

  Time              last_xrandr_set_timestamp;

  gfloat           *supported_scales;
  gint              n_supported_scales;
};

typedef struct
{
  Atom xrandr_name;
} GfMonitorData;

G_DEFINE_TYPE (GfMonitorManagerXrandr, gf_monitor_manager_xrandr, GF_TYPE_MONITOR_MANAGER)

static void
add_supported_scale (GArray *supported_scales,
                     gfloat  scale)
{
  guint i;

  for (i = 0; i < supported_scales->len; i++)
    {
      gfloat supported_scale;

      supported_scale = g_array_index (supported_scales, gfloat, i);

      if (scale == supported_scale)
        return;
    }

  g_array_append_val (supported_scales, scale);
}

static gint
compare_scales (gconstpointer a,
                gconstpointer b)
{
  gfloat f = *(gfloat *) a - *(gfloat *) b;

  if (f < 0)
    return -1;

  if (f > 0)
    return 1;

  return 0;
}

static void
ensure_supported_monitor_scales (GfMonitorManager *manager)
{
  GfMonitorManagerXrandr *xrandr;
  GfMonitorScalesConstraint constraints;
  GArray *supported_scales;
  GList *l;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  if (xrandr->supported_scales)
    return;

  constraints = GF_MONITOR_SCALES_CONSTRAINT_NO_FRAC;
  supported_scales = g_array_new (FALSE, FALSE, sizeof (gfloat));

  for (l = manager->monitors; l; l = l->next)
    {
      GfMonitor *monitor;
      GfMonitorMode *monitor_mode;
      gfloat *monitor_scales;
      gint n_monitor_scales;
      gint i;

      monitor = l->data;
      monitor_mode = gf_monitor_get_preferred_mode (monitor);
      monitor_scales = gf_monitor_calculate_supported_scales (monitor,
                                                              monitor_mode,
                                                              constraints,
                                                              &n_monitor_scales);

      for (i = 0; i < n_monitor_scales; i++)
        {
          add_supported_scale (supported_scales, monitor_scales[i]);
        }

      g_array_sort (supported_scales, compare_scales);
      g_free (monitor_scales);
    }

  xrandr->supported_scales = (gfloat *) supported_scales->data;
  xrandr->n_supported_scales = supported_scales->len;
  g_array_free (supported_scales, FALSE);
}

static void
gf_monitor_manager_xrandr_rebuild_derived (GfMonitorManager *manager,
                                           GfMonitorsConfig *config)
{
  GfMonitorManagerXrandr *xrandr;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  g_clear_pointer (&xrandr->supported_scales, g_free);
  gf_monitor_manager_rebuild_derived (manager, config);
}

static gboolean
xrandr_set_crtc_config (GfMonitorManagerXrandr *xrandr,
                        GfCrtc                 *crtc,
                        gboolean                save_timestamp,
                        xcb_randr_crtc_t        xrandr_crtc,
                        xcb_timestamp_t         timestamp,
                        gint                    x,
                        gint                    y,
                        xcb_randr_mode_t        mode,
                        xcb_randr_rotation_t    rotation,
                        xcb_randr_output_t     *outputs,
                        gint                    n_outputs)
{
  xcb_timestamp_t new_timestamp;

  if (!gf_crtc_xrandr_set_config (crtc, xrandr_crtc, timestamp,
                                  x, y, mode, rotation,
                                  outputs, n_outputs,
                                  &new_timestamp))
    return FALSE;

  if (save_timestamp)
    xrandr->last_xrandr_set_timestamp = new_timestamp;

  return TRUE;
}

static gboolean
is_crtc_assignment_changed (GfCrtc      *crtc,
                            GfCrtcInfo **crtc_infos,
                            guint        n_crtc_infos)
{
  guint i;

  for (i = 0; i < n_crtc_infos; i++)
    {
      GfCrtcInfo *crtc_info = crtc_infos[i];

      if (crtc_info->crtc != crtc)
        continue;

      return gf_crtc_xrandr_is_assignment_changed (crtc, crtc_info);
    }

  return !!gf_crtc_xrandr_get_current_mode (crtc);
}

static gboolean
is_output_assignment_changed (GfOutput      *output,
                              GfCrtcInfo   **crtc_infos,
                              guint          n_crtc_infos,
                              GfOutputInfo **output_infos,
                              guint          n_output_infos)
{
  gboolean output_is_found;
  GfCrtc *assigned_crtc;
  guint i;

  output_is_found = FALSE;

  for (i = 0; i < n_output_infos; i++)
    {
      GfOutputInfo *output_info = output_infos[i];

      if (output_info->output != output)
        continue;

      if (output->is_primary != output_info->is_primary)
        return TRUE;

      if (output->is_presentation != output_info->is_presentation)
        return TRUE;

      if (output->is_underscanning != output_info->is_underscanning)
        return TRUE;

      output_is_found = TRUE;
    }

  assigned_crtc = gf_output_get_assigned_crtc (output);

  if (!output_is_found)
    return assigned_crtc != NULL;

  for (i = 0; i < n_crtc_infos; i++)
    {
      GfCrtcInfo *crtc_info = crtc_infos[i];
      guint j;

      for (j = 0; j < crtc_info->outputs->len; j++)
        {
          GfOutput *crtc_info_output;

          crtc_info_output = ((GfOutput**) crtc_info->outputs->pdata)[j];

          if (crtc_info_output == output &&
              crtc_info->crtc == assigned_crtc)
            return FALSE;
        }
    }

  return TRUE;
}

static gboolean
is_assignments_changed (GfMonitorManager  *manager,
                        GfCrtcInfo       **crtc_infos,
                        guint              n_crtc_infos,
                        GfOutputInfo     **output_infos,
                        guint              n_output_infos)
{
  GfMonitorManagerXrandr *manager_xrandr;
  GList *l;

  manager_xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  for (l = gf_gpu_get_crtcs (manager_xrandr->gpu); l; l = l->next)
    {
      GfCrtc *crtc = l->data;

      if (is_crtc_assignment_changed (crtc, crtc_infos, n_crtc_infos))
        return TRUE;
    }

  for (l = gf_gpu_get_outputs (manager_xrandr->gpu); l; l = l->next)
    {
      GfOutput *output = l->data;

      if (is_output_assignment_changed (output,
                                        crtc_infos,
                                        n_crtc_infos,
                                        output_infos,
                                        n_output_infos))
        return TRUE;
    }

  return FALSE;
}

static xcb_randr_rotation_t
gf_monitor_transform_to_xrandr (GfMonitorTransform transform)
{
  xcb_randr_rotation_t rotation;

  rotation = XCB_RANDR_ROTATION_ROTATE_0;

  switch (transform)
    {
      case GF_MONITOR_TRANSFORM_NORMAL:
        rotation = XCB_RANDR_ROTATION_ROTATE_0;
        break;

      case GF_MONITOR_TRANSFORM_90:
        rotation = XCB_RANDR_ROTATION_ROTATE_90;
        break;

      case GF_MONITOR_TRANSFORM_180:
        rotation = XCB_RANDR_ROTATION_ROTATE_180;
        break;

      case GF_MONITOR_TRANSFORM_270:
        rotation = XCB_RANDR_ROTATION_ROTATE_270;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED:
        rotation = XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_ROTATE_0;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED_90:
        rotation = XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_ROTATE_90;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED_180:
        rotation = XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_ROTATE_180;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED_270:
        rotation = XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_ROTATE_270;
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  return rotation;
}

static void
apply_crtc_assignments (GfMonitorManager  *manager,
                        gboolean           save_timestamp,
                        GfCrtcInfo       **crtcs,
                        guint              n_crtcs,
                        GfOutputInfo     **outputs,
                        guint              n_outputs)
{
  GfMonitorManagerXrandr *xrandr;
  gint width, height, width_mm, height_mm;
  guint i;
  GList *l;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  XGrabServer (xrandr->xdisplay);

  /* First compute the new size of the screen (framebuffer) */
  width = 0; height = 0;
  for (i = 0; i < n_crtcs; i++)
    {
      GfCrtcInfo *crtc_info = crtcs[i];
      GfCrtc *crtc = crtc_info->crtc;

      crtc->is_dirty = TRUE;

      if (crtc_info->mode == NULL)
        continue;

      width = MAX (width, crtc_info->layout.x + crtc_info->layout.width);
      height = MAX (height, crtc_info->layout.y + crtc_info->layout.height);
    }

  /* Second disable all newly disabled CRTCs, or CRTCs that in the previous
   * configuration would be outside the new framebuffer (otherwise X complains
   * loudly when resizing)
   * CRTC will be enabled again after resizing the FB
   */
  for (i = 0; i < n_crtcs; i++)
    {
      GfCrtcInfo *crtc_info = crtcs[i];
      GfCrtc *crtc = crtc_info->crtc;
      GfCrtcConfig *crtc_config;
      int x2, y2;

      crtc_config = crtc->config;
      if (crtc_config == NULL)
        continue;

      x2 = crtc_config->layout.x + crtc_config->layout.width;
      y2 = crtc_config->layout.y + crtc_config->layout.height;

      if (crtc_info->mode == NULL || x2 > width || y2 > height)
        {
          xrandr_set_crtc_config (xrandr,
                                  crtc,
                                  save_timestamp,
                                  (xcb_randr_crtc_t) crtc->crtc_id,
                                  XCB_CURRENT_TIME,
                                  0, 0, XCB_NONE,
                                  XCB_RANDR_ROTATION_ROTATE_0,
                                  NULL, 0);

          gf_crtc_unset_config (crtc);
        }
    }

  /* Disable CRTCs not mentioned in the list */
  for (l = gf_gpu_get_crtcs (xrandr->gpu); l; l = l->next)
    {
      GfCrtc *crtc = l->data;

      if (crtc->is_dirty)
        {
          crtc->is_dirty = FALSE;
          continue;
        }

      if (!crtc->config)
        continue;

      xrandr_set_crtc_config (xrandr,
                              crtc,
                              save_timestamp,
                              (xcb_randr_crtc_t) crtc->crtc_id,
                              XCB_CURRENT_TIME,
                              0, 0, XCB_NONE,
                              XCB_RANDR_ROTATION_ROTATE_0,
                              NULL, 0);

      gf_crtc_unset_config (crtc);
    }

  g_assert (width > 0 && height > 0);
  /* The 'physical size' of an X screen is meaningless if that screen
   * can consist of many monitors. So just pick a size that make the
   * dpi 96.
   *
   * Firefox and Evince apparently believe what X tells them.
   */
  width_mm = (width / DPI_FALLBACK) * 25.4 + 0.5;
  height_mm = (height / DPI_FALLBACK) * 25.4 + 0.5;
  XRRSetScreenSize (xrandr->xdisplay, xrandr->xroot,
                    width, height, width_mm, height_mm);

  for (i = 0; i < n_crtcs; i++)
    {
      GfCrtcInfo *crtc_info = crtcs[i];
      GfCrtc *crtc = crtc_info->crtc;

      if (crtc_info->mode != NULL)
        {
          GfCrtcMode *mode;
          xcb_randr_output_t *output_ids;
          guint j, n_output_ids;
          xcb_randr_rotation_t rotation;

          mode = crtc_info->mode;

          n_output_ids = crtc_info->outputs->len;
          output_ids = g_new0 (xcb_randr_output_t, n_output_ids);

          for (j = 0; j < n_output_ids; j++)
            {
              GfOutput *output;

              output = ((GfOutput**) crtc_info->outputs->pdata)[j];

              output->is_dirty = TRUE;
              gf_output_assign_crtc (output, crtc);

              output_ids[j] = output->winsys_id;
            }

          rotation = gf_monitor_transform_to_xrandr (crtc_info->transform);
          if (!xrandr_set_crtc_config (xrandr,
                                       crtc,
                                       save_timestamp,
                                       (xcb_randr_crtc_t) crtc->crtc_id,
                                       XCB_CURRENT_TIME,
                                       crtc_info->layout.x,
                                       crtc_info->layout.y,
                                       (xcb_randr_mode_t) mode->mode_id,
                                       rotation,
                                       output_ids, n_output_ids))
            {
              g_warning ("Configuring CRTC %d with mode %d (%d x %d @ %f) at position %d, %d and transform %u failed\n",
                         (guint) (crtc->crtc_id), (guint) (mode->mode_id),
                         mode->width, mode->height, (gdouble) mode->refresh_rate,
                         crtc_info->layout.x, crtc_info->layout.y,
                         crtc_info->transform);

              g_free (output_ids);
              continue;
            }

          gf_crtc_set_config (crtc,
                              &crtc_info->layout,
                              mode,
                              crtc_info->transform);

          g_free (output_ids);
        }
    }

  for (i = 0; i < n_outputs; i++)
    {
      GfOutputInfo *output_info = outputs[i];
      GfOutput *output = output_info->output;

      output->is_primary = output_info->is_primary;
      output->is_presentation = output_info->is_presentation;
      output->is_underscanning = output_info->is_underscanning;

      gf_output_xrandr_apply_mode (output);
    }

  /* Disable outputs not mentioned in the list */
  for (l = gf_gpu_get_outputs (xrandr->gpu); l; l = l->next)
    {
      GfOutput *output = l->data;

      if (output->is_dirty)
        {
          output->is_dirty = FALSE;
          continue;
        }

      gf_output_unassign_crtc (output);
      output->is_primary = FALSE;
    }

  XUngrabServer (xrandr->xdisplay);
  XFlush (xrandr->xdisplay);
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

  xrandr->gpu = GF_GPU (gf_gpu_xrandr_new (xrandr));
  gf_monitor_manager_add_gpu (GF_MONITOR_MANAGER (xrandr), xrandr->gpu);

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

  g_clear_object (&xrandr->gpu);
  g_clear_pointer (&xrandr->supported_scales, g_free);

  G_OBJECT_CLASS (gf_monitor_manager_xrandr_parent_class)->finalize (object);
}

static GBytes *
gf_monitor_manager_xrandr_read_edid (GfMonitorManager *manager,
                                     GfOutput         *output)
{
  return gf_output_xrandr_read_edid (output);
}

static void
gf_monitor_manager_xrandr_read_current_state (GfMonitorManager *manager)
{
  GfMonitorManagerXrandr *self;
  CARD16 dpms_state;
  BOOL dpms_enabled;
  GfPowerSave power_save_mode;
  GfMonitorManagerClass *parent_class;

  self = GF_MONITOR_MANAGER_XRANDR (manager);

  if (DPMSCapable (self->xdisplay) &&
      DPMSInfo (self->xdisplay, &dpms_state, &dpms_enabled) &&
      dpms_enabled)
    {
      switch (dpms_state)
        {
          case DPMSModeOn:
            power_save_mode = GF_POWER_SAVE_ON;
            break;

          case DPMSModeStandby:
            power_save_mode = GF_POWER_SAVE_STANDBY;
            break;

          case DPMSModeSuspend:
            power_save_mode = GF_POWER_SAVE_SUSPEND;
            break;

          case DPMSModeOff:
            power_save_mode = GF_POWER_SAVE_OFF;
            break;

          default:
            power_save_mode = GF_POWER_SAVE_UNSUPPORTED;
            break;
        }
    }
  else
    {
      power_save_mode = GF_POWER_SAVE_UNSUPPORTED;
    }

  gf_monitor_manager_power_save_mode_changed (manager, power_save_mode);

  parent_class = GF_MONITOR_MANAGER_CLASS (gf_monitor_manager_xrandr_parent_class);
  parent_class->read_current_state (manager);
}

static void
gf_monitor_manager_xrandr_ensure_initial_config (GfMonitorManager *manager)
{
  GfMonitorConfigManager *config_manager;
  GfMonitorsConfig *config;

  gf_monitor_manager_ensure_configured (manager);

  /* Normally we don't rebuild our data structures until we see the
   * RRScreenNotify event, but at least at startup we want to have the
   * right configuration immediately.
   */
  gf_monitor_manager_read_current_state (manager);

  config_manager = gf_monitor_manager_get_config_manager (manager);
  config = gf_monitor_config_manager_get_current (config_manager);

  gf_monitor_manager_update_logical_state_derived (manager, config);
}

static gboolean
gf_monitor_manager_xrandr_apply_monitors_config (GfMonitorManager        *manager,
                                                 GfMonitorsConfig        *config,
                                                 GfMonitorsConfigMethod   method,
                                                 GError                 **error)
{
  GPtrArray *crtc_infos;
  GPtrArray *output_infos;

  if (!config)
    {
      gf_monitor_manager_xrandr_rebuild_derived (manager, NULL);
      return TRUE;
    }

  if (!gf_monitor_config_manager_assign (manager, config,
                                         &crtc_infos, &output_infos,
                                         error))
    return FALSE;

  if (method != GF_MONITORS_CONFIG_METHOD_VERIFY)
    {
      /*
       * If the assignment has not changed, we won't get any notification about
       * any new configuration from the X server; but we still need to update
       * our own configuration, as something not applicable in Xrandr might
       * have changed locally, such as the logical monitors scale. This means we
       * must check that our new assignment actually changes anything, otherwise
       * just update the logical state.
       */
      if (is_assignments_changed (manager,
                                  (GfCrtcInfo **) crtc_infos->pdata,
                                  crtc_infos->len,
                                  (GfOutputInfo **) output_infos->pdata,
                                  output_infos->len))
        {
          apply_crtc_assignments (manager,
                                  TRUE,
                                  (GfCrtcInfo **) crtc_infos->pdata,
                                  crtc_infos->len,
                                  (GfOutputInfo **) output_infos->pdata,
                                  output_infos->len);
        }
      else
        {
          gf_monitor_manager_xrandr_rebuild_derived (manager, config);
        }
    }

  g_ptr_array_free (crtc_infos, TRUE);
  g_ptr_array_free (output_infos, TRUE);

  return TRUE;
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
  gf_output_xrandr_change_backlight (output, value);
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
  GfMonitorManagerXrandr *xrandr;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  ensure_supported_monitor_scales (manager);

  *n_supported_scales = xrandr->n_supported_scales;
  return g_memdup (xrandr->supported_scales,
                   xrandr->n_supported_scales * sizeof (gfloat));
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

  gf_gpu_xrandr_get_max_screen_size (GF_GPU_XRANDR (xrandr->gpu),
                                     max_width, max_height);

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

  manager_class->read_edid = gf_monitor_manager_xrandr_read_edid;
  manager_class->read_current_state = gf_monitor_manager_xrandr_read_current_state;
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

Display *
gf_monitor_manager_xrandr_get_xdisplay (GfMonitorManagerXrandr *xrandr)
{
  return xrandr->xdisplay;
}

gboolean
gf_monitor_manager_xrandr_has_randr15 (GfMonitorManagerXrandr *xrandr)
{
  return xrandr->has_randr15;
}

gboolean
gf_monitor_manager_xrandr_handle_xevent (GfMonitorManagerXrandr *xrandr,
                                         XEvent                 *event)
{
  GfMonitorManager *manager;
  GfGpuXrandr *gpu_xrandr;
  XRRScreenResources *resources;

  manager = GF_MONITOR_MANAGER (xrandr);

  if ((event->type - xrandr->rr_event_base) != RRScreenChangeNotify)
    return FALSE;

  XRRUpdateConfiguration (event);
  gf_monitor_manager_read_current_state (manager);

  gpu_xrandr = GF_GPU_XRANDR (xrandr->gpu);
  resources = gf_gpu_xrandr_get_resources (gpu_xrandr);

  if (!resources)
    return TRUE;

  if (resources->timestamp < resources->configTimestamp)
    {
      gf_monitor_manager_on_hotplug (manager);
    }
  else
    {
      GfMonitorsConfig *config;

      config = NULL;

      if (resources->timestamp == xrandr->last_xrandr_set_timestamp)
        {
          GfMonitorConfigManager *config_manager;

          config_manager = gf_monitor_manager_get_config_manager (manager);
          config = gf_monitor_config_manager_get_current (config_manager);
        }

      gf_monitor_manager_xrandr_rebuild_derived (manager, config);
    }

  return TRUE;
}
