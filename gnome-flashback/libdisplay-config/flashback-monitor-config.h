/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2015 Alberts Muktupāvels
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
 * Adapted from mutter 3.16.0:
 * - /src/backends/meta-monitor-config.h
 */

#ifndef FLASHBACK_MONITOR_CONFIG_H
#define FLASHBACK_MONITOR_CONFIG_H

#include "flashback-monitor-manager.h"

G_BEGIN_DECLS

#define FLASHBACK_TYPE_MONITOR_CONFIG flashback_monitor_config_get_type ()
G_DECLARE_FINAL_TYPE (FlashbackMonitorConfig, flashback_monitor_config,
                      FLASHBACK, MONITOR_CONFIG,
                      GObject)

FlashbackMonitorConfig *flashback_monitor_config_new              (FlashbackMonitorManager *manager);

gboolean                flashback_monitor_config_apply_stored     (FlashbackMonitorConfig  *config);

void                    flashback_monitor_config_make_default     (FlashbackMonitorConfig  *config);

void                    flashback_monitor_config_update_current   (FlashbackMonitorConfig  *config);
void                    flashback_monitor_config_make_persistent  (FlashbackMonitorConfig  *config);

void                    flashback_monitor_config_restore_previous (FlashbackMonitorConfig  *config);

G_END_DECLS

#endif
