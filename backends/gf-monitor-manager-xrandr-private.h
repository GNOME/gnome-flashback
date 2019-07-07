/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
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
 * - src/backends/x11/meta-monitor-manager-xrandr.h
 */

#ifndef GF_MONITOR_MANAGER_XRANDR_PRIVATE_H
#define GF_MONITOR_MANAGER_XRANDR_PRIVATE_H

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "gf-monitor-manager-private.h"

G_BEGIN_DECLS

#define GF_TYPE_MONITOR_MANAGER_XRANDR (gf_monitor_manager_xrandr_get_type ())
G_DECLARE_FINAL_TYPE (GfMonitorManagerXrandr, gf_monitor_manager_xrandr,
                      GF, MONITOR_MANAGER_XRANDR, GfMonitorManager)

Display  *gf_monitor_manager_xrandr_get_xdisplay  (GfMonitorManagerXrandr *xrandr);

gboolean  gf_monitor_manager_xrandr_has_randr15   (GfMonitorManagerXrandr *xrandr);

gboolean  gf_monitor_manager_xrandr_handle_xevent (GfMonitorManagerXrandr *xrandr,
                                                   XEvent                 *event);

G_END_DECLS

#endif
