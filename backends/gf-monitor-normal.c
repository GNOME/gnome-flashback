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
#include "gf-crtc-private.h"
#include "gf-monitor-normal-private.h"
#include "gf-output-private.h"

struct _GfMonitorNormal
{
  GfMonitor parent;
};

G_DEFINE_TYPE (GfMonitorNormal, gf_monitor_normal, GF_TYPE_MONITOR)

static void
generate_modes (GfMonitorNormal *normal)
{
  GfMonitor *monitor;
  GfOutput *output;
  const GfOutputInfo *output_info;
  GfCrtcMode *preferred_mode;
  GfCrtcModeFlag preferred_mode_flags;
  guint i;

  monitor = GF_MONITOR (normal);
  output = gf_monitor_get_main_output (monitor);
  output_info = gf_output_get_info (output);
  preferred_mode = output_info->preferred_mode;
  preferred_mode_flags = gf_crtc_mode_get_info (preferred_mode)->flags;

  for (i = 0; i < output_info->n_modes; i++)
    {
      GfCrtcMode *crtc_mode;
      const GfCrtcModeInfo *crtc_mode_info;
      GfMonitorMode *mode;
      gboolean replace;
      GfCrtc *crtc;

      crtc_mode = output_info->modes[i];
      crtc_mode_info = gf_crtc_mode_get_info (crtc_mode);

      mode = g_new0 (GfMonitorMode, 1);
      mode->monitor = monitor;
      mode->spec = gf_monitor_create_spec (monitor,
                                           crtc_mode_info->width,
                                           crtc_mode_info->height,
                                           crtc_mode);

      mode->id = gf_monitor_mode_spec_generate_id (&mode->spec);

      mode->crtc_modes = g_new (GfMonitorCrtcMode, 1);
      mode->crtc_modes[0].output = output;
      mode->crtc_modes[0].crtc_mode = crtc_mode;

      /*
       * We don't distinguish between all available mode flags, just the ones
       * that are configurable. We still need to pick some mode though, so
       * prefer ones that has the same set of flags as the preferred mode;
       * otherwise take the first one in the list. This guarantees that the
       * preferred mode is always added.
       */
      replace = crtc_mode_info->flags == preferred_mode_flags;

      if (!gf_monitor_add_mode (monitor, mode, replace))
        {
          g_assert (crtc_mode != output_info->preferred_mode);
          gf_monitor_mode_free (mode);
          continue;
        }

      if (crtc_mode == output_info->preferred_mode)
        gf_monitor_set_preferred_mode (monitor, mode);

      crtc = gf_output_get_assigned_crtc (output);

      if (crtc != NULL)
        {
          const GfCrtcConfig *crtc_config;

          crtc_config = gf_crtc_get_config (crtc);
          if (crtc_config && crtc_mode == crtc_config->mode)
            gf_monitor_set_current_mode (monitor, mode);
        }
    }
}

static GfOutput *
gf_monitor_normal_get_main_output (GfMonitor *monitor)
{
  GList *outputs;

  outputs = gf_monitor_get_outputs (monitor);

  return outputs->data;
}

static void
gf_monitor_normal_derive_layout (GfMonitor   *monitor,
                                 GfRectangle *layout)
{
  GfOutput *output;
  GfCrtc *crtc;
  const GfCrtcConfig *crtc_config;

  output = gf_monitor_get_main_output (monitor);
  crtc = gf_output_get_assigned_crtc (output);
  crtc_config = gf_crtc_get_config (crtc);

  g_return_if_fail (crtc_config);

  *layout = crtc_config->layout;
}

static void
gf_monitor_normal_calculate_crtc_pos (GfMonitor          *monitor,
                                      GfMonitorMode      *monitor_mode,
                                      GfOutput           *output,
                                      GfMonitorTransform  crtc_transform,
                                      gint               *out_x,
                                      gint               *out_y)
{
  *out_x = 0;
  *out_y = 0;
}

static gboolean
gf_monitor_normal_get_suggested_position (GfMonitor *monitor,
                                          gint      *x,
                                          gint      *y)
{
  const GfOutputInfo *output_info;

  output_info = gf_monitor_get_main_output_info (monitor);

  if (!output_info->hotplug_mode_update)
    return FALSE;

  if (output_info->suggested_x < 0 && output_info->suggested_y < 0)
    return FALSE;

  if (x != NULL)
    *x = output_info->suggested_x;

  if (y != NULL)
    *y = output_info->suggested_y;

  return TRUE;
}

static void
gf_monitor_normal_class_init (GfMonitorNormalClass *normal_class)
{
  GfMonitorClass *monitor_class;

  monitor_class = GF_MONITOR_CLASS (normal_class);

  monitor_class->get_main_output = gf_monitor_normal_get_main_output;
  monitor_class->derive_layout = gf_monitor_normal_derive_layout;
  monitor_class->calculate_crtc_pos = gf_monitor_normal_calculate_crtc_pos;
  monitor_class->get_suggested_position = gf_monitor_normal_get_suggested_position;
}

static void
gf_monitor_normal_init (GfMonitorNormal *normal)
{
}

GfMonitorNormal *
gf_monitor_normal_new (GfMonitorManager *monitor_manager,
                       GfOutput         *output)
{
  GfBackend *backend;
  GfMonitorNormal *normal;
  GfMonitor *monitor;

  backend = gf_monitor_manager_get_backend (monitor_manager);
  normal = g_object_new (GF_TYPE_MONITOR_NORMAL,
                         "backend", backend,
                         NULL);

  monitor = GF_MONITOR (normal);

  gf_monitor_append_output (monitor, output);
  gf_output_set_monitor (output, monitor);

  gf_monitor_set_winsys_id (monitor, gf_output_get_id (output));
  gf_monitor_generate_spec (monitor);
  generate_modes (normal);

  gf_monitor_make_display_name (monitor);

  return normal;
}
