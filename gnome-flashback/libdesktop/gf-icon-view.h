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

#ifndef GF_ICON_VIEW_H
#define GF_ICON_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GF_TYPE_ICON_VIEW (gf_icon_view_get_type ())
G_DECLARE_FINAL_TYPE (GfIconView, gf_icon_view, GF, ICON_VIEW, GtkFixed)

GtkWidget *gf_icon_view_new (void);

G_END_DECLS

#endif