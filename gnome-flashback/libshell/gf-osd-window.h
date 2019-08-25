/*
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
 */

#ifndef GF_OSD_WINDOW_H
#define GF_OSD_WINDOW_H

#include <libcommon/gf-popup-window.h>

G_BEGIN_DECLS

#define GF_TYPE_OSD_WINDOW gf_osd_window_get_type ()
G_DECLARE_FINAL_TYPE (GfOsdWindow, gf_osd_window,
                      GF, OSD_WINDOW, GfPopupWindow)

GfOsdWindow *gf_osd_window_new           (gint         monitor);

void         gf_osd_window_set_icon      (GfOsdWindow *window,
                                          GIcon       *icon);

void         gf_osd_window_set_label     (GfOsdWindow *window,
                                          const gchar *label);

void         gf_osd_window_set_level     (GfOsdWindow *window,
                                          gdouble      level);

void         gf_osd_window_set_max_level (GfOsdWindow *window,
                                          gdouble      max_level);

void         gf_osd_window_show          (GfOsdWindow *window);

void         gf_osd_window_hide          (GfOsdWindow *window);

G_END_DECLS

#endif
