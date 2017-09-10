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
  guint i;

  monitor = GF_MONITOR (normal);
  output = gf_monitor_get_main_output (monitor);

  for (i = 0; i < output->n_modes; i++)
    {
      GfCrtcMode *crtc_mode;
      GfMonitorMode *mode;

      crtc_mode = output->modes[i];

      mode = g_new0 (GfMonitorMode, 1);
      mode->spec.width = crtc_mode->width;
      mode->spec.height = crtc_mode->height;
      mode->spec.refresh_rate = crtc_mode->refresh_rate;
      mode->spec.flags = crtc_mode->flags & HANDLED_CRTC_MODE_FLAGS;

      mode->id = gf_monitor_mode_spec_generate_id (&mode->spec);

      mode->crtc_modes = g_new (GfMonitorCrtcMode, 1);
      mode->crtc_modes[0].output = output;
      mode->crtc_modes[0].crtc_mode = crtc_mode;

      if (gf_monitor_add_mode (monitor, mode))
        {
          if (crtc_mode == output->preferred_mode)
            gf_monitor_set_preferred_mode (monitor, mode);

          if (output->crtc && crtc_mode == output->crtc->current_mode)
            gf_monitor_set_current_mode (monitor, mode);
        }
      else
        {
          gf_monitor_mode_free (mode);
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

  output = gf_monitor_get_main_output (monitor);

  *layout = (GfRectangle) {
    .x = output->crtc->rect.x,
    .y = output->crtc->rect.y,
    .width = output->crtc->rect.width,
    .height = output->crtc->rect.height
  };
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
  GfOutput *output;

  output = gf_monitor_get_main_output (monitor);
  if (output->suggested_x < 0 && output->suggested_y < 0)
    return FALSE;

  *x = output->suggested_x;
  *y = output->suggested_y;

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
  GfMonitorNormal *normal;
  GfMonitor *monitor;

  normal = g_object_new (GF_TYPE_MONITOR_NORMAL,
                         "monitor-manager", monitor_manager,
                         NULL);

  monitor = GF_MONITOR (normal);

  gf_monitor_append_output (monitor, output);
  gf_monitor_set_winsys_id (monitor, output->winsys_id);
  gf_monitor_generate_spec (monitor);
  generate_modes (normal);

  return normal;
}
