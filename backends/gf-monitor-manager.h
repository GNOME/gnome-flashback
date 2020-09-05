/*
 * Copyright (C) 2015 Red Hat
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
 * - src/meta/meta-monitor-manager.h
 */

#ifndef GF_MONITOR_MANAGER_H
#define GF_MONITOR_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
  GF_MONITOR_SWITCH_CONFIG_ALL_MIRROR,
  GF_MONITOR_SWITCH_CONFIG_ALL_LINEAR,
  GF_MONITOR_SWITCH_CONFIG_EXTERNAL,
  GF_MONITOR_SWITCH_CONFIG_BUILTIN,
  GF_MONITOR_SWITCH_CONFIG_UNKNOWN,
} GfMonitorSwitchConfigType;

typedef struct _GfMonitorManager GfMonitorManager;

gint                      gf_monitor_manager_get_monitor_for_connector         (GfMonitorManager          *manager,
                                                                                const gchar               *connector);

gboolean                  gf_monitor_manager_get_is_builtin_display_on         (GfMonitorManager          *manager);

GfMonitorSwitchConfigType gf_monitor_manager_get_switch_config                 (GfMonitorManager          *manager);

gboolean                  gf_monitor_manager_can_switch_config                 (GfMonitorManager          *manager);

void                      gf_monitor_manager_switch_config                     (GfMonitorManager          *manager,
                                                                                GfMonitorSwitchConfigType  config_type);

gint                      gf_monitor_manager_get_display_configuration_timeout (void);

gboolean                  gf_monitor_manager_get_panel_orientation_managed     (GfMonitorManager          *self);

void                      gf_monitor_manager_confirm_configuration             (GfMonitorManager          *manager,
                                                                                gboolean                   ok);

G_END_DECLS

#endif
