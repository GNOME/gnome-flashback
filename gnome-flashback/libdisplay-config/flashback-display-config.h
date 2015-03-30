/*
 * Copyright (C) 2014 Alberts Muktupāvels
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
 */

#ifndef FLASHBACK_DISPLAY_CONFIG_H
#define FLASHBACK_DISPLAY_CONFIG_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _FlashbackMonitorManager FlashbackMonitorManager;

#define FLASHBACK_TYPE_DISPLAY_CONFIG flashback_display_config_get_type ()
G_DECLARE_FINAL_TYPE (FlashbackDisplayConfig, flashback_display_config,
                      FLASHBACK, DISPLAY_CONFIG,
                      GObject)

FlashbackDisplayConfig  *flashback_display_config_new                 (void);

FlashbackMonitorManager *flashback_display_config_get_monitor_manager (FlashbackDisplayConfig *config);

G_END_DECLS

#endif
