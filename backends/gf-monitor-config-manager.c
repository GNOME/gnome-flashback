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

#include <math.h>

#include "gf-crtc-private.h"
#include "gf-monitor-config-store-private.h"
#include "gf-monitor-config-manager-private.h"
#include "gf-monitor-config-migration-private.h"
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
  MONITOR_MATCH_EXTERNAL = (1 << 0)
} MonitorMatchRule;

struct _GfMonitorConfigManager
{
  GObject               parent;

  GfMonitorManager     *monitor_manager;

  GfMonitorConfigStore *config_store;

  GfMonitorsConfig     *current_config;
  GQueue                config_history;
};

G_DEFINE_TYPE (GfMonitorConfigManager, gf_monitor_config_manager, G_TYPE_OBJECT)

static void
history_unref (gpointer data,
               gpointer user_data)
{
  g_object_unref (data);
}

static GfMonitor *
find_monitor_with_highest_preferred_resolution (GfMonitorManager *monitor_manager,
                                                MonitorMatchRule  match_rule)
{
  GList *monitors;
  GList *l;
  int largest_area = 0;
  GfMonitor *largest_monitor = NULL;

  monitors = gf_monitor_manager_get_monitors (monitor_manager);
  for (l = monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;
      GfMonitorMode *mode;
      int width, height;
      int area;

      if (match_rule & MONITOR_MATCH_EXTERNAL)
        {
          if (gf_monitor_is_laptop_panel (monitor))
            continue;
        }

      mode = gf_monitor_get_preferred_mode (monitor);
      gf_monitor_mode_get_resolution (mode, &width, &height);
      area = width * height;

      if (area > largest_area)
        {
          largest_area = area;
          largest_monitor = monitor;
        }
    }

  return largest_monitor;
}

static gboolean
is_lid_closed (GfMonitorManager *monitor_manager)
{
  GfBackend *backend;

  backend = gf_monitor_manager_get_backend (monitor_manager);

  return gf_backend_is_lid_closed (backend);
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
find_primary_monitor (GfMonitorManager *monitor_manager)
{
  GfMonitor *monitor;

  if (is_lid_closed (monitor_manager))
    {
      monitor = gf_monitor_manager_get_primary_monitor (monitor_manager);
      if (monitor && !gf_monitor_is_laptop_panel (monitor))
        return monitor;

      monitor = find_monitor_with_highest_preferred_resolution (monitor_manager,
                                                                MONITOR_MATCH_EXTERNAL);

      if (monitor)
        return monitor;

      return find_monitor_with_highest_preferred_resolution (monitor_manager,
                                                             MONITOR_MATCH_ALL);
    }
  else
    {
      monitor = gf_monitor_manager_get_primary_monitor (monitor_manager);
      if (monitor)
        return monitor;

      monitor = gf_monitor_manager_get_laptop_panel (monitor_manager);
      if (monitor)
        return monitor;

      return find_monitor_with_highest_preferred_resolution (monitor_manager,
                                                             MONITOR_MATCH_ALL);
    }
}

static GfMonitorTransform
get_monitor_transform (GfMonitorManager *monitor_manager,
                       GfMonitor        *monitor)
{
  GfBackend *backend;
  GfOrientationManager *orientation_manager;

  if (!gf_monitor_is_laptop_panel (monitor))
    return GF_MONITOR_TRANSFORM_NORMAL;

  backend = gf_monitor_manager_get_backend (monitor_manager);
  orientation_manager = gf_backend_get_orientation_manager (backend);

  switch (gf_orientation_manager_get_orientation (orientation_manager))
    {
      case GF_ORIENTATION_BOTTOM_UP:
        return GF_MONITOR_TRANSFORM_180;

      case GF_ORIENTATION_LEFT_UP:
        return GF_MONITOR_TRANSFORM_90;

      case GF_ORIENTATION_RIGHT_UP:
        return GF_MONITOR_TRANSFORM_270;

      case GF_ORIENTATION_UNDEFINED:
      case GF_ORIENTATION_NORMAL:
      default:
        return GF_MONITOR_TRANSFORM_NORMAL;
    }
}

static GfLogicalMonitorConfig *
create_preferred_logical_monitor_config (GfMonitorManager           *monitor_manager,
                                         GfMonitor                  *monitor,
                                         int                         x,
                                         int                         y,
                                         GfLogicalMonitorConfig     *primary_logical_monitor_config,
                                         GfLogicalMonitorLayoutMode  layout_mode)
{
  GfMonitorMode *mode;
  int width, height;
  float scale;
  GfMonitorTransform transform;
  GfMonitorConfig *monitor_config;
  GfLogicalMonitorConfig *logical_monitor_config;

  mode = gf_monitor_get_preferred_mode (monitor);
  gf_monitor_mode_get_resolution (mode, &width, &height);

  if ((gf_monitor_manager_get_capabilities (monitor_manager) &
       GF_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED) &&
      primary_logical_monitor_config)
    scale = primary_logical_monitor_config->scale;
  else
    scale = gf_monitor_manager_calculate_monitor_mode_scale (monitor_manager,
                                                             layout_mode,
                                                             monitor, mode);

  switch (layout_mode)
    {
      case GF_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
        width = (int) roundf (width / scale);
        height = (int) roundf (height / scale);
        break;

      case GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
      default:
        break;
    }

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

static GfMonitorsConfig *
create_for_switch_config_all_mirror (GfMonitorConfigManager *config_manager)
{
  GfMonitorManager *monitor_manager = config_manager->monitor_manager;
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

      scale = gf_monitor_manager_calculate_monitor_mode_scale (monitor_manager,
                                                               layout_mode,
                                                               l_monitor,
                                                               mode);

      best_scale = MAX (best_scale, scale);
      monitor_configs = g_list_prepend (monitor_configs, gf_monitor_config_new (l_monitor, mode));
    }

  logical_monitor_config = g_new0 (GfLogicalMonitorConfig, 1);
  *logical_monitor_config = (GfLogicalMonitorConfig) {
    .layout = (GfRectangle) {
      .x = 0,
      .y = 0,
      .width = common_mode_w,
      .height = common_mode_h
    },
    .scale = best_scale,
    .monitor_configs = monitor_configs
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

static GfMonitorsConfig *
create_for_switch_config_external (GfMonitorConfigManager *config_manager)
{
  GfMonitorManager *monitor_manager = config_manager->monitor_manager;
  GList *logical_monitor_configs = NULL;
  int x = 0;
  GfLogicalMonitorLayoutMode layout_mode;
  GList *monitors;
  GList *l;
  GfMonitorsConfig *monitors_config;

  layout_mode = gf_monitor_manager_get_default_layout_mode (monitor_manager);

  monitors = gf_monitor_manager_get_monitors (monitor_manager);
  for (l = monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;
      GfLogicalMonitorConfig *logical_monitor_config;

      if (gf_monitor_is_laptop_panel (monitor))
        continue;

      logical_monitor_config = create_preferred_logical_monitor_config (monitor_manager,
                                                                        monitor, x, 0, NULL,
                                                                        layout_mode);

      logical_monitor_configs = g_list_append (logical_monitor_configs,
                                               logical_monitor_config);

      if (x == 0)
        logical_monitor_config->is_primary = TRUE;

      x += logical_monitor_config->layout.width;
    }

  monitors_config = gf_monitors_config_new (monitor_manager,
                                            logical_monitor_configs,
                                            layout_mode,
                                            GF_MONITORS_CONFIG_FLAG_NONE);

  if (monitors_config)
    gf_monitors_config_set_switch_config (monitors_config,
                                          GF_MONITOR_SWITCH_CONFIG_EXTERNAL);

  return monitors_config;
}

static GfMonitorsConfig *
create_for_switch_config_builtin (GfMonitorConfigManager *config_manager)
{
  GfMonitorManager *monitor_manager = config_manager->monitor_manager;
  GfLogicalMonitorLayoutMode layout_mode;
  GList *logical_monitor_configs;
  GfLogicalMonitorConfig *primary_logical_monitor_config;
  GfMonitor *monitor;
  GfMonitorsConfig *monitors_config;

  monitor = gf_monitor_manager_get_laptop_panel (monitor_manager);
  if (!monitor)
    return NULL;

  layout_mode = gf_monitor_manager_get_default_layout_mode (monitor_manager);

  primary_logical_monitor_config = create_preferred_logical_monitor_config (monitor_manager,
                                                                            monitor, 0, 0, NULL,
                                                                            layout_mode);

  primary_logical_monitor_config->is_primary = TRUE;
  logical_monitor_configs = g_list_append (NULL, primary_logical_monitor_config);

  monitors_config = gf_monitors_config_new (monitor_manager,
                                            logical_monitor_configs,
                                            layout_mode,
                                            GF_MONITORS_CONFIG_FLAG_NONE);

  if (monitors_config)
    gf_monitors_config_set_switch_config (monitors_config,
                                          GF_MONITOR_SWITCH_CONFIG_BUILTIN);

  return monitors_config;
}

static GList *
clone_monitor_config_list (GList *configs_in)
{
  GList *configs_out;
  GList *l;

  configs_out = NULL;

  for (l = configs_in; l != NULL; l = l->next)
    {
      GfMonitorConfig *config_in;
      GfMonitorConfig *config_out;

      config_in = l->data;

      config_out = g_new0 (GfMonitorConfig, 1);
      *config_out = (GfMonitorConfig) {
        .monitor_spec = gf_monitor_spec_clone (config_in->monitor_spec),
        .mode_spec = g_memdup2 (config_in->mode_spec, sizeof (GfMonitorModeSpec)),
        .enable_underscanning = config_in->enable_underscanning
      };

      configs_out = g_list_append (configs_out, config_out);
    }

  return configs_out;
}

static GList *
clone_logical_monitor_config_list (GList *configs_in)
{
  GList *configs_out;
  GList *l;

  configs_out = NULL;

  for (l = configs_in; l != NULL; l = l->next)
    {
      GfLogicalMonitorConfig *config_in;
      GfLogicalMonitorConfig *config_out;
      GList *config_list;

      config_in = l->data;

      config_out = g_memdup2 (config_in, sizeof (GfLogicalMonitorConfig));

      config_list = clone_monitor_config_list (config_in->monitor_configs);
      config_out->monitor_configs = config_list;

      configs_out = g_list_append (configs_out, config_out);
    }

  return configs_out;
}

static GfLogicalMonitorConfig *
find_logical_config_for_builtin_display_rotation (GfMonitorConfigManager *config_manager,
                                                  GList                  *logical_monitor_configs)
{
  GfMonitor *panel;
  GList *l;

  panel = gf_monitor_manager_get_laptop_panel (config_manager->monitor_manager);

  if (panel == NULL || !gf_monitor_is_active (panel))
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
        return logical_monitor_config;
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
  GList *logical_monitor_configs;
  GList *current_configs;
  GfLogicalMonitorLayoutMode layout_mode;
  GList *current_monitor_configs;

  g_return_val_if_fail (base_config, NULL);

  current_configs = base_config->logical_monitor_configs;
  current_logical_monitor_config = find_logical_config_for_builtin_display_rotation (config_manager,
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
  logical_monitor_configs = clone_logical_monitor_config_list (current_monitor_configs);

  logical_monitor_config = find_logical_config_for_builtin_display_rotation (config_manager,
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

  return gf_monitors_config_new (monitor_manager, logical_monitor_configs,
                                 layout_mode, GF_MONITORS_CONFIG_FLAG_NONE);
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
    .is_underscanning = data->monitor_config->enable_underscanning
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
                   "Invalid mode %dx%d (%f) for monitor '%s %s'",
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
  GError *error = NULL;

  config_key = gf_create_monitors_config_key_for_current_state (monitor_manager);
  if (!config_key)
    return NULL;

  config = gf_monitor_config_store_lookup (config_manager->config_store, config_key);
  gf_monitors_config_key_free (config_key);

  if (!config)
    return NULL;

  if (config->flags & GF_MONITORS_CONFIG_FLAG_MIGRATED)
    {
      if (!gf_finish_monitors_config_migration (monitor_manager, config, &error))
        {
          g_warning ("Failed to finish monitors config migration: %s", error->message);
          g_error_free (error);

          gf_monitor_config_store_remove (config_manager->config_store, config);
          return NULL;
        }
    }

  return config;
}

GfMonitorsConfig *
gf_monitor_config_manager_create_linear (GfMonitorConfigManager *config_manager)
{
  GfMonitorManager *monitor_manager = config_manager->monitor_manager;
  GList *logical_monitor_configs;
  GfMonitor *primary_monitor;
  GfLogicalMonitorLayoutMode layout_mode;
  GfLogicalMonitorConfig *primary_logical_monitor_config;
  int x;
  GList *monitors;
  GList *l;
  GfMonitorsConfig *monitors_config;

  primary_monitor = find_primary_monitor (monitor_manager);
  if (!primary_monitor)
    return NULL;

  layout_mode = gf_monitor_manager_get_default_layout_mode (monitor_manager);

  primary_logical_monitor_config = create_preferred_logical_monitor_config (monitor_manager,
                                                                            primary_monitor,
                                                                            0, 0, NULL,
                                                                            layout_mode);

  primary_logical_monitor_config->is_primary = TRUE;
  logical_monitor_configs = g_list_append (NULL, primary_logical_monitor_config);

  x = primary_logical_monitor_config->layout.width;
  monitors = gf_monitor_manager_get_monitors (monitor_manager);
  for (l = monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;
      GfLogicalMonitorConfig *logical_monitor_config;

      if (monitor == primary_monitor)
        continue;

      if (gf_monitor_is_laptop_panel (monitor) &&
          is_lid_closed (monitor_manager))
        continue;

      logical_monitor_config = create_preferred_logical_monitor_config (monitor_manager,
                                                                        monitor, x, 0,
                                                                        primary_logical_monitor_config,
                                                                        layout_mode);

      logical_monitor_configs = g_list_append (logical_monitor_configs,
                                               logical_monitor_config);

      x += logical_monitor_config->layout.width;
    }

  monitors_config = gf_monitors_config_new (monitor_manager,
                                            logical_monitor_configs,
                                            layout_mode,
                                            GF_MONITORS_CONFIG_FLAG_NONE);

  if (monitors_config)
    gf_monitors_config_set_switch_config (monitors_config,
                                          GF_MONITOR_SWITCH_CONFIG_ALL_LINEAR);

  return monitors_config;
}

GfMonitorsConfig *
gf_monitor_config_manager_create_fallback (GfMonitorConfigManager *config_manager)
{
  GfMonitorManager *monitor_manager = config_manager->monitor_manager;
  GfMonitor *primary_monitor;
  GList *logical_monitor_configs;
  GfLogicalMonitorLayoutMode layout_mode;
  GfLogicalMonitorConfig *primary_logical_monitor_config;

  primary_monitor = find_primary_monitor (monitor_manager);
  if (!primary_monitor)
    return NULL;

  layout_mode = gf_monitor_manager_get_default_layout_mode (monitor_manager);

  primary_logical_monitor_config = create_preferred_logical_monitor_config (monitor_manager,
                                                                            primary_monitor,
                                                                            0, 0, NULL,
                                                                            layout_mode);

  primary_logical_monitor_config->is_primary = TRUE;
  logical_monitor_configs = g_list_append (NULL, primary_logical_monitor_config);

  return gf_monitors_config_new (monitor_manager, logical_monitor_configs,
                                 layout_mode, GF_MONITORS_CONFIG_FLAG_NONE);
}

GfMonitorsConfig *
gf_monitor_config_manager_create_suggested (GfMonitorConfigManager *config_manager)
{
  GfMonitorManager *monitor_manager = config_manager->monitor_manager;
  GfLogicalMonitorConfig *primary_logical_monitor_config = NULL;
  GfMonitor *primary_monitor;
  GfLogicalMonitorLayoutMode layout_mode;
  GList *logical_monitor_configs;
  GList *region;
  int x, y;
  GList *monitors;
  GList *l;

  primary_monitor = find_primary_monitor (monitor_manager);
  if (!primary_monitor)
    return NULL;

  if (!gf_monitor_get_suggested_position (primary_monitor, &x, &y))
    return NULL;

  layout_mode = gf_monitor_manager_get_default_layout_mode (monitor_manager);

  primary_logical_monitor_config = create_preferred_logical_monitor_config (monitor_manager,
                                                                            primary_monitor,
                                                                            x, y, NULL,
                                                                            layout_mode);

  primary_logical_monitor_config->is_primary = TRUE;
  logical_monitor_configs = g_list_append (NULL, primary_logical_monitor_config);
  region = g_list_prepend (NULL, &primary_logical_monitor_config->layout);

  monitors = gf_monitor_manager_get_monitors (monitor_manager);
  for (l = monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;
      GfLogicalMonitorConfig *logical_monitor_config;

      if (monitor == primary_monitor)
        continue;

      if (!gf_monitor_get_suggested_position (monitor, &x, &y))
        continue;

      logical_monitor_config = create_preferred_logical_monitor_config (monitor_manager,
                                                                        monitor, x, y,
                                                                        primary_logical_monitor_config,
                                                                        layout_mode);

      logical_monitor_configs = g_list_append (logical_monitor_configs, logical_monitor_config);

      if (gf_rectangle_overlaps_with_region (region, &logical_monitor_config->layout))
        {
          g_warning ("Suggested monitor config has overlapping region, rejecting");
          g_list_free (region);
          g_list_free_full (logical_monitor_configs,
                            (GDestroyNotify) gf_logical_monitor_config_free);

          return NULL;
        }

      region = g_list_prepend (region, &logical_monitor_config->layout);
    }

  g_list_free (region);

  if (!logical_monitor_configs)
    return NULL;

  return gf_monitors_config_new (monitor_manager, logical_monitor_configs,
                                 layout_mode, GF_MONITORS_CONFIG_FLAG_NONE);
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
  if (config_manager->current_config)
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
