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
#include "gf-dummy-icon.h"

struct _GfDummyIcon
{
  GfIcon parent;
};

G_DEFINE_TYPE (GfDummyIcon, gf_dummy_icon, GF_TYPE_ICON)

static void
gf_dummy_icon_class_init (GfDummyIconClass *self_class)
{
}

static void
gf_dummy_icon_init (GfDummyIcon *self)
{
}

GtkWidget *
gf_dummy_icon_new (void)
{
  GFile *file;
  GFileInfo *info;
  GIcon *icon;
  const char *name;
  GtkWidget *widget;

  file = g_file_new_for_commandline_arg ("");
  info = g_file_info_new ();

  icon = g_icon_new_for_string ("text-x-generic", NULL);
  g_file_info_set_icon (info, icon);
  g_object_unref (icon);

  name = "Lorem Ipsum is simply dummy text of the printing and typesetting "
         "industry. Lorem Ipsum has been the industry's standard dummy text "
         "ever since the 1500s, when an unknown printer took a galley of "
         "type and scrambled it to make a type specimen book.";

  g_file_info_set_display_name (info, name);

  widget = g_object_new (GF_TYPE_DUMMY_ICON,
                         "file", file,
                         "info", info,
                         NULL);

  g_object_unref (file);
  g_object_unref (info);

  return widget;
}
