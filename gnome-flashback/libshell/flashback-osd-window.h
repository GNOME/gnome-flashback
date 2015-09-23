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

#ifndef FLASHBACK_OSD_WINDOW_H
#define FLASHBACK_OSD_WINDOW_H

#include <libcommon/gf-popup-window.h>

G_BEGIN_DECLS

#define FLASHBACK_TYPE_OSD_WINDOW flashback_osd_window_get_type ()
G_DECLARE_FINAL_TYPE (FlashbackOsdWindow, flashback_osd_window,
                      FLASHBACK, OSD_WINDOW, GfPopupWindow)

FlashbackOsdWindow *flashback_osd_window_new       (gint                monitor);

void                flashback_osd_window_set_icon  (FlashbackOsdWindow *window,
                                                    GIcon              *icon);
void                flashback_osd_window_set_label (FlashbackOsdWindow *window,
                                                    const gchar        *label);
void                flashback_osd_window_set_level (FlashbackOsdWindow *window,
                                                    gint                level);

void                flashback_osd_window_show      (FlashbackOsdWindow *window);
void                flashback_osd_window_hide      (FlashbackOsdWindow *window);

G_END_DECLS

#endif
