/*
 * Copyright (C) 2016 Alberts Muktupāvels
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

#ifndef SN_WATCHER_PRIVATE_H
#define SN_WATCHER_PRIVATE_H

#include <glib.h>

G_BEGIN_DECLS

#define SN_WATCHER_V0_BUS_NAME "org.kde.StatusNotifierWatcher"
#define SN_WATCHER_V0_OBJECT_PATH "/StatusNotifierWatcher"

#define SN_WATCHER_V1_BUS_NAME "org.freedesktop.StatusNotifier1.Watcher"
#define SN_WATCHER_V1_OBJECT_PATH "/org/freedesktop/StatusNotifier1/Watcher"

G_END_DECLS

#endif
