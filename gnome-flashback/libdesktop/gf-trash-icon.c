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
#include "gf-trash-icon.h"

struct _GfTrashIcon
{
  GfIcon        parent;

  GCancellable *cancellable;

  GFileMonitor *monitor;

  gboolean      empty;
};

G_DEFINE_TYPE (GfTrashIcon, gf_trash_icon, GF_TYPE_ICON)

static void
next_files_cb (GObject      *object,
               GAsyncResult *res,
               gpointer      user_data)
{
  GFileEnumerator *enumerator;
  GList *files;
  GError *error;
  GfTrashIcon *self;

  enumerator = G_FILE_ENUMERATOR (object);

  error = NULL;
  files = g_file_enumerator_next_files_finish (enumerator, res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_TRASH_ICON (user_data);
  self->empty = files == NULL;

  g_list_free_full (files, g_object_unref);
}

static void
enumerate_children_cb (GObject      *object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GFileEnumerator *enumerator;
  GError *error;
  GfTrashIcon *self;

  error = NULL;
  enumerator = g_file_enumerate_children_finish (G_FILE (object), res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_TRASH_ICON (user_data);
  self->empty = TRUE;

  g_file_enumerator_next_files_async (enumerator,
                                      1,
                                      G_PRIORITY_LOW,
                                      self->cancellable,
                                      next_files_cb,
                                      user_data);

  g_object_unref (enumerator);
}

static void
check_if_empty (GfTrashIcon *self)
{
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();

  g_file_enumerate_children_async (gf_icon_get_file (GF_ICON (self)),
                                   NULL,
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_LOW,
                                   self->cancellable,
                                   enumerate_children_cb,
                                   self);
}

static void
trash_changed_cb (GFileMonitor      *monitor,
                  GFile             *file,
                  GFile             *other_file,
                  GFileMonitorEvent  event_type,
                  GfTrashIcon       *self)
{
  switch (event_type)
    {
      case G_FILE_MONITOR_EVENT_DELETED:
      case G_FILE_MONITOR_EVENT_CREATED:
      case G_FILE_MONITOR_EVENT_MOVED_IN:
      case G_FILE_MONITOR_EVENT_MOVED_OUT:
        gf_icon_update (GF_ICON (self));
        check_if_empty (self);
        break;

      case G_FILE_MONITOR_EVENT_CHANGED:
      case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
      case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
      case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
      case G_FILE_MONITOR_EVENT_UNMOUNTED:
      case G_FILE_MONITOR_EVENT_MOVED:
      case G_FILE_MONITOR_EVENT_RENAMED:
      default:
        break;
    }
}

static void
gf_trash_icon_constructed (GObject *object)
{
  GfTrashIcon *self;

  self = GF_TRASH_ICON (object);

  G_OBJECT_CLASS (gf_trash_icon_parent_class)->constructed (object);

  check_if_empty (self);
}

static void
gf_trash_icon_dispose (GObject *object)
{
  GfTrashIcon *self;

  self = GF_TRASH_ICON (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (gf_trash_icon_parent_class)->dispose (object);
}

static void
gf_trash_icon_create_file_monitor (GfIcon *icon)
{
  GfTrashIcon *self;
  GError *error;

  self = GF_TRASH_ICON (icon);

  g_clear_object (&self->monitor);

  error = NULL;
  self->monitor = g_file_monitor_directory (gf_icon_get_file (GF_ICON (self)),
                                            G_FILE_MONITOR_WATCH_MOVES,
                                            self->cancellable,
                                            &error);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (self->monitor,
                    "changed",
                    G_CALLBACK (trash_changed_cb),
                    self);
}

static GIcon *
gf_trash_icon_get_icon (GfIcon   *icon,
                        gboolean *is_thumbnail)
{
  GFileInfo *info;

  info = gf_icon_get_file_info (icon);
  *is_thumbnail = FALSE;

  return g_file_info_get_icon (info);
}

static gboolean
gf_trash_icon_can_delete (GfIcon *icon)
{
  return FALSE;
}

static gboolean
gf_trash_icon_can_rename (GfIcon *icon)
{
  return FALSE;
}

static void
gf_trash_icon_class_init (GfTrashIconClass *self_class)
{
  GObjectClass *object_class;
  GfIconClass *icon_class;

  object_class = G_OBJECT_CLASS (self_class);
  icon_class = GF_ICON_CLASS (self_class);

  object_class->constructed = gf_trash_icon_constructed;
  object_class->dispose = gf_trash_icon_dispose;

  icon_class->create_file_monitor = gf_trash_icon_create_file_monitor;
  icon_class->get_icon = gf_trash_icon_get_icon;
  icon_class->can_delete = gf_trash_icon_can_delete;
  icon_class->can_rename = gf_trash_icon_can_rename;
}

static void
gf_trash_icon_init (GfTrashIcon *self)
{
  self->cancellable = g_cancellable_new ();
  self->empty = TRUE;
}

GtkWidget *
gf_trash_icon_new (GfIconView  *icon_view,
                   GError     **error)
{
  GFile *file;
  char *attributes;
  GFileQueryInfoFlags flags;
  GFileInfo *info;
  GtkWidget *widget;

  file = g_file_new_for_uri ("trash:///");

  attributes = gf_icon_view_get_file_attributes (icon_view);
  flags = G_FILE_QUERY_INFO_NONE;

  info = g_file_query_info (file, attributes, flags, NULL, error);
  g_free (attributes);

  if (info == NULL)
    {
      g_object_unref (file);
      return NULL;
    }

  widget = g_object_new (GF_TYPE_TRASH_ICON,
                         "icon-view", icon_view,
                         "file", file,
                         "info", info,
                         NULL);

  g_object_unref (file);
  g_object_unref (info);

  return widget;
}

gboolean
gf_trash_icon_is_empty (GfTrashIcon *self)
{
  return self->empty;
}
