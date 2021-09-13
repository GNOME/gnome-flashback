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
 * - src/backends/meta-monitor-config-manager.h
 */

#ifndef GF_MONITOR_CONFIG_MANAGER_PRIVATE_H
#define GF_MONITOR_CONFIG_MANAGER_PRIVATE_H

#include "gf-logical-monitor-config-private.h"
#include "gf-monitor-config-private.h"
#include "gf-monitor-manager-private.h"
#include "gf-monitors-config-private.h"

G_BEGIN_DECLS

#define GF_TYPE_MONITOR_CONFIG_MANAGER (gf_monitor_config_manager_get_type ())
G_DECLARE_FINAL_TYPE (GfMonitorConfigManager, gf_monitor_config_manager,
                      GF, MONITOR_CONFIG_MANAGER, GObject)

GfMonitorConfigManager *gf_monitor_config_manager_new                       (GfMonitorManager            *monitor_manager);

GfMonitorConfigStore   *gf_monitor_config_manager_get_store                 (GfMonitorConfigManager      *config_manager);

gboolean                gf_monitor_config_manager_assign                    (GfMonitorManager            *manager,
                                                                             GfMonitorsConfig            *config,
                                                                             GPtrArray                  **crtc_assignments,
                                                                             GPtrArray                  **output_assignments,
                                                                             GError                     **error);

GfMonitorsConfig       *gf_monitor_config_manager_get_stored                (GfMonitorConfigManager      *config_manager);

GfMonitorsConfig       *gf_monitor_config_manager_create_linear             (GfMonitorConfigManager      *config_manager);

GfMonitorsConfig       *gf_monitor_config_manager_create_fallback           (GfMonitorConfigManager      *config_manager);

GfMonitorsConfig       *gf_monitor_config_manager_create_suggested          (GfMonitorConfigManager      *config_manager);

GfMonitorsConfig       *gf_monitor_config_manager_create_for_orientation    (GfMonitorConfigManager      *config_manager,
                                                                             GfMonitorsConfig            *base_config,
                                                                             GfMonitorTransform           transform);

GfMonitorsConfig       *gf_monitor_config_manager_create_for_builtin_orientation (GfMonitorConfigManager *config_manager,
                                                                                  GfMonitorsConfig       *base_config);

GfMonitorsConfig       *gf_monitor_config_manager_create_for_rotate_monitor (GfMonitorConfigManager      *config_manager);

GfMonitorsConfig       *gf_monitor_config_manager_create_for_switch_config  (GfMonitorConfigManager      *config_manager,
                                                                             GfMonitorSwitchConfigType    config_type);

void                    gf_monitor_config_manager_set_current               (GfMonitorConfigManager      *config_manager,
                                                                             GfMonitorsConfig            *config);

GfMonitorsConfig       *gf_monitor_config_manager_get_current               (GfMonitorConfigManager      *config_manager);

GfMonitorsConfig       *gf_monitor_config_manager_pop_previous              (GfMonitorConfigManager      *config_manager);

GfMonitorsConfig       *gf_monitor_config_manager_get_previous              (GfMonitorConfigManager      *config_manager);

void                    gf_monitor_config_manager_clear_history             (GfMonitorConfigManager      *config_manager);

void                    gf_monitor_config_manager_save_current              (GfMonitorConfigManager      *config_manager);

GfMonitorsConfigKey    *gf_create_monitors_config_key_for_current_state     (GfMonitorManager            *monitor_manager);

G_END_DECLS

#endif
