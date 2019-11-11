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

#ifndef GF_ICON_H
#define GF_ICON_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
  GF_ICON_SELECTED_NONE = 0,
  GF_ICON_SELECTED_CLEAR = (1 << 0),
  GF_ICON_SELECTED_ADD = (1 << 1),
  GF_ICON_SELECTED_REMOVE = (1 << 2)
} GfIconSelectedFlags;

#define GF_TYPE_ICON (gf_icon_get_type ())
G_DECLARE_FINAL_TYPE (GfIcon, gf_icon, GF, ICON, GtkButton)

GtkWidget *gf_icon_new          (GFile               *file,
                                 GFileInfo           *info);

GFile     *gf_icon_get_file     (GfIcon              *self);

void       gf_icon_set_selected (GfIcon              *self,
                                 gboolean             selected,
                                 GfIconSelectedFlags  flags);

gboolean   gf_icon_get_selected (GfIcon              *self);

G_END_DECLS

#endif
