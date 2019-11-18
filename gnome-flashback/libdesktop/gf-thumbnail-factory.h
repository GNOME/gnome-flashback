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

#ifndef GF_THUMBNAIL_FACTORY_H
#define GF_THUMBNAIL_FACTORY_H

#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum
{
  GF_THUMBNAIL_ERROR_CAN_NOT_THUMBNAIL,
  GF_THUMBNAIL_ERROR_HAS_FAILED_THUMBNAIL,
  GF_THUMBNAIL_ERROR_GENERATION_FAILED
} GfThumbnailError;

#define GF_THUMBNAIL_ERROR (gf_thumbnail_error_quark ())
GQuark gf_thumbnail_error_quark (void);

#define GF_TYPE_THUMBNAIL_FACTORY (gf_thumbnail_factory_get_type ())
G_DECLARE_FINAL_TYPE (GfThumbnailFactory, gf_thumbnail_factory,
                      GF, THUMBNAIL_FACTORY, GObject)

GfThumbnailFactory *gf_thumbnail_factory_new         (void);

void                gf_thumbnail_factory_load_async  (GfThumbnailFactory   *self,
                                                      const char           *uri,
                                                      const char           *content_type,
                                                      guint64               time_modified,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);

GIcon              *gf_thumbnail_factory_load_finish (GfThumbnailFactory   *self,
                                                      GAsyncResult         *result,
                                                      GError              **error);

G_END_DECLS

#endif
