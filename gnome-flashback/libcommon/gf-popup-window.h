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

#ifndef GF_POPUP_WINDOW_H
#define GF_POPUP_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GF_TYPE_POPUP_WINDOW gf_popup_window_get_type ()
G_DECLARE_DERIVABLE_TYPE (GfPopupWindow, gf_popup_window,
                          GF, POPUP_WINDOW, GtkWindow)

struct _GfPopupWindowClass
{
  GtkWindowClass parent_class;
};

void gf_popup_window_fade_start  (GfPopupWindow *window);
void gf_popup_window_fade_cancel (GfPopupWindow *window);

G_END_DECLS

#endif
