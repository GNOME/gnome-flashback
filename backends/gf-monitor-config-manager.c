/*
 * Copyright (C) 2016 Red Hat
 * Copyright (C) 2017-2019 Alberts Muktupāvels
 * Copyright (c) 2018 DisplayLink (UK) Ltd.
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
 * - src/backends/meta-monitor-config-manager.c
 */

#include "config.h"

#include <gio/gio.h>
#include <math.h>

#include "gf-crtc-private.h"
#include "gf-monitor-config-store-private.h"
#include "gf-monitor-config-manager-private.h"
#include "gf-monitor-config-utils.h"
#include "gf-monitor-spec-private.h"
#include "gf-output-private.h"
#include "gf-rectangle-private.h"

#define CONFIG_HISTORY_MAX_SIZE 3

typedef struct
{
  GfMonitorManager       *monitor_manager;
  GfMonitorsConfig       *config;
  GfLogicalMonitorConfig *logical_monitor_config;
  GfMonitorConfig        *monitor_config;
  GPtrArray              *crtc_assignments;
  GPtrArray              *output_assignments;
  GArray                 *reserved_crtcs;
} MonitorAssignmentData;

typedef enum
{
  MONITOR_MATCH_ALL = 0,
  MONITOR_MATCH_EXTERNAL = (1 << 0),
  MONITOR_MATCH_BUILTIN = (1 << 1),
  MONITOR_MATCH_VISIBLE = (1 << 2),
  MONITOR_MATCH_WITH_SUGGESTED_POSITION = (1 << 3),
  MONITOR_MATCH_PRIMARY = (1 << 4),
  MONITOR_MATCH_ALLOW_FALLBACK = (1 << 5)
} MonitorMatchRule;

typedef enum
{
  MONITOR_POSITIONING_LINEAR,
  MONITOR_POSITIONING_SUGGESTED,
} MonitorPositioningMode;

struct _GfMonitorConfigManager
{
  GObject               parent;

  GfMonitorManager     *monitor_manager;

  GfMonitorConfigStore *config_store;

  GfMonitorsConfig     *current_config;
  GQueue                config_history;
};

G_DEFINE_TYPE (GfMonitorConfigManager, gf_monitor_config_manager, G_TYPE_OBJECT)

static GfMonitorsConfig *
get_root_config (GfMonitorsConfig *config)
{
  if (config->parent_config == NULL)
    return config;

  return get_root_config (config->parent_config);
}

static gboolean
has_same_root_config (GfMonitorsConfig *config_a,
                      GfMonitorsConfig *config_b)
{
  return get_root_config (config_a) == get_root_config (config_b);
}

static void
history_unref (gpointer data,
               gpointer user_data)
{
  g_object_unref (data);
}

static gboolean
is_lid_closed (GfMonitorManager *monitor_manager)
{
  GfBackend *backend;

  backend = gf_monitor_manager_get_backend (monitor_manager);

  return gf_backend_is_lid_closed (backend);
}

static gboolean
monitor_matches_rule (GfMonitor        *monitor,
                      GfMonitorManager *monitor_manager,
                      MonitorMatchRule  match_rule)
{
  if (monitor == NULL)
    return FALSE;

  if (match_rule & MONITOR_MATCH_BUILTIN)
    {
      if (!gf_monitor_is_laptop_panel (monitor))
        return FALSE;
    }
  else if (match_rule & MONITOR_MATCH_EXTERNAL)
    {
      if (gf_monitor_is_laptop_panel (monitor))
        return FALSE;
    }

  if (match_rule & MONITOR_MATCH_VISIBLE)
    {
      if (gf_monitor_is_laptop_panel (monitor) &&
          is_lid_closed (monitor_manager))
        return FALSE;
    }

  if (match_rule & MONITOR_MATCH_WITH_SUGGESTED_POSITION)
    {
      if (!gf_monitor_get_suggested_position (monitor, NULL, NULL))
        return FALSE;
    }

  return TRUE;
}

static GList *
find_monitors (GfMonitorManager *monitor_manager,
               MonitorMatchRule  match_rule,
               GfMonitor        *not_this_one)
{
  GList *result;
  GList *monitors;
  GList *l;

  result = NULL;
  monitors = gf_monitor_manager_get_monitors (monitor_manager);

  for (l = g_list_last (monitors); l; l = l->prev)
    {
      GfMonitor *monitor = l->data;

      if (not_this_one != NULL && monitor == not_this_one)
        continue;

      if (monitor_matches_rule (monitor, monitor_manager, match_rule))
        result = g_list_prepend (result, monitor);
    }

  return result;
}

static GfMonitor *
find_monitor_with_highest_preferred_resolution (GfMonitorManager *monitor_manager,
                                                MonitorMatchRule  match_rule)
{
  GList *monitors;
  GList *l;
  int largest_area = 0;
  GfMonitor *largest_monitor = NULL;

  monitors = find_monitors (monitor_manager, match_rule, NULL);

  for (l = monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;
      GfMonitorMode *mode;
      int width, height;
      int area;

      mode = gf_monitor_get_preferred_mode (monitor);
      gf_monitor_mode_get_resolution (mode, &width, &height);
      area = width * height;

      if (area > largest_area)
        {
          largest_area = area;
          largest_monitor = monitor;
        }
    }

  g_clear_pointer (&monitors, g_list_free);

  return largest_monitor;
}

/*
 * Try to find the primary monitor. The priority of classification is:
 *
 * 1. Find the primary monitor as reported by the underlying system,
 * 2. Find the laptop panel
 * 3. Find the external monitor with highest resolution
 *
 * If the laptop lid is closed, exclude the laptop panel from possible
 * alternatives, except if no other alternatives exist.
 */
static GfMonitor *
find_primary_monitor (GfMonitorManager *monitor_manager,
                      MonitorMatchRule  match_rule)
{
  GfMonitor *monitor;

  monitor = gf_monitor_manager_get_primary_monitor (monitor_manager);

  if (monitor_matches_rule (monitor, monitor_manager, match_rule))
    return monitor;

  monitor = gf_monitor_manager_get_laptop_panel (monitor_manager);

  if (monitor_matches_rule (monitor, monitor_manager, match_rule))
    return monitor;

  monitor = find_monitor_with_highest_preferred_resolution (monitor_manager,
                                                            match_rule);

  if (monitor != NULL)
    return monitor;

  if (match_rule & MONITOR_MATCH_ALLOW_FALLBACK)
    return find_monitor_with_highest_preferred_resolution (monitor_manager,
                                                           MONITOR_MATCH_ALL);

  return NULL;
}

static GfMonitorTransform
get_monitor_transform (GfMonitorManager *monitor_manager,
                       GfMonitor        *monitor)
{
  GfBackend *backend;
  GfOrientationManager *orientation_manager;
  GfOrientation orientation;

  if (!gf_monitor_is_laptop_panel (monitor) ||
      !gf_monitor_manager_get_panel_orientation_managed (monitor_manager))
    return GF_MONITOR_TRANSFORM_NORMAL;

  backend = gf_monitor_manager_get_backend (monitor_manager);
  orientation_manager = gf_backend_get_orientation_manager (backend);
  orientation = gf_orientation_manager_get_orientation (orientation_manager);

  return gf_monitor_transform_from_orientation (orientation);
}

static void
scale_logical_monitor_width (GfLogicalMonitorLayoutMode  layout_mode,
                             float                       scale,
                             int                         mode_width,
                             int                         mode_height,
                             int                        *width,
                             int                        *height)
{
  switch (layout_mode)
    {
      case GF_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
        *width = (int) roundf (mode_width / scale);
        *height = (int) roundf (mode_height / scale);
        return;

      case GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
        *width = mode_width;
        *height = mode_height;
        return;

      default:
        break;
    }

  g_assert_not_reached ();
}

static GfLogicalMonitorConfig *
create_preferred_logical_monitor_config (GfMonitorManager           *monitor_manager,
                                         GfMonitor                  *monitor,
                                         int                         x,
                                         int                         y,
                                         float                       scale,
                                         GfLogicalMonitorLayoutMode  layout_mode)
{
  GfMonitorMode *mode;
  int width, height;
  GfMonitorTransform transform;
  GfMonitorConfig *monitor_config;
  GfLogicalMonitorConfig *logical_monitor_config;

  mode = gf_monitor_get_preferred_mode (monitor);
  gf_monitor_mode_get_resolution (mode, &width, &height);

  scale_logical_monitor_width (layout_mode,
                               scale,
                               width,
                               height,
                               &width,
                               &height);

  monitor_config = gf_monitor_config_new (monitor, mode);
  transform = get_monitor_transform (monitor_manager, monitor);

  if (gf_monitor_transform_is_rotated (transform))
    {
      int temp = width;

      width = height;
      height = temp;
    }

  logical_monitor_config = g_new0 (GfLogicalMonitorConfig, 1);
  *logical_monitor_config = (GfLogicalMonitorConfig) {
    .layout = (GfRectangle) {
      .x = x,
      .y = y,
      .width = width,
      .height = height
    },
    .transform = transform,
    .scale = scale,
    .monitor_configs = g_list_append (NULL, monitor_config)
  };

  return logical_monitor_config;
}

static GfLogicalMonitorConfig *
find_monitor_config (GfMonitorsConfig *config,
                     GfMonitor        *monitor,
                     GfMonitorMode    *monitor_mode)
{
  int mode_width;
  int mode_height;
  GList *l;

  gf_monitor_mode_get_resolution (monitor_mode, &mode_width, &mode_height);

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config;
      GList *l_monitor;

      logical_monitor_config = l->data;

      for (l_monitor = logical_monitor_config->monitor_configs; l_monitor; l_monitor = l_monitor->next)
        {
          GfMonitorConfig *monitor_config;
          GfMonitorModeSpec *mode_spec;

          monitor_config = l_monitor->data;
          mode_spec = gf_monitor_mode_get_spec (monitor_mode);

          if (gf_monitor_spec_equals (gf_monitor_get_spec (monitor), monitor_config->monitor_spec) &&
              gf_monitor_mode_spec_has_similar_size (mode_spec, monitor_config->mode_spec))
            return logical_monitor_config;
        }
    }

  return NULL;
}

static gboolean
get_last_scale_for_monitor (GfMonitorConfigManager *config_manager,
                            GfMonitor              *monitor,
                            GfMonitorMode          *monitor_mode,
                            float                  *out_scale)
{
  GList *configs;
  GList *l;

  configs = NULL;

  if (config_manager->current_config != NULL)
    configs = g_list_append (configs, config_manager->current_config);

  configs = g_list_concat (configs, g_list_copy (config_manager->config_history.head));

  for (l = configs; l; l = l->next)
    {
      GfMonitorsConfig *config;
      GfLogicalMonitorConfig *logical_monitor_config;

      config = l->data;
      logical_monitor_config = find_monitor_config (config,
                                                    monitor,
                                                    monitor_mode);

      if (logical_monitor_config != NULL)
        {
          *out_scale = logical_monitor_config->scale;
          g_list_free (configs);

          return TRUE;
        }
    }

  g_list_free (configs);

  return FALSE;
}


static float
compute_scale_for_monitor (GfMonitorConfigManager *config_manager,
                           GfMonitor              *monitor,
                           GfMonitor              *primary_monitor)

{
  GfMonitorManager *monitor_manager;
  GfMonitor *target_monitor;
  GfMonitorManagerCapability capabilities;
  GfLogicalMonitorLayoutMode layout_mode;
  GfMonitorMode *monitor_mode;
  float scale;

  monitor_manager = config_manager->monitor_manager;
  target_monitor = monitor;
  capabilities = gf_monitor_manager_get_capabilities (monitor_manager);

  if ((capabilities & GF_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED) &&
      primary_monitor != NULL)
    {
      target_monitor = primary_monitor;
    }

  layout_mode = gf_monitor_manager_get_default_layout_mode (monitor_manager);
  monitor_mode = gf_monitor_get_preferred_mode (target_monitor);

  if (get_last_scale_for_monitor (config_manager,
                                  target_monitor,
                                  monitor_mode,
                                  &scale))
    return scale;

  return gf_monitor_manager_calculate_monitor_mode_scale (monitor_manager,
                                                          layout_mode,
                                                          target_monitor,
                                                          monitor_mode);
}

static GfMonitorsConfig *
create_for_switch_config_all_mirror (GfMonitorConfigManager *config_manager)
{
  GfMonitorManager *monitor_manager = config_manager->monitor_manager;
  GfMonitor *primary_monitor;
  GfLogicalMonitorLayoutMode layout_mode;
  GfLogicalMonitorConfig *logical_monitor_config = NULL;
  GList *logical_monitor_configs;
  GList *monitor_configs = NULL;
  gint common_mode_w = 0, common_mode_h = 0;
  float best_scale = 1.0;
  GfMonitor *monitor;
  GList *modes;
  GList *monitors;
  GList *l;
  GfMonitorsConfig *monitors_config;
  int width;
  int height;

  primary_monitor = find_primary_monitor (monitor_manager,
                                          MONITOR_MATCH_ALLOW_FALLBACK);

  if (primary_monitor == NULL)
    return NULL;

  layout_mode = gf_monitor_manager_get_default_layout_mode (monitor_manager);
  monitors = gf_monitor_manager_get_monitors (monitor_manager);
  monitor = monitors->data;
  modes = gf_monitor_get_modes (monitor);
  for (l = modes; l; l = l->next)
    {
      GfMonitorMode *mode = l->data;
      gboolean common_mode_size = TRUE;
      gint mode_w, mode_h;
      GList *ll;

      gf_monitor_mode_get_resolution (mode, &mode_w, &mode_h);

      for (ll = monitors->next; ll; ll = ll->next)
        {
          GfMonitor *monitor_b = ll->data;
          gboolean have_same_mode_size = FALSE;
          GList *mm;

          for (mm = gf_monitor_get_modes (monitor_b); mm; mm = mm->next)
            {
              GfMonitorMode *mode_b = mm->data;
              gint mode_b_w, mode_b_h;

              gf_monitor_mode_get_resolution (mode_b, &mode_b_w, &mode_b_h);

              if (mode_w == mode_b_w && mode_h == mode_b_h)
                {
                  have_same_mode_size = TRUE;
                  break;
                }
            }

          if (!have_same_mode_size)
            {
              common_mode_size = FALSE;
              break;
            }
        }

      if (common_mode_size &&
          common_mode_w * common_mode_h < mode_w * mode_h)
        {
          common_mode_w = mode_w;
          common_mode_h = mode_h;
        }
    }

  if (common_mode_w == 0 || common_mode_h == 0)
    return NULL;

  for (l = monitors; l; l = l->next)
    {
      GfMonitor *l_monitor = l->data;
      GfMonitorMode *mode = NULL;
      GList *ll;
      float scale;

      for (ll = gf_monitor_get_modes (l_monitor); ll; ll = ll->next)
        {
          gint mode_w, mode_h;

          mode = ll->data;
          gf_monitor_mode_get_resolution (mode, &mode_w, &mode_h);

          if (mode_w == common_mode_w && mode_h == common_mode_h)
            break;
        }

      if (!mode)
        continue;

      scale = compute_scale_for_monitor (config_manager,
                                         l_monitor,
                                         primary_monitor);

      best_scale = MAX (best_scale, scale);
      monitor_configs = g_list_prepend (monitor_configs, gf_monitor_config_new (l_monitor, mode));
    }

  scale_logical_monitor_width (layout_mode,
                               best_scale,
                               common_mode_w,
                               common_mode_h,
                               &width,
                               &height);

  logical_monitor_config = g_new0 (GfLogicalMonitorConfig, 1);
  *logical_monitor_config = (GfLogicalMonitorConfig) {
    .layout = (GfRectangle) {
      .x = 0,
      .y = 0,
      .width = width,
      .height = height
    },
    .scale = best_scale,
    .monitor_configs = monitor_configs,
    .is_primary = TRUE
  };

  logical_monitor_configs = g_list_append (NULL, logical_monitor_config);

  monitors_config = gf_monitors_config_new (monitor_manager,
                                            logical_monitor_configs,
                                            layout_mode,
                                            GF_MONITORS_CONFIG_FLAG_NONE);

  if (monitors_config)
    gf_monitors_config_set_switch_config (monitors_config,
                                          GF_MONITOR_SWITCH_CONFIG_ALL_MIRROR);

  return monitors_config;
}

static gboolean
verify_suggested_monitors_config (GList *logical_monitor_configs)
{
  GList *region;
  GList *l;

  region = NULL;

  for (l = logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config = l->data;
      GfRectangle *rect = &logical_monitor_config->layout;

      if (gf_rectangle_overlaps_with_region (region, rect))
        {
          g_warning ("Suggested monitor config has overlapping region, "
                      "rejecting");

          g_list_free (region);

          return FALSE;
        }

      region = g_list_prepend (region, rect);
    }

  for (l = region; region->next && l; l = l->next)
    {
      GfRectangle *rect = l->data;

      if (!gf_rectangle_is_adjacent_to_any_in_region (region, rect))
        {
          g_warning ("Suggested monitor config has monitors with no "
                      "neighbors, rejecting");

          g_list_free (region);

          return FALSE;
        }
    }

  g_list_free (region);

  return TRUE;
}

static GfMonitorsConfig *
create_monitors_config (GfMonitorConfigManager *config_manager,
                        MonitorMatchRule        match_rule,
                        MonitorPositioningMode  positioning,
                        GfMonitorsConfigFlag    config_flags)
{
  GfMonitorManager *monitor_manager = config_manager->monitor_manager;
  GfMonitor *primary_monitor;
  GfLogicalMonitorLayoutMode layout_mode;
  GList *logical_monitor_configs;
  float scale;
  int x, y;
  GList *monitors;
  GList *l;

  primary_monitor = find_primary_monitor (monitor_manager,
                                          match_rule | MONITOR_MATCH_VISIBLE);

  if (!primary_monitor)
    return NULL;

  x = y = 0;
  layout_mode = gf_monitor_manager_get_default_layout_mode (monitor_manager);
  logical_monitor_configs = NULL;

  monitors = NULL;
  if (!(match_rule & MONITOR_MATCH_PRIMARY))
    monitors = find_monitors (monitor_manager, match_rule, primary_monitor);

  /*
   * The primary monitor needs to be at the head of the list for the
   * linear positioning to be correct.
   */
  monitors = g_list_prepend (monitors, primary_monitor);

  for (l = monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;
      GfLogicalMonitorConfig *logical_monitor_config;
      gboolean has_suggested_position;

      switch (positioning)
        {
          case MONITOR_POSITIONING_SUGGESTED:
            has_suggested_position = gf_monitor_get_suggested_position (monitor,
                                                                        &x,
                                                                        &y);
            g_assert (has_suggested_position);
            break;

          case MONITOR_POSITIONING_LINEAR:
          default:
            break;
        }

      scale = compute_scale_for_monitor (config_manager,
                                         monitor,
                                         primary_monitor);

      logical_monitor_config = create_preferred_logical_monitor_config (monitor_manager,
                                                                        monitor,
                                                                        x,
                                                                        y,
                                                                        scale,
                                                                        layout_mode);

      logical_monitor_config->is_primary = (monitor == primary_monitor);
      logical_monitor_configs = g_list_append (logical_monitor_configs, logical_monitor_config);

      x += logical_monitor_config->layout.width;
    }

  g_clear_pointer (&monitors, g_list_free);

  if (positioning == MONITOR_POSITIONING_SUGGESTED)
    {
      if (!verify_suggested_monitors_config (logical_monitor_configs))
        {
          g_list_free_full (logical_monitor_configs,
                            (GDestroyNotify) gf_logical_monitor_config_free);

          return NULL;
        }
    }

  return gf_monitors_config_new (monitor_manager,
                                 logical_monitor_configs,
                                 layout_mode,
                                 config_flags);
}

static GfMonitorsConfig *
create_monitors_switch_config (GfMonitorConfigManager    *config_manager,
                               MonitorMatchRule           match_rule,
                               MonitorPositioningMode     positioning,
                               GfMonitorsConfigFlag       config_flags,
                               GfMonitorSwitchConfigType  switch_config)
{
  GfMonitorsConfig *monitors_config;

  monitors_config = create_monitors_config (config_manager,
                                            match_rule,
                                            positioning,
                                            config_flags);

  if (monitors_config == NULL)
    return NULL;

  gf_monitors_config_set_switch_config (monitors_config, switch_config);

  return monitors_config;
}

static GfMonitorsConfig *
create_for_switch_config_external (GfMonitorConfigManager *config_manager)
{
  return create_monitors_switch_config (config_manager,
                                        MONITOR_MATCH_EXTERNAL,
                                        MONITOR_POSITIONING_LINEAR,
                                        GF_MONITORS_CONFIG_FLAG_NONE,
                                        GF_MONITOR_SWITCH_CONFIG_EXTERNAL);
}

static GfMonitorsConfig *
create_for_switch_config_builtin (GfMonitorConfigManager *config_manager)
{
  return create_monitors_switch_config (config_manager,
                                        MONITOR_MATCH_BUILTIN,
                                        MONITOR_POSITIONING_LINEAR,
                                        GF_MONITORS_CONFIG_FLAG_NONE,
                                        GF_MONITOR_SWITCH_CONFIG_BUILTIN);
}

static GfLogicalMonitorConfig *
find_logical_config_for_builtin_monitor (GfMonitorConfigManager *config_manager,
                                         GList                  *logical_monitor_configs)
{
  GfMonitor *panel;
  GList *l;

  panel = gf_monitor_manager_get_laptop_panel (config_manager->monitor_manager);

  if (panel == NULL)
    return NULL;

  for (l = logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config;
      GfMonitorConfig *monitor_config;

      logical_monitor_config = l->data;

      /*
       * We only want to return the config for the panel if it is
       * configured on its own, so we skip configs which contain clones.
       */
      if (g_list_length (logical_monitor_config->monitor_configs) != 1)
        continue;

      monitor_config = logical_monitor_config->monitor_configs->data;
      if (gf_monitor_spec_equals (gf_monitor_get_spec (panel),
                                  monitor_config->monitor_spec))
        {
          GfMonitorMode *mode;

          mode = gf_monitor_get_mode_from_spec (panel,
                                                monitor_config->mode_spec);

          if (mode != NULL)
            return logical_monitor_config;
        }
    }

  return NULL;
}


static GfMonitorsConfig *
create_for_builtin_display_rotation (GfMonitorConfigManager *config_manager,
                                     GfMonitorsConfig       *base_config,
                                     gboolean                rotate,
                                     GfMonitorTransform      transform)
{
  GfMonitorManager *monitor_manager = config_manager->monitor_manager;
  GfLogicalMonitorConfig *logical_monitor_config;
  GfLogicalMonitorConfig *current_logical_monitor_config;
  GfMonitorsConfig *config;
  GList *logical_monitor_configs;
  GList *current_configs;
  GfLogicalMonitorLayoutMode layout_mode;
  GList *current_monitor_configs;

  g_return_val_if_fail (base_config, NULL);

  current_configs = base_config->logical_monitor_configs;
  current_logical_monitor_config = find_logical_config_for_builtin_monitor (config_manager,
                                                                            current_configs);

  if (!current_logical_monitor_config)
    return NULL;

  if (rotate)
    transform = (current_logical_monitor_config->transform + 1) % GF_MONITOR_TRANSFORM_FLIPPED;
  else
    {
      GfMonitor *panel;

      /*
       * The transform coming from the accelerometer should be applied to
       * the crtc as is, without taking panel-orientation into account, this
       * is done so that non panel-orientation aware desktop environments do the
       * right thing. Mutter corrects for panel-orientation when applying the
       * transform from a logical-monitor-config, so we must convert here.
       */
      panel = gf_monitor_manager_get_laptop_panel (config_manager->monitor_manager);
      transform = gf_monitor_crtc_to_logical_transform (panel, transform);
    }

  if (current_logical_monitor_config->transform == transform)
    return NULL;

  current_monitor_configs = base_config->logical_monitor_configs;
  logical_monitor_configs = gf_clone_logical_monitor_config_list (current_monitor_configs);

  logical_monitor_config = find_logical_config_for_builtin_monitor (config_manager,
                                                                    logical_monitor_configs);
  logical_monitor_config->transform = transform;

  if (gf_monitor_transform_is_rotated (current_logical_monitor_config->transform) !=
      gf_monitor_transform_is_rotated (logical_monitor_config->transform))
    {
      gint temp = logical_monitor_config->layout.width;

      logical_monitor_config->layout.width = logical_monitor_config->layout.height;
      logical_monitor_config->layout.height = temp;
    }

  layout_mode = base_config->layout_mode;

  config = gf_monitors_config_new (monitor_manager,
                                   logical_monitor_configs,
                                   layout_mode,
                                   GF_MONITORS_CONFIG_FLAG_NONE);

  gf_monitors_config_set_parent_config (config, base_config);

  return config;
}

static gboolean
is_crtc_reserved (GfCrtc *crtc,
                  GArray *reserved_crtcs)
{
  unsigned int i;

  for (i = 0; i < reserved_crtcs->len; i++)
    {
       uint64_t id;

       id = g_array_index (reserved_crtcs, uint64_t, i);

       if (id == gf_crtc_get_id (crtc))
         return TRUE;
    }

  return FALSE;
}

static gboolean
is_crtc_assigned (GfCrtc    *crtc,
                  GPtrArray *crtc_assignments)
{
  unsigned int i;

  for (i = 0; i < crtc_assignments->len; i++)
    {
      GfCrtcAssignment *assigned_crtc_assignment;

      assigned_crtc_assignment = g_ptr_array_index (crtc_assignments, i);

      if (assigned_crtc_assignment->crtc == crtc)
        return TRUE;
    }

  return FALSE;
}

static GfCrtc *
find_unassigned_crtc (GfOutput  *output,
                      GPtrArray *crtc_assignments,
                      GArray    *reserved_crtcs)
{
  GfCrtc *crtc;
  const GfOutputInfo *output_info;
  unsigned int i;

  crtc = gf_output_get_assigned_crtc (output);
  if (crtc && !is_crtc_assigned (crtc, crtc_assignments))
    return crtc;

  output_info = gf_output_get_info (output);

  /* then try to assign a CRTC that wasn't used */
  for (i = 0; i < output_info->n_possible_crtcs; i++)
    {
      crtc = output_info->possible_crtcs[i];

      if (is_crtc_assigned (crtc, crtc_assignments))
        continue;

      if (is_crtc_reserved (crtc, reserved_crtcs))
        continue;

      return crtc;
    }

  /* finally just give a CRTC that we haven't assigned */
  for (i = 0; i < output_info->n_possible_crtcs; i++)
    {
      crtc = output_info->possible_crtcs[i];

      if (is_crtc_assigned (crtc, crtc_assignments))
        continue;

      return crtc;
    }

  return NULL;
}

static gboolean
assign_monitor_crtc (GfMonitor          *monitor,
                     GfMonitorMode      *mode,
                     GfMonitorCrtcMode  *monitor_crtc_mode,
                     gpointer            user_data,
                     GError            **error)
{
  MonitorAssignmentData *data = user_data;
  GfOutput *output;
  GfCrtc *crtc;
  GfMonitorTransform transform;
  GfMonitorTransform crtc_transform;
  GfMonitorTransform crtc_hw_transform;
  int crtc_x, crtc_y;
  float x_offset, y_offset;
  float scale;
  float width, height;
  GfCrtcMode *crtc_mode;
  const GfCrtcModeInfo *crtc_mode_info;
  GfRectangle crtc_layout;
  GfCrtcAssignment *crtc_assignment;
  GfOutputAssignment *output_assignment;
  GfMonitorConfig *first_monitor_config;
  gboolean assign_output_as_primary;
  gboolean assign_output_as_presentation;

  output = monitor_crtc_mode->output;
  crtc = find_unassigned_crtc (output,
                               data->crtc_assignments,
                               data->reserved_crtcs);

  if (!crtc)
    {
      GfMonitorSpec *monitor_spec = gf_monitor_get_spec (monitor);

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "No available CRTC for monitor '%s %s' not found",
                   monitor_spec->vendor, monitor_spec->product);

      return FALSE;
    }

  transform = data->logical_monitor_config->transform;
  crtc_transform = gf_monitor_logical_to_crtc_transform (monitor, transform);
  if (gf_monitor_manager_is_transform_handled (data->monitor_manager,
                                               crtc,
                                               crtc_transform))
    crtc_hw_transform = crtc_transform;
  else
    crtc_hw_transform = GF_MONITOR_TRANSFORM_NORMAL;

  gf_monitor_calculate_crtc_pos (monitor, mode, output, crtc_transform,
                                 &crtc_x, &crtc_y);

  x_offset = data->logical_monitor_config->layout.x;
  y_offset = data->logical_monitor_config->layout.y;

  switch (data->config->layout_mode)
    {
      case GF_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
        scale = data->logical_monitor_config->scale;
        break;

      case GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
      default:
        scale = 1.0;
        break;
    }

  crtc_mode = monitor_crtc_mode->crtc_mode;
  crtc_mode_info = gf_crtc_mode_get_info (crtc_mode);

  if (gf_monitor_transform_is_rotated (crtc_transform))
    {
      width = crtc_mode_info->height / scale;
      height = crtc_mode_info->width / scale;
    }
  else
    {
      width = crtc_mode_info->width / scale;
      height = crtc_mode_info->height / scale;
    }

  crtc_layout.x = (int) roundf (x_offset + (crtc_x / scale));
  crtc_layout.y = (int) roundf (y_offset + (crtc_y / scale));
  crtc_layout.width = (int) roundf (width);
  crtc_layout.height = (int) roundf (height);

  crtc_assignment = g_new0 (GfCrtcAssignment, 1);
  *crtc_assignment = (GfCrtcAssignment) {
    .crtc = crtc,
    .mode = crtc_mode,
    .layout = crtc_layout,
    .transform = crtc_hw_transform,
    .outputs = g_ptr_array_new ()
  };
  g_ptr_array_add (crtc_assignment->outputs, output);

  /*
   * Only one output can be marked as primary (due to Xrandr limitation),
   * so only mark the main output of the first monitor in the logical monitor
   * as such.
   */
  first_monitor_config = data->logical_monitor_config->monitor_configs->data;
  if (data->logical_monitor_config->is_primary &&
      data->monitor_config == first_monitor_config &&
      gf_monitor_get_main_output (monitor) == output)
    assign_output_as_primary = TRUE;
  else
    assign_output_as_primary = FALSE;

  if (data->logical_monitor_config->is_presentation)
    assign_output_as_presentation = TRUE;
  else
    assign_output_as_presentation = FALSE;

  output_assignment = g_new0 (GfOutputAssignment, 1);
  *output_assignment = (GfOutputAssignment) {
    .output = output,
    .is_primary = assign_output_as_primary,
    .is_presentation = assign_output_as_presentation,
    .is_underscanning = data->monitor_config->enable_underscanning,
    .has_max_bpc = data->monitor_config->has_max_bpc,
    .max_bpc = data->monitor_config->max_bpc
  };

  g_ptr_array_add (data->crtc_assignments, crtc_assignment);
  g_ptr_array_add (data->output_assignments, output_assignment);

  return TRUE;
}

static gboolean
assign_monitor_crtcs (GfMonitorManager        *manager,
                      GfMonitorsConfig        *config,
                      GfLogicalMonitorConfig  *logical_monitor_config,
                      GfMonitorConfig         *monitor_config,
                      GPtrArray               *crtc_assignments,
                      GPtrArray               *output_assignments,
                      GArray                  *reserved_crtcs,
                      GError                 **error)
{
  GfMonitorSpec *monitor_spec = monitor_config->monitor_spec;
  GfMonitorModeSpec *monitor_mode_spec = monitor_config->mode_spec;
  GfMonitor *monitor;
  GfMonitorMode *monitor_mode;
  MonitorAssignmentData data;

  monitor = gf_monitor_manager_get_monitor_from_spec (manager, monitor_spec);
  if (!monitor)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Configured monitor '%s %s' not found",
                   monitor_spec->vendor, monitor_spec->product);

      return FALSE;
    }

  monitor_mode = gf_monitor_get_mode_from_spec (monitor, monitor_mode_spec);
  if (!monitor_mode)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid mode %dx%d (%.3f) for monitor '%s %s'",
                   monitor_mode_spec->width, monitor_mode_spec->height,
                   (gdouble) monitor_mode_spec->refresh_rate,
                   monitor_spec->vendor, monitor_spec->product);

      return FALSE;
    }

  data = (MonitorAssignmentData) {
    .monitor_manager = manager,
    .config = config,
    .logical_monitor_config = logical_monitor_config,
    .monitor_config = monitor_config,
    .crtc_assignments = crtc_assignments,
    .output_assignments = output_assignments,
    .reserved_crtcs = reserved_crtcs
  };

  if (!gf_monitor_mode_foreach_crtc (monitor, monitor_mode,
                                     assign_monitor_crtc,
                                     &data, error))
    return FALSE;

  return TRUE;
}

static gboolean
assign_logical_monitor_crtcs (GfMonitorManager        *manager,
                              GfMonitorsConfig        *config,
                              GfLogicalMonitorConfig  *logical_monitor_config,
                              GPtrArray               *crtc_assignments,
                              GPtrArray               *output_assignments,
                              GArray                  *reserved_crtcs,
                              GError                 **error)
{
  GList *l;

  for (l = logical_monitor_config->monitor_configs; l; l = l->next)
    {
      GfMonitorConfig *monitor_config = l->data;

      if (!assign_monitor_crtcs (manager,
                                 config,
                                 logical_monitor_config,
                                 monitor_config,
                                 crtc_assignments,
                                 output_assignments,
                                 reserved_crtcs,
                                 error))
        return FALSE;
    }

  return TRUE;
}

GfMonitorsConfigKey *
gf_create_monitors_config_key_for_current_state (GfMonitorManager *monitor_manager)
{
  GfMonitorsConfigKey *config_key;
  GfMonitorSpec *laptop_monitor_spec;
  GList *l;
  GList *monitor_specs;

  laptop_monitor_spec = NULL;
  monitor_specs = NULL;
  for (l = monitor_manager->monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;
      GfMonitorSpec *monitor_spec;

      if (gf_monitor_is_laptop_panel (monitor))
        {
          laptop_monitor_spec = gf_monitor_get_spec (monitor);

          if (is_lid_closed (monitor_manager))
            continue;
        }

      monitor_spec = gf_monitor_spec_clone (gf_monitor_get_spec (monitor));
      monitor_specs = g_list_prepend (monitor_specs, monitor_spec);
    }

  if (!monitor_specs && laptop_monitor_spec)
    {
      monitor_specs =
        g_list_prepend (NULL, gf_monitor_spec_clone (laptop_monitor_spec));
    }

  if (!monitor_specs)
    return NULL;

  monitor_specs = g_list_sort (monitor_specs, (GCompareFunc) gf_monitor_spec_compare);

  config_key = g_new0 (GfMonitorsConfigKey, 1);
  config_key->monitor_specs = monitor_specs;
  config_key->layout_mode = gf_monitor_manager_get_default_layout_mode (monitor_manager);

  return config_key;
}

static void
gf_crtc_assignment_free (GfCrtcAssignment *assignment)
{
  g_ptr_array_free (assignment->outputs, TRUE);
  g_free (assignment);
}

static void
gf_output_assignment_free (GfOutputAssignment *assignment)
{
  g_free (assignment);
}

static void
gf_monitor_config_manager_dispose (GObject *object)
{
  GfMonitorConfigManager *config_manager;

  config_manager = GF_MONITOR_CONFIG_MANAGER (object);

  g_clear_object (&config_manager->current_config);
  gf_monitor_config_manager_clear_history (config_manager);

  G_OBJECT_CLASS (gf_monitor_config_manager_parent_class)->dispose (object);
}

static void
gf_monitor_config_manager_class_init (GfMonitorConfigManagerClass *config_manager_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (config_manager_class);

  object_class->dispose = gf_monitor_config_manager_dispose;
}

static void
gf_monitor_config_manager_init (GfMonitorConfigManager *config_manager)
{
  g_queue_init (&config_manager->config_history);
}

GfMonitorConfigManager *
gf_monitor_config_manager_new (GfMonitorManager *monitor_manager)
{
  GfMonitorConfigManager *config_manager;

  config_manager = g_object_new (GF_TYPE_MONITOR_CONFIG_MANAGER, NULL);
  config_manager->monitor_manager = monitor_manager;
  config_manager->config_store = gf_monitor_config_store_new (monitor_manager);

  return config_manager;
}

GfMonitorConfigStore *
gf_monitor_config_manager_get_store (GfMonitorConfigManager *config_manager)
{
  return config_manager->config_store;
}

gboolean
gf_monitor_config_manager_assign (GfMonitorManager  *manager,
                                  GfMonitorsConfig  *config,
                                  GPtrArray        **out_crtc_assignments,
                                  GPtrArray        **out_output_assignments,
                                  GError           **error)
{
  GPtrArray *crtc_assignments;
  GPtrArray *output_assignments;
  GArray *reserved_crtcs;
  GList *l;

  crtc_assignments = g_ptr_array_new_with_free_func ((GDestroyNotify) gf_crtc_assignment_free);
  output_assignments = g_ptr_array_new_with_free_func ((GDestroyNotify) gf_output_assignment_free);
  reserved_crtcs = g_array_new (FALSE, FALSE, sizeof (uint64_t));

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config = l->data;
      GList *k;

      for (k = logical_monitor_config->monitor_configs; k; k = k->next)
        {
          GfMonitorConfig *monitor_config = k->data;
          GfMonitorSpec *monitor_spec = monitor_config->monitor_spec;
          GfMonitor *monitor;
          GList *o;

          monitor = gf_monitor_manager_get_monitor_from_spec (manager, monitor_spec);

          for (o = gf_monitor_get_outputs (monitor); o; o = o->next)
            {
              GfOutput *output = o->data;
              GfCrtc *crtc;

              crtc = gf_output_get_assigned_crtc (output);
              if (crtc)
                {
                  uint64_t crtc_id;

                  crtc_id = gf_crtc_get_id (crtc);

                  g_array_append_val (reserved_crtcs, crtc_id);
                }
            }
        }
    }

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config = l->data;

      if (!assign_logical_monitor_crtcs (manager,
                                         config,
                                         logical_monitor_config,
                                         crtc_assignments,
                                         output_assignments,
                                         reserved_crtcs,
                                         error))
        {
          g_ptr_array_free (crtc_assignments, TRUE);
          g_ptr_array_free (output_assignments, TRUE);
          g_array_free (reserved_crtcs, TRUE);
          return FALSE;
        }
    }

  *out_crtc_assignments = crtc_assignments;
  *out_output_assignments = output_assignments;

  g_array_free (reserved_crtcs, TRUE);

  return TRUE;
}

GfMonitorsConfig *
gf_monitor_config_manager_get_stored (GfMonitorConfigManager *config_manager)
{
  GfMonitorManager *monitor_manager = config_manager->monitor_manager;
  GfMonitorsConfigKey *config_key;
  GfMonitorsConfig *config;

  config_key = gf_create_monitors_config_key_for_current_state (monitor_manager);
  if (!config_key)
    return NULL;

  config = gf_monitor_config_store_lookup (config_manager->config_store, config_key);
  gf_monitors_config_key_free (config_key);

  return config;
}

GfMonitorsConfig *
gf_monitor_config_manager_create_linear (GfMonitorConfigManager *config_manager)
{
  return create_monitors_config (config_manager,
                                 MONITOR_MATCH_VISIBLE |
                                 MONITOR_MATCH_ALLOW_FALLBACK,
                                 MONITOR_POSITIONING_LINEAR,
                                 GF_MONITORS_CONFIG_FLAG_NONE);
}

GfMonitorsConfig *
gf_monitor_config_manager_create_fallback (GfMonitorConfigManager *config_manager)
{
  return create_monitors_config (config_manager,
                                 MONITOR_MATCH_PRIMARY |
                                 MONITOR_MATCH_ALLOW_FALLBACK,
                                 MONITOR_POSITIONING_LINEAR,
                                 GF_MONITORS_CONFIG_FLAG_NONE);
}

GfMonitorsConfig *
gf_monitor_config_manager_create_suggested (GfMonitorConfigManager *config_manager)
{
  return create_monitors_config (config_manager,
                                 MONITOR_MATCH_WITH_SUGGESTED_POSITION,
                                 MONITOR_POSITIONING_SUGGESTED,
                                 GF_MONITORS_CONFIG_FLAG_NONE);
}

GfMonitorsConfig *
gf_monitor_config_manager_create_for_orientation (GfMonitorConfigManager *config_manager,
                                                  GfMonitorsConfig       *base_config,
                                                  GfMonitorTransform      transform)
{
  return create_for_builtin_display_rotation (config_manager,
                                              base_config,
                                              FALSE,
                                              transform);
}

GfMonitorsConfig *
gf_monitor_config_manager_create_for_builtin_orientation (GfMonitorConfigManager *config_manager,
                                                          GfMonitorsConfig       *base_config)
{
  GfMonitorManager *monitor_manager;
  gboolean panel_orientation_managed;
  GfMonitorTransform current_transform;
  GfMonitor *laptop_panel;

  monitor_manager = config_manager->monitor_manager;
  panel_orientation_managed = gf_monitor_manager_get_panel_orientation_managed (monitor_manager);

  g_return_val_if_fail (panel_orientation_managed, NULL);

  laptop_panel = gf_monitor_manager_get_laptop_panel (monitor_manager);
  current_transform = get_monitor_transform (monitor_manager, laptop_panel);

  return create_for_builtin_display_rotation (config_manager,
                                              base_config,
                                              FALSE,
                                              current_transform);
}

GfMonitorsConfig *
gf_monitor_config_manager_create_for_rotate_monitor (GfMonitorConfigManager *config_manager)
{
  if (config_manager->current_config == NULL)
    return NULL;

  return create_for_builtin_display_rotation (config_manager,
                                              config_manager->current_config,
                                              TRUE,
                                              GF_MONITOR_TRANSFORM_NORMAL);
}

GfMonitorsConfig *
gf_monitor_config_manager_create_for_switch_config (GfMonitorConfigManager    *config_manager,
                                                    GfMonitorSwitchConfigType  config_type)
{
  GfMonitorManager *monitor_manager;
  GfMonitorsConfig *config;

  monitor_manager = config_manager->monitor_manager;
  config = NULL;

  if (!gf_monitor_manager_can_switch_config (monitor_manager))
    return NULL;

  switch (config_type)
    {
      case GF_MONITOR_SWITCH_CONFIG_ALL_MIRROR:
        config = create_for_switch_config_all_mirror (config_manager);
        break;

      case GF_MONITOR_SWITCH_CONFIG_ALL_LINEAR:
        config = gf_monitor_config_manager_create_linear (config_manager);
        break;

      case GF_MONITOR_SWITCH_CONFIG_EXTERNAL:
        config = create_for_switch_config_external (config_manager);
        break;

      case GF_MONITOR_SWITCH_CONFIG_BUILTIN:
        config = create_for_switch_config_builtin (config_manager);
        break;

      case GF_MONITOR_SWITCH_CONFIG_UNKNOWN:
      default:
        g_warn_if_reached ();
        break;
    }

  return config;
}

void
gf_monitor_config_manager_set_current (GfMonitorConfigManager *config_manager,
                                       GfMonitorsConfig       *config)
{
  GfMonitorsConfig *current_config;
  gboolean overrides_current;

  current_config = config_manager->current_config;
  overrides_current = FALSE;

  if (config != NULL &&
      current_config != NULL &&
      has_same_root_config (config, current_config))
    {
      overrides_current = gf_monitors_config_key_equal (config->key,
                                                        current_config->key);
    }

  if (current_config != NULL && !overrides_current)
    {
      g_queue_push_head (&config_manager->config_history,
                         g_object_ref (config_manager->current_config));
      if (g_queue_get_length (&config_manager->config_history) >
          CONFIG_HISTORY_MAX_SIZE)
        g_object_unref (g_queue_pop_tail (&config_manager->config_history));
    }

  g_set_object (&config_manager->current_config, config);
}

GfMonitorsConfig *
gf_monitor_config_manager_get_current (GfMonitorConfigManager *config_manager)
{
  return config_manager->current_config;
}

GfMonitorsConfig *
gf_monitor_config_manager_pop_previous (GfMonitorConfigManager *config_manager)
{
  return g_queue_pop_head (&config_manager->config_history);
}

GfMonitorsConfig *
gf_monitor_config_manager_get_previous (GfMonitorConfigManager *config_manager)
{
  return g_queue_peek_head (&config_manager->config_history);
}

void
gf_monitor_config_manager_clear_history (GfMonitorConfigManager *config_manager)
{
  g_queue_foreach (&config_manager->config_history, history_unref, NULL);
  g_queue_clear (&config_manager->config_history);
}

void
gf_monitor_config_manager_save_current (GfMonitorConfigManager *config_manager)
{
  g_return_if_fail (config_manager->current_config);

  gf_monitor_config_store_add (config_manager->config_store,
                               config_manager->current_config);
}

gboolean
gf_monitor_manager_is_monitor_visible (GfMonitorManager *monitor_manager,
                                       GfMonitor        *monitor)
{
  return monitor_matches_rule (monitor, monitor_manager, MONITOR_MATCH_VISIBLE);
}
