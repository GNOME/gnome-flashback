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

#ifndef FLASHBACK_SELECT_AREA_H
#define FLASHBACK_SELECT_AREA_H

#include <glib-object.h>

G_BEGIN_DECLS

#define FLASHBACK_TYPE_SELECT_AREA flashback_select_area_get_type ()
G_DECLARE_FINAL_TYPE (FlashbackSelectArea, flashback_select_area,
                      FLASHBACK, SELECT_AREA, GObject)

FlashbackSelectArea *flashback_select_area_new    (void);

gboolean             flashback_select_area_select (FlashbackSelectArea *select_area,
                                                   gint                *x,
                                                   gint                *y,
                                                   gint                *width,
                                                   gint                *height);

G_END_DECLS

#endif
