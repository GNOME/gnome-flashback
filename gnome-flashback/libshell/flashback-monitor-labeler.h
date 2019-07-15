/*
 * Copyright (C) 2015-2019 Alberts Muktupāvels
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

#ifndef FLASHBACK_MONITOR_LABELER_H
#define FLASHBACK_MONITOR_LABELER_H

#include "flashback-shell.h"

G_BEGIN_DECLS

#define FLASHBACK_TYPE_MONITOR_LABELER flashback_monitor_labeler_get_type ()
G_DECLARE_FINAL_TYPE (FlashbackMonitorLabeler, flashback_monitor_labeler,
                      FLASHBACK, MONITOR_LABELER, GObject)

FlashbackMonitorLabeler *flashback_monitor_labeler_new  (void);

void                     flashback_monitor_labeler_show (FlashbackMonitorLabeler *labeler,
                                                         GfMonitorManager        *monitor_manager,
                                                         const gchar             *sender,
                                                         GVariant                *params);

void                     flashback_monitor_labeler_hide (FlashbackMonitorLabeler *labeler,
                                                         const gchar             *sender);

G_END_DECLS

#endif
