/*
 * Copyright (C) 2019 Alberts Muktupāvels
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

#ifndef GF_MONITOR_VIEW_H
#define GF_MONITOR_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GF_TYPE_MONITOR_VIEW (gf_monitor_view_get_type ())
G_DECLARE_FINAL_TYPE (GfMonitorView, gf_monitor_view, GF, MONITOR_VIEW, GtkFixed)

GtkWidget  *gf_monitor_view_new         (GdkMonitor    *monitor,
                                         guint          icon_size);

GdkMonitor *gf_monitor_view_get_monitor (GfMonitorView *self);

G_END_DECLS

#endif