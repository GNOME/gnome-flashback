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
#include "gf-home-icon.h"

#include <glib/gi18n.h>

struct _GfHomeIcon
{
  GfIcon parent;
};

G_DEFINE_TYPE (GfHomeIcon, gf_home_icon, GF_TYPE_ICON)

static GIcon *
gf_home_icon_get_icon (GfIcon   *icon,
                       gboolean *is_thumbnail)
{
  GFileInfo *info;

  info = gf_icon_get_file_info (icon);
  *is_thumbnail = FALSE;

  return g_file_info_get_icon (info);
}

static const char *
gf_home_icon_get_text (GfIcon *icon)
{
  return _("Home");
}

static gboolean
gf_home_icon_can_delete (GfIcon *icon)
{
  return FALSE;
}

static gboolean
gf_home_icon_can_rename (GfIcon *icon)
{
  return FALSE;
}

static void
gf_home_icon_class_init (GfHomeIconClass *self_class)
{
  GfIconClass *icon_class;

  icon_class = GF_ICON_CLASS (self_class);

  icon_class->get_icon = gf_home_icon_get_icon;
  icon_class->get_text = gf_home_icon_get_text;
  icon_class->can_delete = gf_home_icon_can_delete;
  icon_class->can_rename = gf_home_icon_can_rename;
}

static void
gf_home_icon_init (GfHomeIcon *self)
{
}

GtkWidget *
gf_home_icon_new (GfIconView  *icon_view,
                  GError     **error)
{
  GFile *file;
  char *attributes;
  GFileQueryInfoFlags flags;
  GFileInfo *info;
  GtkWidget *widget;

  file = g_file_new_for_path (g_get_home_dir ());

  attributes = gf_icon_view_get_file_attributes (icon_view);
  flags = G_FILE_QUERY_INFO_NONE;

  info = g_file_query_info (file, attributes, flags, NULL, error);
  g_free (attributes);

  if (info == NULL)
    {
      g_object_unref (file);
      return NULL;
    }

  widget = g_object_new (GF_TYPE_HOME_ICON,
                         "icon-view", icon_view,
                         "file", file,
                         "info", info,
                         NULL);

  g_object_unref (file);
  g_object_unref (info);

  return widget;
}
