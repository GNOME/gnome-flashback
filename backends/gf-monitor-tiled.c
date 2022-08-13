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
#include "gf-monitor-manager-private.h"
#include "gf-monitor-spec-private.h"
#include "gf-monitor-tiled-private.h"
#include "gf-output-private.h"

typedef struct
{
  GfMonitorMode parent;
  gboolean      is_tiled;
} GfMonitorModeTiled;

struct _GfMonitorTiled
{
  GfMonitor         parent;

  GfMonitorManager *monitor_manager;

  uint32_t          tile_group_id;

  /* The tile (0, 0) output. */
  GfOutput         *origin_output;

  /* The output enabled even when a non-tiled mode is used. */
  GfOutput         *main_output;
};

G_DEFINE_TYPE (GfMonitorTiled, gf_monitor_tiled, GF_TYPE_MONITOR)

static gboolean
is_crtc_mode_tiled (GfOutput   *output,
                    GfCrtcMode *crtc_mode)
{
  const GfOutputInfo *output_info;
  const GfCrtcModeInfo *crtc_mode_info;

  output_info = gf_output_get_info (output);
  crtc_mode_info = gf_crtc_mode_get_info (crtc_mode);

  return (crtc_mode_info->width == (int) output_info->tile_info.tile_w &&
          crtc_mode_info->height == (int) output_info->tile_info.tile_h);
}

static GfCrtcMode *
find_tiled_crtc_mode (GfOutput   *output,
                      GfCrtcMode *reference_crtc_mode)
{
  const GfOutputInfo *output_info;
  const GfCrtcModeInfo *reference_crtc_mode_info;
  GfCrtcMode *crtc_mode;
  guint i;

  output_info = gf_output_get_info (output);
  reference_crtc_mode_info = gf_crtc_mode_get_info (reference_crtc_mode);
  crtc_mode = output_info->preferred_mode;

  if (is_crtc_mode_tiled (output, crtc_mode))
    return crtc_mode;

  for (i = 0; i < output_info->n_modes; i++)
    {
      const GfCrtcModeInfo *crtc_mode_info;

      crtc_mode = output_info->modes[i];
      crtc_mode_info = gf_crtc_mode_get_info (crtc_mode);

      if (!is_crtc_mode_tiled (output, crtc_mode))
        continue;

      if (crtc_mode_info->refresh_rate != reference_crtc_mode_info->refresh_rate)
        continue;

      if (crtc_mode_info->flags != reference_crtc_mode_info->flags)
        continue;

      return crtc_mode;
    }

  return NULL;
}

static void
calculate_tiled_size (GfMonitor *monitor,
                      gint      *out_width,
                      gint      *out_height)
{
  GList *outputs;
  GList *l;
  gint width;
  gint height;

  outputs = gf_monitor_get_outputs (monitor);

  width = 0;
  height = 0;

  for (l = outputs; l; l = l->next)
    {
      const GfOutputInfo *output_info;

      output_info = gf_output_get_info (l->data);

      if (output_info->tile_info.loc_v_tile == 0)
        width += output_info->tile_info.tile_w;

      if (output_info->tile_info.loc_h_tile == 0)
        height += output_info->tile_info.tile_h;
    }

  *out_width = width;
  *out_height = height;
}

static GfMonitorMode *
create_tiled_monitor_mode (GfMonitorTiled *tiled,
                           GfCrtcMode     *reference_crtc_mode,
                           gboolean       *out_is_preferred)
{
  GfMonitor *monitor;
  GList *outputs;
  gboolean is_preferred;
  GfMonitorModeTiled *mode;
  gint width, height;
  GList *l;
  guint i;

  monitor = GF_MONITOR (tiled);
  outputs = gf_monitor_get_outputs (monitor);

  is_preferred = TRUE;

  mode = g_new0 (GfMonitorModeTiled, 1);
  mode->is_tiled = TRUE;

  calculate_tiled_size (monitor, &width, &height);

  mode->parent.monitor = monitor;
  mode->parent.spec = gf_monitor_create_spec (monitor,
                                              width,
                                              height,
                                              reference_crtc_mode);

  mode->parent.id = gf_monitor_mode_spec_generate_id (&mode->parent.spec);
  mode->parent.crtc_modes = g_new0 (GfMonitorCrtcMode, g_list_length (outputs));

  for (l = outputs, i = 0; l; l = l->next, i++)
    {
      GfOutput *output;
      const GfOutputInfo *output_info;
      GfCrtcMode *tiled_crtc_mode;

      output = l->data;
      output_info = gf_output_get_info (output);
      tiled_crtc_mode = find_tiled_crtc_mode (output, reference_crtc_mode);

      if (!tiled_crtc_mode)
        {
          g_warning ("No tiled mode found on %s", gf_output_get_name (output));
          gf_monitor_mode_free ((GfMonitorMode *) mode);
          return NULL;
        }

      mode->parent.crtc_modes[i].output = output;
      mode->parent.crtc_modes[i].crtc_mode = tiled_crtc_mode;

      is_preferred = (is_preferred &&
                      tiled_crtc_mode == output_info->preferred_mode);
    }

  *out_is_preferred = is_preferred;

  return (GfMonitorMode *) mode;
}

static GfMonitorMode *
create_untiled_monitor_mode (GfMonitorTiled *tiled,
                             GfOutput       *main_output,
                             GfCrtcMode     *crtc_mode)
{
  GfMonitor *monitor;
  GList *outputs;
  GfMonitorModeTiled *mode;
  const GfCrtcModeInfo *crtc_mode_info;
  GList *l;
  guint i;

  monitor = GF_MONITOR (tiled);
  outputs = gf_monitor_get_outputs (monitor);

  if (is_crtc_mode_tiled (main_output, crtc_mode))
    return NULL;

  mode = g_new0 (GfMonitorModeTiled, 1);
  mode->is_tiled = FALSE;

  mode->parent.monitor = monitor;

  crtc_mode_info = gf_crtc_mode_get_info (crtc_mode);
  mode->parent.spec = gf_monitor_create_spec (monitor,
                                              crtc_mode_info->width,
                                              crtc_mode_info->height,
                                              crtc_mode);

  mode->parent.id = gf_monitor_mode_spec_generate_id (&mode->parent.spec);
  mode->parent.crtc_modes = g_new0 (GfMonitorCrtcMode, g_list_length (outputs));

  for (l = outputs, i = 0; l; l = l->next, i++)
    {
      GfOutput *output = l->data;

      if (output == main_output)
        {
          mode->parent.crtc_modes[i].output = output;
          mode->parent.crtc_modes[i].crtc_mode = crtc_mode;
        }
      else
        {
          mode->parent.crtc_modes[i].output = output;
          mode->parent.crtc_modes[i].crtc_mode = NULL;
        }
    }

  return &mode->parent;
}

static void
generate_tiled_monitor_modes (GfMonitorTiled *tiled)
{
  GfMonitor *monitor;
  GfOutput *main_output;
  const GfOutputInfo *main_output_info;
  GList *tiled_modes;
  GfMonitorMode *best_mode;
  guint i;
  GList *l;

  monitor = GF_MONITOR (tiled);
  main_output = gf_monitor_get_main_output (monitor);
  main_output_info = gf_output_get_info (main_output);

  tiled_modes = NULL;
  best_mode = NULL;

  for (i = 0; i < main_output_info->n_modes; i++)
    {
      GfCrtcMode *reference_crtc_mode;
      GfMonitorMode *mode;
      gboolean is_preferred;

      reference_crtc_mode = main_output_info->modes[i];

      if (!is_crtc_mode_tiled (main_output, reference_crtc_mode))
        continue;

      mode = create_tiled_monitor_mode (tiled, reference_crtc_mode, &is_preferred);

      if (!mode)
        continue;

      tiled_modes = g_list_append (tiled_modes, mode);

      if (gf_monitor_is_mode_assigned (monitor, mode))
        gf_monitor_set_current_mode (monitor, mode);

      if (is_preferred)
        gf_monitor_set_preferred_mode (monitor, mode);
    }

  while ((l = tiled_modes))
    {
      GfMonitorMode *mode;
      GfMonitorMode *preferred_mode;

      mode = l->data;
      tiled_modes = g_list_remove_link (tiled_modes, l);

      if (!gf_monitor_add_mode (monitor, mode, FALSE))
        {
          gf_monitor_mode_free (mode);
          continue;
        }

      preferred_mode = gf_monitor_get_preferred_mode (monitor);
      if (!preferred_mode)
        {
          if (!best_mode || mode->spec.refresh_rate > best_mode->spec.refresh_rate)
            best_mode = mode;
        }
    }

  if (best_mode)
    gf_monitor_set_preferred_mode (monitor, best_mode);
}

static void
generate_untiled_monitor_modes (GfMonitorTiled *tiled)
{
  GfMonitor *monitor;
  GfOutput *main_output;
  const GfOutputInfo *main_output_info;
  guint i;

  monitor = GF_MONITOR (tiled);
  main_output = gf_monitor_get_main_output (monitor);
  main_output_info = gf_output_get_info (main_output);

  for (i = 0; i < main_output_info->n_modes; i++)
    {
      GfCrtcMode *crtc_mode;
      GfMonitorMode *mode;
      GfMonitorMode *preferred_mode;

      crtc_mode = main_output_info->modes[i];
      mode = create_untiled_monitor_mode (tiled, main_output, crtc_mode);

      if (!mode)
        continue;

      if (!gf_monitor_add_mode (monitor, mode, FALSE))
        {
          gf_monitor_mode_free (mode);
          continue;
        }

      if (gf_monitor_is_mode_assigned (monitor, mode))
        {
          GfMonitorMode *current_mode;

          current_mode = gf_monitor_get_current_mode (monitor);
          g_assert (!current_mode);

          gf_monitor_set_current_mode (monitor, mode);
        }

      preferred_mode = gf_monitor_get_preferred_mode (monitor);
      if (!preferred_mode && crtc_mode == main_output_info->preferred_mode)
        gf_monitor_set_preferred_mode (monitor, mode);
    }
}

static GfMonitorMode *
find_best_mode (GfMonitor *monitor)
{
  GList *modes;
  GfMonitorMode *best_mode;
  GList *l;

  modes = gf_monitor_get_modes (monitor);
  best_mode = NULL;

  for (l = modes; l; l = l->next)
    {
      GfMonitorMode *mode;
      gint area, best_area;

      mode = l->data;

      if (!best_mode)
        {
          best_mode = mode;
          continue;
        }

      area = mode->spec.width * mode->spec.height;
      best_area = best_mode->spec.width * best_mode->spec.height;
      if (area > best_area)
        {
          best_mode = mode;
          continue;
        }

      if (mode->spec.refresh_rate > best_mode->spec.refresh_rate)
        {
          best_mode = mode;
          continue;
        }
    }

  return best_mode;
}

static void
generate_modes (GfMonitorTiled *tiled)
{
  GfMonitor *monitor;

  monitor = GF_MONITOR (tiled);

  /*
   * Tiled monitors may look a bit different from each other, depending on the
   * monitor itself, the driver, etc.
   *
   * On some, the tiled modes will be the preferred CRTC modes, and running
   * untiled is done by only enabling (0, 0) tile. In this case, things are
   * pretty straight forward.
   *
   * Other times a monitor may have some bogus mode preferred on the main tile,
   * and an untiled mode preferred on the non-main tile, and there seems to be
   * no guarantee that the (0, 0) tile is the one that should drive the
   * non-tiled mode.
   *
   * To handle both these cases, the following hueristics are implemented:
   *
   *  1) Find all the tiled CRTC modes of the (0, 0) tile, and create tiled
   *     monitor modes for all tiles based on these.
   *  2) If there is any tiled monitor mode combination where all CRTC modes
   *     are the preferred ones, that one is marked as preferred.
   *  3) If there is no preferred mode determined so far, assume the tiled
   *     monitor mode with the highest refresh rate is preferred.
   *  4) Find the tile with highest number of untiled CRTC modes available,
   *     assume this is the one driving the monitor in untiled mode, and
   *     create monitor modes for all untiled CRTC modes of that tile. If
   *     there is still no preferred mode, set any untiled mode as preferred
   *     if the CRTC mode is marked as such.
   *  5) If at this point there is still no preferred mode, just pick the one
   *     with the highest number of pixels and highest refresh rate.
   *
   * Note that this ignores the preference if the preference is a non-tiled
   * mode. This seems to be the case on some systems, where the user tends to
   * manually set up the tiled mode anyway.
   */

  generate_tiled_monitor_modes (tiled);

  if (!gf_monitor_get_preferred_mode (monitor))
    {
      GfMonitorSpec *spec;

      spec = gf_monitor_get_spec (monitor);

      g_warning ("Tiled monitor on %s didn't have any tiled modes",
                 spec->connector);
    }

  generate_untiled_monitor_modes (tiled);

  if (!gf_monitor_get_preferred_mode (monitor))
    {
      GfMonitorSpec *spec;

      spec = gf_monitor_get_spec (monitor);

      g_warning ("Tiled monitor on %s didn't have a valid preferred mode",
                 spec->connector);

      gf_monitor_set_preferred_mode (monitor, find_best_mode (monitor));
    }
}

static int
count_untiled_crtc_modes (GfOutput *output)
{
  const GfOutputInfo *output_info;
  gint count;
  guint i;

  output_info = gf_output_get_info (output);

  count = 0;
  for (i = 0; i < output_info->n_modes; i++)
    {
      GfCrtcMode *crtc_mode;

      crtc_mode = output_info->modes[i];
      if (!is_crtc_mode_tiled (output, crtc_mode))
        count++;
    }

  return count;
}

static GfOutput *
find_untiled_output (GfMonitorTiled *tiled)
{
  GfMonitor *monitor;
  GList *outputs;
  GfOutput *best_output;
  gint best_mode_count;
  GList *l;

  monitor = GF_MONITOR (tiled);
  outputs = gf_monitor_get_outputs (monitor);

  best_output = tiled->origin_output;
  best_mode_count = count_untiled_crtc_modes (tiled->origin_output);

  for (l = outputs; l; l = l->next)
    {
      GfOutput *output;
      gint mode_count;

      output = l->data;
      if (output == tiled->origin_output)
        continue;

      mode_count = count_untiled_crtc_modes (output);
      if (mode_count > best_mode_count)
        {
          best_mode_count = mode_count;
          best_output = output;
        }
    }

  return best_output;
}

static void
add_tiled_monitor_outputs (GfGpu          *gpu,
                           GfMonitorTiled *tiled)
{
  GfMonitor *monitor;
  GList *outputs;
  GList *l;

  monitor = GF_MONITOR (tiled);

  outputs = gf_gpu_get_outputs (gpu);
  for (l = outputs; l; l = l->next)
    {
      GfOutput *output;
      const GfOutputInfo *output_info;

      output = l->data;
      output_info = gf_output_get_info (output);

      if (output_info->tile_info.group_id != tiled->tile_group_id)
        continue;

      gf_monitor_append_output (monitor, output);
      gf_output_set_monitor (output, GF_MONITOR (tiled));
    }
}

static void
calculate_tile_coordinate (GfMonitor          *monitor,
                           GfOutput           *output,
                           GfMonitorTransform  crtc_transform,
                           gint               *out_x,
                           gint               *out_y)
{
  const GfOutputInfo *output_info;
  GList *outputs;
  GList *l;
  gint x;
  gint y;

  output_info = gf_output_get_info (output);
  outputs = gf_monitor_get_outputs (monitor);
  x = y = 0;

  for (l = outputs; l; l = l->next)
    {
      const GfOutputInfo *other_output_info;

      other_output_info = gf_output_get_info (l->data);

      switch (crtc_transform)
        {
          case GF_MONITOR_TRANSFORM_NORMAL:
          case GF_MONITOR_TRANSFORM_FLIPPED:
            if (other_output_info->tile_info.loc_v_tile == output_info->tile_info.loc_v_tile &&
                other_output_info->tile_info.loc_h_tile < output_info->tile_info.loc_h_tile)
              x += other_output_info->tile_info.tile_w;
            if (other_output_info->tile_info.loc_h_tile == output_info->tile_info.loc_h_tile &&
                other_output_info->tile_info.loc_v_tile < output_info->tile_info.loc_v_tile)
              y += other_output_info->tile_info.tile_h;
            break;

          case GF_MONITOR_TRANSFORM_180:
          case GF_MONITOR_TRANSFORM_FLIPPED_180:
            if (other_output_info->tile_info.loc_v_tile == output_info->tile_info.loc_v_tile &&
                other_output_info->tile_info.loc_h_tile > output_info->tile_info.loc_h_tile)
              x += other_output_info->tile_info.tile_w;
            if (other_output_info->tile_info.loc_h_tile == output_info->tile_info.loc_h_tile &&
                other_output_info->tile_info.loc_v_tile > output_info->tile_info.loc_v_tile)
              y += other_output_info->tile_info.tile_h;
            break;

          case GF_MONITOR_TRANSFORM_270:
          case GF_MONITOR_TRANSFORM_FLIPPED_270:
            if (other_output_info->tile_info.loc_v_tile == output_info->tile_info.loc_v_tile &&
                other_output_info->tile_info.loc_h_tile > output_info->tile_info.loc_h_tile)
              y += other_output_info->tile_info.tile_w;
            if (other_output_info->tile_info.loc_h_tile == output_info->tile_info.loc_h_tile &&
                other_output_info->tile_info.loc_v_tile > output_info->tile_info.loc_v_tile)
              x += other_output_info->tile_info.tile_h;
            break;

          case GF_MONITOR_TRANSFORM_90:
          case GF_MONITOR_TRANSFORM_FLIPPED_90:
            if (other_output_info->tile_info.loc_v_tile == output_info->tile_info.loc_v_tile &&
                other_output_info->tile_info.loc_h_tile < output_info->tile_info.loc_h_tile)
              y += other_output_info->tile_info.tile_w;
            if (other_output_info->tile_info.loc_h_tile == output_info->tile_info.loc_h_tile &&
                other_output_info->tile_info.loc_v_tile < output_info->tile_info.loc_v_tile)
              x += other_output_info->tile_info.tile_h;
            break;

          default:
            break;
        }
    }

  *out_x = x;
  *out_y = y;
}

static void
gf_monitor_tiled_finalize (GObject *object)
{
  GfMonitorTiled *self;

  self = GF_MONITOR_TILED (object);

  gf_monitor_manager_tiled_monitor_removed (self->monitor_manager,
                                            GF_MONITOR (self));

  G_OBJECT_CLASS (gf_monitor_tiled_parent_class)->finalize (object);
}

static GfOutput *
gf_monitor_tiled_get_main_output (GfMonitor *monitor)
{
  GfMonitorTiled *tiled;

  tiled = GF_MONITOR_TILED (monitor);

  return tiled->main_output;
}

static void
gf_monitor_tiled_derive_layout (GfMonitor   *monitor,
                                GfRectangle *layout)
{
  GList *outputs;
  GList *l;
  gint min_x;
  gint min_y;
  gint max_x;
  gint max_y;

  outputs = gf_monitor_get_outputs (monitor);

  min_x = INT_MAX;
  min_y = INT_MAX;
  max_x = 0;
  max_y = 0;

  for (l = outputs; l; l = l->next)
    {
      GfOutput *output;
      GfCrtc *crtc;
      const GfCrtcConfig *crtc_config;

      output = l->data;
      crtc = gf_output_get_assigned_crtc (output);

      if (!crtc)
        continue;

      crtc_config = gf_crtc_get_config (crtc);
      g_return_if_fail (crtc_config);

      min_x = MIN (crtc_config->layout.x, min_x);
      min_y = MIN (crtc_config->layout.y, min_y);
      max_x = MAX (crtc_config->layout.x + crtc_config->layout.width, max_x);
      max_y = MAX (crtc_config->layout.y + crtc_config->layout.height, max_y);
    }

  *layout = (GfRectangle) {
    .x = min_x,
    .y = min_y,
    .width = max_x - min_x,
    .height = max_y - min_y
  };
}

static void
gf_monitor_tiled_calculate_crtc_pos (GfMonitor          *monitor,
                                     GfMonitorMode      *monitor_mode,
                                     GfOutput           *output,
                                     GfMonitorTransform  crtc_transform,
                                     gint               *out_x,
                                     gint               *out_y)
{
  GfMonitorModeTiled *mode_tiled;

  mode_tiled = (GfMonitorModeTiled *) monitor_mode;

  if (mode_tiled->is_tiled)
    {
      calculate_tile_coordinate (monitor, output, crtc_transform, out_x, out_y);
    }
  else
    {
      *out_x = 0;
      *out_y = 0;
    }
}

static gboolean
gf_monitor_tiled_get_suggested_position (GfMonitor *monitor,
                                         gint      *x,
                                         gint      *y)
{
  return FALSE;
}

static void
gf_monitor_tiled_class_init (GfMonitorTiledClass *tiled_class)
{
  GObjectClass *object_class;
  GfMonitorClass *monitor_class;

  object_class = G_OBJECT_CLASS (tiled_class);
  monitor_class = GF_MONITOR_CLASS (tiled_class);

  object_class->finalize = gf_monitor_tiled_finalize;

  monitor_class->get_main_output = gf_monitor_tiled_get_main_output;
  monitor_class->derive_layout = gf_monitor_tiled_derive_layout;
  monitor_class->calculate_crtc_pos = gf_monitor_tiled_calculate_crtc_pos;
  monitor_class->get_suggested_position = gf_monitor_tiled_get_suggested_position;
}

static void
gf_monitor_tiled_init (GfMonitorTiled *tiled)
{
}

GfMonitorTiled *
gf_monitor_tiled_new (GfMonitorManager *monitor_manager,
                      GfOutput         *output)
{
  const GfOutputInfo *output_info;
  GfBackend *backend;
  GfMonitorTiled *tiled;
  GfMonitor *monitor;

  output_info = gf_output_get_info (output);

  backend = gf_monitor_manager_get_backend (monitor_manager);
  tiled = g_object_new (GF_TYPE_MONITOR_TILED,
                        "backend", backend,
                        NULL);

  monitor = GF_MONITOR (tiled);

  tiled->monitor_manager = monitor_manager;

  tiled->tile_group_id = output_info->tile_info.group_id;
  gf_monitor_set_winsys_id (monitor, gf_output_get_id (output));

  tiled->origin_output = output;
  add_tiled_monitor_outputs (gf_output_get_gpu (output), tiled);

  tiled->main_output = find_untiled_output (tiled);

  gf_monitor_generate_spec (monitor);

  gf_monitor_manager_tiled_monitor_added (monitor_manager, monitor);
  generate_modes (tiled);

  gf_monitor_make_display_name (monitor);

  return tiled;
}

uint32_t
gf_monitor_tiled_get_tile_group_id (GfMonitorTiled *tiled)
{
  return tiled->tile_group_id;
}
