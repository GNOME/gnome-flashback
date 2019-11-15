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
  GfIcon parent;
};

G_DEFINE_TYPE (GfTrashIcon, gf_trash_icon, GF_TYPE_ICON)

static void
gf_trash_icon_class_init (GfTrashIconClass *self_class)
{
}

static void
gf_trash_icon_init (GfTrashIcon *self)
{
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
