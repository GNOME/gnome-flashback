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

#ifndef GF_DESKTOP_WINDOW_H
#define GF_DESKTOP_WINDOW_H

#include "backends/gf-monitor-manager.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GF_TYPE_DESKTOP_WINDOW (gf_desktop_window_get_type ())
G_DECLARE_FINAL_TYPE (GfDesktopWindow, gf_desktop_window,
                      GF, DESKTOP_WINDOW, GtkWindow)

GtkWidget *gf_desktop_window_new                 (gboolean           draw_background,
                                                  gboolean           show_icons,
                                                  GError           **error);

void       gf_desktop_window_set_monitor_manager (GfDesktopWindow   *self,
                                                  GfMonitorManager  *monitor_manager);

gboolean   gf_desktop_window_is_ready            (GfDesktopWindow   *self);

int        gf_desktop_window_get_width           (GfDesktopWindow   *self);

int        gf_desktop_window_get_height          (GfDesktopWindow   *self);

G_END_DECLS

#endif
