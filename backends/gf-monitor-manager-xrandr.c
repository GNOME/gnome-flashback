/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2017-2019 Alberts Muktupāvels
 * Copyright (C) 2020 NVIDIA CORPORATION
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

  Time              last_xrandr_set_timestamp;
};

typedef struct
{
  Atom xrandr_name;
} GfMonitorData;

G_DEFINE_TYPE (GfMonitorManagerXrandr, gf_monitor_manager_xrandr, GF_TYPE_MONITOR_MANAGER)

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

  if (!gf_crtc_xrandr_set_config (GF_CRTC_XRANDR (crtc),
                                  xrandr_crtc, timestamp,
                                  x, y, mode, rotation,
                                  outputs, n_outputs,
                                  &new_timestamp))
    return FALSE;

  if (save_timestamp)
    xrandr->last_xrandr_set_timestamp = new_timestamp;

  return TRUE;
}

static gboolean
is_crtc_assignment_changed (GfCrtc            *crtc,
                            GfCrtcAssignment **crtc_assignments,
                            unsigned int       n_crtc_assignments)
{
  guint i;

  for (i = 0; i < n_crtc_assignments; i++)
    {
      GfCrtcAssignment *crtc_assignment;

      crtc_assignment = crtc_assignments[i];

      if (crtc_assignment->crtc != crtc)
        continue;

      return gf_crtc_xrandr_is_assignment_changed (GF_CRTC_XRANDR (crtc),
                                                   crtc_assignment);
    }

  return !!gf_crtc_xrandr_get_current_mode (GF_CRTC_XRANDR (crtc));
}

static gboolean
is_output_assignment_changed (GfOutput            *output,
                              GfCrtcAssignment   **crtc_assignments,
                              guint                n_crtc_assignments,
                              GfOutputAssignment **output_assignments,
                              guint                n_output_assignments)
{
  gboolean output_is_found;
  GfCrtc *assigned_crtc;
  guint i;

  output_is_found = FALSE;

  for (i = 0; i < n_output_assignments; i++)
    {
      GfOutputAssignment *output_assignment;
      unsigned int max_bpc;

      output_assignment = output_assignments[i];

      if (output_assignment->output != output)
        continue;

      if (gf_output_is_primary (output) != output_assignment->is_primary)
        return TRUE;

      if (gf_output_is_presentation (output) != output_assignment->is_presentation)
        return TRUE;

      if (gf_output_is_underscanning (output) != output_assignment->is_underscanning)
        return TRUE;

      if (gf_output_get_max_bpc (output, &max_bpc))
        {
          if (!output_assignment->has_max_bpc ||
              max_bpc != output_assignment->max_bpc)
            return TRUE;
        }
      else if (output_assignment->has_max_bpc)
        {
          return TRUE;
        }

      output_is_found = TRUE;
    }

  assigned_crtc = gf_output_get_assigned_crtc (output);

  if (!output_is_found)
    return assigned_crtc != NULL;

  for (i = 0; i < n_crtc_assignments; i++)
    {
      GfCrtcAssignment *crtc_assignment;
      guint j;

      crtc_assignment = crtc_assignments[i];

      for (j = 0; j < crtc_assignment->outputs->len; j++)
        {
          GfOutput *crtc_assignment_output;

          crtc_assignment_output = ((GfOutput**) crtc_assignment->outputs->pdata)[j];

          if (crtc_assignment_output == output &&
              crtc_assignment->crtc == assigned_crtc)
            return FALSE;
        }
    }

  return TRUE;
}

static GfGpu *
get_gpu (GfMonitorManagerXrandr *self)
{
  GfMonitorManager *manager;
  GfBackend *backend;

  manager = GF_MONITOR_MANAGER (self);
  backend = gf_monitor_manager_get_backend (manager);

  return GF_GPU (gf_backend_get_gpus (backend)->data);
}

static gboolean
is_assignments_changed (GfMonitorManager    *manager,
                        GfCrtcAssignment   **crtc_assignments,
                        guint                n_crtc_assignments,
                        GfOutputAssignment **output_assignments,
                        guint                n_output_assignments)
{
  GfMonitorManagerXrandr *manager_xrandr;
  GfGpu *gpu;
  GList *l;

  manager_xrandr = GF_MONITOR_MANAGER_XRANDR (manager);
  gpu = get_gpu (manager_xrandr);

  for (l = gf_gpu_get_crtcs (gpu); l; l = l->next)
    {
      GfCrtc *crtc = l->data;

      if (is_crtc_assignment_changed (crtc, crtc_assignments, n_crtc_assignments))
        return TRUE;
    }

  for (l = gf_gpu_get_outputs (gpu); l; l = l->next)
    {
      GfOutput *output = l->data;

      if (is_output_assignment_changed (output,
                                        crtc_assignments,
                                        n_crtc_assignments,
                                        output_assignments,
                                        n_output_assignments))
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
apply_crtc_assignments (GfMonitorManager    *manager,
                        gboolean             save_timestamp,
                        GfCrtcAssignment   **crtcs,
                        guint                n_crtcs,
                        GfOutputAssignment **outputs,
                        guint                n_outputs)
{
  GfMonitorManagerXrandr *xrandr;
  GfGpu *gpu;
  GList *to_configure_outputs;
  GList *to_disable_crtcs;
  gint width, height, width_mm, height_mm;
  guint i;
  GList *l;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);
  gpu = get_gpu (xrandr);

  to_configure_outputs = g_list_copy (gf_gpu_get_outputs (gpu));
  to_disable_crtcs = g_list_copy (gf_gpu_get_crtcs (gpu));

  XGrabServer (xrandr->xdisplay);

  /* First compute the new size of the screen (framebuffer) */
  width = 0; height = 0;
  for (i = 0; i < n_crtcs; i++)
    {
      GfCrtcAssignment *crtc_assignment = crtcs[i];
      GfCrtc *crtc = crtc_assignment->crtc;

      if (crtc_assignment->mode == NULL)
        continue;

      to_disable_crtcs = g_list_remove (to_disable_crtcs, crtc);

      width = MAX (width, crtc_assignment->layout.x + crtc_assignment->layout.width);
      height = MAX (height, crtc_assignment->layout.y + crtc_assignment->layout.height);
    }

  /* Second disable all newly disabled CRTCs, or CRTCs that in the previous
   * configuration would be outside the new framebuffer (otherwise X complains
   * loudly when resizing)
   * CRTC will be enabled again after resizing the FB
   */
  for (i = 0; i < n_crtcs; i++)
    {
      GfCrtcAssignment *crtc_assignment = crtcs[i];
      GfCrtc *crtc = crtc_assignment->crtc;
      const GfCrtcConfig *crtc_config;
      int x2, y2;

      crtc_config = gf_crtc_get_config (crtc);
      if (crtc_config == NULL)
        continue;

      x2 = crtc_config->layout.x + crtc_config->layout.width;
      y2 = crtc_config->layout.y + crtc_config->layout.height;

      if (crtc_assignment->mode == NULL || x2 > width || y2 > height)
        {
          xrandr_set_crtc_config (xrandr,
                                  crtc,
                                  save_timestamp,
                                  (xcb_randr_crtc_t) gf_crtc_get_id (crtc),
                                  XCB_CURRENT_TIME,
                                  0, 0, XCB_NONE,
                                  XCB_RANDR_ROTATION_ROTATE_0,
                                  NULL, 0);

          gf_crtc_unset_config (crtc);
        }
    }

  for (l = to_disable_crtcs; l; l = l->next)
    {
      GfCrtc *crtc = l->data;

      if (!gf_crtc_get_config (crtc))
        continue;

      xrandr_set_crtc_config (xrandr,
                              crtc,
                              save_timestamp,
                              (xcb_randr_crtc_t) gf_crtc_get_id (crtc),
                              XCB_CURRENT_TIME,
                              0, 0, XCB_NONE,
                              XCB_RANDR_ROTATION_ROTATE_0,
                              NULL, 0);

      gf_crtc_unset_config (crtc);
    }

  if (n_crtcs == 0)
    goto out;

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
      GfCrtcAssignment *crtc_assignment = crtcs[i];
      GfCrtc *crtc = crtc_assignment->crtc;

      if (crtc_assignment->mode != NULL)
        {
          GfCrtcMode *crtc_mode;
          xcb_randr_output_t *output_ids;
          guint j, n_output_ids;
          xcb_randr_rotation_t rotation;
          xcb_randr_mode_t mode;

          crtc_mode = crtc_assignment->mode;

          n_output_ids = crtc_assignment->outputs->len;
          output_ids = g_new0 (xcb_randr_output_t, n_output_ids);

          for (j = 0; j < n_output_ids; j++)
            {
              GfOutput *output;
              GfOutputAssignment *output_assignment;

              output = ((GfOutput**) crtc_assignment->outputs->pdata)[j];

              to_configure_outputs = g_list_remove (to_configure_outputs, output);

              output_assignment = gf_find_output_assignment (outputs, n_outputs, output);
              gf_output_assign_crtc (output, crtc, output_assignment);

              output_ids[j] = gf_output_get_id (output);
            }

          rotation = gf_monitor_transform_to_xrandr (crtc_assignment->transform);
          mode = gf_crtc_mode_get_id (crtc_mode);

          if (!xrandr_set_crtc_config (xrandr,
                                       crtc,
                                       save_timestamp,
                                       (xcb_randr_crtc_t) gf_crtc_get_id (crtc),
                                       XCB_CURRENT_TIME,
                                       crtc_assignment->layout.x,
                                       crtc_assignment->layout.y,
                                       mode,
                                       rotation,
                                       output_ids, n_output_ids))
            {
              const GfCrtcModeInfo *crtc_mode_info;

              crtc_mode_info = gf_crtc_mode_get_info (crtc_mode);

              g_warning ("Configuring CRTC %d with mode %d (%d x %d @ %f) at position %d, %d and transform %u failed\n",
                         (unsigned) gf_crtc_get_id (crtc),
                         (unsigned) mode,
                         crtc_mode_info->width,
                         crtc_mode_info->height,
                         (double) crtc_mode_info->refresh_rate,
                         crtc_assignment->layout.x,
                         crtc_assignment->layout.y,
                         crtc_assignment->transform);

              g_free (output_ids);
              continue;
            }

          gf_crtc_set_config (crtc,
                              &crtc_assignment->layout,
                              crtc_mode,
                              crtc_assignment->transform);

          g_free (output_ids);
        }
    }

  for (i = 0; i < n_outputs; i++)
    {
      GfOutputAssignment *output_assignment = outputs[i];
      GfOutput *output = output_assignment->output;

      gf_output_xrandr_apply_mode (GF_OUTPUT_XRANDR (output));
    }

  for (l = to_configure_outputs; l; l = l->next)
    {
      GfOutput *output = l->data;

      gf_output_unassign_crtc (output);
    }

out:
  XUngrabServer (xrandr->xdisplay);
  XFlush (xrandr->xdisplay);

  g_clear_pointer (&to_configure_outputs, g_list_free);
  g_clear_pointer (&to_disable_crtcs, g_list_free);
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

static GBytes *
gf_monitor_manager_xrandr_read_edid (GfMonitorManager *manager,
                                     GfOutput         *output)
{
  return gf_output_xrandr_read_edid (GF_OUTPUT_XRANDR (output));
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
  GPtrArray *crtc_assignments;
  GPtrArray *output_assignments;

  if (!config)
    {
      if (!manager->in_init)
        apply_crtc_assignments (manager, TRUE, NULL, 0, NULL, 0);

      gf_monitor_manager_rebuild_derived (manager, NULL);
      return TRUE;
    }

  if (!gf_monitor_config_manager_assign (manager, config,
                                         &crtc_assignments, &output_assignments,
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
                                  (GfCrtcAssignment **) crtc_assignments->pdata,
                                  crtc_assignments->len,
                                  (GfOutputAssignment **) output_assignments->pdata,
                                  output_assignments->len))
        {
          apply_crtc_assignments (manager,
                                  TRUE,
                                  (GfCrtcAssignment **) crtc_assignments->pdata,
                                  crtc_assignments->len,
                                  (GfOutputAssignment **) output_assignments->pdata,
                                  output_assignments->len);
        }
      else
        {
          gf_monitor_manager_rebuild_derived (manager, config);
        }
    }

  g_ptr_array_free (crtc_assignments, TRUE);
  g_ptr_array_free (output_assignments, TRUE);

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
  gamma = XRRGetCrtcGamma (xrandr->xdisplay, (XID) gf_crtc_get_id (crtc));

  if (size)
    *size = gamma->size;

  if (red)
    *red = g_memdup2 (gamma->red, sizeof (gushort) * gamma->size);

  if (green)
    *green = g_memdup2 (gamma->green, sizeof (gushort) * gamma->size);

  if (blue)
    *blue = g_memdup2 (gamma->blue, sizeof (gushort) * gamma->size);

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

  XRRSetCrtcGamma (xrandr->xdisplay, (XID) gf_crtc_get_id (crtc), gamma);
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

      monitor_info->outputs[i] = gf_output_get_id (output);
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
  g_warn_if_fail ((gf_crtc_get_all_transforms (crtc) & transform) == transform);

  return TRUE;
}

static gfloat
gf_monitor_manager_xrandr_calculate_monitor_mode_scale (GfMonitorManager           *manager,
                                                        GfLogicalMonitorLayoutMode  layout_mode,
                                                        GfMonitor                  *monitor,
                                                        GfMonitorMode              *monitor_mode)
{
  GfMonitorScalesConstraint constraints;

  constraints = GF_MONITOR_SCALES_CONSTRAINT_NO_FRAC;

  return gf_monitor_calculate_mode_scale (monitor, monitor_mode, constraints);
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

  return gf_monitor_calculate_supported_scales (monitor,
                                                monitor_mode,
                                                constraints,
                                                n_supported_scales);
}

static GfMonitorManagerCapability
gf_monitor_manager_xrandr_get_capabilities (GfMonitorManager *manager)
{
  return GF_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED;
}

static gboolean
gf_monitor_manager_xrandr_get_max_screen_size (GfMonitorManager *manager,
                                               gint             *max_width,
                                               gint             *max_height)
{
  GfMonitorManagerXrandr *xrandr;
  GfGpu *gpu;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);
  gpu = get_gpu (xrandr);

  gf_gpu_xrandr_get_max_screen_size (GF_GPU_XRANDR (gpu),
                                     max_width, max_height);

  return TRUE;
}

static GfLogicalMonitorLayoutMode
gf_monitor_manager_xrandr_get_default_layout_mode (GfMonitorManager *manager)
{
  return GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;
}

static void
gf_monitor_manager_xrandr_set_output_ctm (GfOutput          *output,
                                          const GfOutputCtm *ctm)
{
  gf_output_xrandr_set_ctm (GF_OUTPUT_XRANDR (output), ctm);
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

  manager_class->read_edid = gf_monitor_manager_xrandr_read_edid;
  manager_class->read_current_state = gf_monitor_manager_xrandr_read_current_state;
  manager_class->ensure_initial_config = gf_monitor_manager_xrandr_ensure_initial_config;
  manager_class->apply_monitors_config = gf_monitor_manager_xrandr_apply_monitors_config;
  manager_class->set_power_save_mode = gf_monitor_manager_xrandr_set_power_save_mode;
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
  manager_class->set_output_ctm = gf_monitor_manager_xrandr_set_output_ctm;
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
  GfGpu *gpu;
  GfGpuXrandr *gpu_xrandr;
  XRRScreenResources *resources;

  manager = GF_MONITOR_MANAGER (xrandr);

  if ((event->type - xrandr->rr_event_base) != RRScreenChangeNotify)
    return FALSE;

  XRRUpdateConfiguration (event);
  gf_monitor_manager_read_current_state (manager);

  gpu = get_gpu (xrandr);
  gpu_xrandr = GF_GPU_XRANDR (gpu);
  resources = gf_gpu_xrandr_get_resources (gpu_xrandr);

  if (!resources)
    return TRUE;

  if (resources->timestamp < resources->configTimestamp)
    {
      gf_monitor_manager_reconfigure (manager);
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

      gf_monitor_manager_rebuild_derived (manager, config);
    }

  return TRUE;
}
