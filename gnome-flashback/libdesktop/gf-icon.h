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

#include "gf-icon-view.h"

G_BEGIN_DECLS

#define GF_TYPE_ICON (gf_icon_get_type ())
G_DECLARE_DERIVABLE_TYPE (GfIcon, gf_icon, GF, ICON, GtkButton)

struct _GfIconClass
{
  GtkButtonClass parent_class;
};

GtkWidget  *gf_icon_new               (GfIconView *icon_view,
                                       GFile      *file,
                                       GFileInfo  *info);

GFile      *gf_icon_get_file          (GfIcon     *self);

const char *gf_icon_get_name          (GfIcon     *self);

const char *gf_icon_get_name_collated (GfIcon     *self);

GFileType   gf_icon_get_file_type     (GfIcon     *self);

guint64     gf_icon_get_time_modified (GfIcon     *self);

guint64     gf_icon_get_size          (GfIcon     *self);

gboolean    gf_icon_is_hidden         (GfIcon     *self);

void        gf_icon_set_selected      (GfIcon     *self,
                                       gboolean    selected);

gboolean    gf_icon_get_selected      (GfIcon     *self);

G_END_DECLS

#endif
