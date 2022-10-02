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

#include "config.h"
#include "gf-thumbnail-factory.h"

#include <libgnome-desktop/gnome-desktop-thumbnail.h>

typedef struct
{
  GfThumbnailFactory *self;

  char               *uri;
  char               *content_type;
  guint64             time_modified;
} GfLoadData;

struct _GfThumbnailFactory
{
  GObject                       parent;

  GnomeDesktopThumbnailFactory *factory;
};

G_DEFINE_QUARK (gf-thumbnail-error-quark, gf_thumbnail_error)

G_DEFINE_TYPE (GfThumbnailFactory, gf_thumbnail_factory, G_TYPE_OBJECT)

static GfLoadData *
gf_load_data_new (GfThumbnailFactory *self,
                  const char         *uri,
                  const char         *content_type,
                  guint64             time_modified)
{
  GfLoadData *data;

  data = g_new0 (GfLoadData, 1);
  data->self = g_object_ref (self);

  data->uri = g_strdup (uri);
  data->content_type = g_strdup (content_type);
  data->time_modified = time_modified;

  return data;
}

static void
gf_load_data_free (gpointer data)
{
  GfLoadData *load_data;

  load_data = data;

  g_object_unref (load_data->self);
  g_free (load_data->uri);
  g_free (load_data->content_type);
  g_free (load_data);
}

static void
load_icon_in_thread (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  GfLoadData *data;
  char *path;
  GdkPixbuf *pixbuf;

  data = g_task_get_task_data (task);

  if (!gnome_desktop_thumbnail_factory_can_thumbnail (data->self->factory,
                                                      data->uri,
                                                      data->content_type,
                                                      data->time_modified))
    {
      g_task_return_new_error (task,
                               GF_THUMBNAIL_ERROR,
                               GF_THUMBNAIL_ERROR_CAN_NOT_THUMBNAIL,
                               "Can not thumbnail this file");
      return;
    }

  path = gnome_desktop_thumbnail_factory_lookup (data->self->factory,
                                                 data->uri,
                                                 data->time_modified);

  if (path != NULL)
    {
      GFile *file;

      file = g_file_new_for_path (path);
      g_free (path);

      g_task_return_pointer (task, g_file_icon_new (file), g_object_unref);
      g_object_unref (file);
      return;
    }

  if (gnome_desktop_thumbnail_factory_has_valid_failed_thumbnail (data->self->factory,
                                                                  data->uri,
                                                                  data->time_modified))
    {
      g_task_return_new_error (task,
                               GF_THUMBNAIL_ERROR,
                               GF_THUMBNAIL_ERROR_HAS_FAILED_THUMBNAIL,
                               "File has valid failed thumbnail");
      return;
    }

  pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (data->self->factory,
                                                               data->uri,
                                                               data->content_type,
                                                               NULL,
                                                               NULL);

  if (pixbuf != NULL)
    {
      gnome_desktop_thumbnail_factory_save_thumbnail (data->self->factory,
                                                      pixbuf,
                                                      data->uri,
                                                      data->time_modified,
                                                      NULL,
                                                      NULL);

      g_task_return_pointer (task, pixbuf, g_object_unref);
      return;
    }

  gnome_desktop_thumbnail_factory_create_failed_thumbnail (data->self->factory,
                                                           data->uri,
                                                           data->time_modified,
                                                           NULL,
                                                           NULL);

  g_task_return_new_error (task,
                           GF_THUMBNAIL_ERROR,
                           GF_THUMBNAIL_ERROR_GENERATION_FAILED,
                           "Thumbnail generation failed");
}

static void
gf_thumbnail_factory_dispose (GObject *object)
{
  GfThumbnailFactory *self;

  self = GF_THUMBNAIL_FACTORY (object);

  g_clear_object (&self->factory);

  G_OBJECT_CLASS (gf_thumbnail_factory_parent_class)->dispose (object);
}

static void
gf_thumbnail_factory_class_init (GfThumbnailFactoryClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_thumbnail_factory_dispose;
}

static void
gf_thumbnail_factory_init (GfThumbnailFactory *self)
{
  GnomeDesktopThumbnailSize thumbnail_size;

  thumbnail_size = GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE;
  self->factory = gnome_desktop_thumbnail_factory_new (thumbnail_size);
}

GfThumbnailFactory *
gf_thumbnail_factory_new (void)
{
  return g_object_new (GF_TYPE_THUMBNAIL_FACTORY,
                       NULL);
}

void
gf_thumbnail_factory_load_async (GfThumbnailFactory  *self,
                                 const char          *uri,
                                 const char          *content_type,
                                 guint64              time_modified,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GTask *task;
  GfLoadData *data;

  task = g_task_new (self, cancellable, callback, user_data);

  data = gf_load_data_new (self, uri, content_type, time_modified);
  g_task_set_task_data (task, data, gf_load_data_free);

  g_task_run_in_thread (task, load_icon_in_thread);
  g_object_unref (task);
}

GIcon *
gf_thumbnail_factory_load_finish (GfThumbnailFactory  *self,
                                  GAsyncResult        *result,
                                  GError             **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
