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
#include "gf-thumbnail-factory.h"

G_BEGIN_DECLS

#define GF_TYPE_ICON_VIEW (gf_icon_view_get_type ())
G_DECLARE_FINAL_TYPE (GfIconView, gf_icon_view, GF, ICON_VIEW, GtkEventBox)

GtkWidget          *gf_icon_view_new                      (void);

GfThumbnailFactory *gf_icon_view_get_thumbnail_factory    (GfIconView          *self);

char               *gf_icon_view_get_file_attributes      (GfIconView          *self);

char               *gf_icon_view_get_desktop_uri          (GfIconView          *self);

void                gf_icon_view_set_representative_color (GfIconView          *self,
                                                           GdkRGBA             *color);

void                gf_icon_view_set_drag_rectangles      (GfIconView          *self,
                                                           GPtrArray           *rectangles);

void                gf_icon_view_clear_selection          (GfIconView          *self);

GList              *gf_icon_view_get_selected_icons       (GfIconView          *self);

void                gf_icon_view_show_item_properties     (GfIconView          *self,
                                                           const char * const  *uris);

void                gf_icon_view_empty_trash              (GfIconView          *self,
                                                           guint32              timestamp);

gboolean            gf_icon_view_validate_new_name        (GfIconView          *self,
                                                           GFileType            file_type,
                                                           const char          *new_name,
                                                           char               **message);

void                gf_icon_view_move_to_trash            (GfIconView          *self,
                                                           const char * const  *uris,
                                                           guint32              timestamp);

void                gf_icon_view_delete                   (GfIconView          *self,
                                                           const char * const  *uris,
                                                           guint32              timestamp);

void                gf_icon_view_rename_file              (GfIconView          *self,
                                                           const char          *uri,
                                                           const char          *new_name,
                                                           guint32              timestamp);

void                gf_icon_view_copy_uris                (GfIconView          *self,
                                                           const char * const  *uris,
                                                           const char          *destination,
                                                           guint32              timestamp);

void                gf_icon_view_move_uris                (GfIconView          *self,
                                                           const char * const  *uris,
                                                           const char          *destination,
                                                           guint32              timestamp);

G_END_DECLS

#endif
