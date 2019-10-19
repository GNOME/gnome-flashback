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

struct _GfDesktop
{
  GObject parent;
};

G_DEFINE_TYPE (GfDesktop, gf_desktop, G_TYPE_OBJECT)

static void
gf_desktop_class_init (GfDesktopClass *self_class)
{
}

static void
gf_desktop_init (GfDesktop *self)
{
}

GfDesktop *
gf_desktop_new (void)
{
  return g_object_new (GF_TYPE_DESKTOP, NULL);
}
