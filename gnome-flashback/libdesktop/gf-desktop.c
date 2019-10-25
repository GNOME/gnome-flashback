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
#include "gf-desktop.h"

#include <gio/gio.h>

struct _GfDesktop
{
  GObject    parent;

  GSettings *settings;
};

G_DEFINE_TYPE (GfDesktop, gf_desktop, G_TYPE_OBJECT)

static void
gf_desktop_dispose (GObject *object)
{
  GfDesktop *self;

  self = GF_DESKTOP (object);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gf_desktop_parent_class)->dispose (object);
}

static void
gf_desktop_class_init (GfDesktopClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_desktop_dispose;
}

static void
gf_desktop_init (GfDesktop *self)
{
  self->settings = g_settings_new ("org.gnome.gnome-flashback.desktop");
}

GfDesktop *
gf_desktop_new (void)
{
  return g_object_new (GF_TYPE_DESKTOP, NULL);
}
