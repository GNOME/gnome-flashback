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

#ifndef GF_LABEL_WINDOW_H
#define GF_LABEL_WINDOW_H

#include <libcommon/gf-popup-window.h>

G_BEGIN_DECLS

#define GF_TYPE_LABEL_WINDOW gf_label_window_get_type ()
G_DECLARE_FINAL_TYPE (GfLabelWindow, gf_label_window,
                      GF, LABEL_WINDOW, GfPopupWindow)

GfLabelWindow *gf_label_window_new  (gint           monitor,
                                     const gchar   *label);

void           gf_label_window_show (GfLabelWindow *window);
void           gf_label_window_hide (GfLabelWindow *window);

G_END_DECLS

#endif
