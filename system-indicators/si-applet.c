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
#include "si-applet.h"

#include "si-menu-bar.h"

struct _SiApplet
{
  GpApplet   parent;

  GtkWidget *menu_bar;
};

G_DEFINE_TYPE (SiApplet, si_applet, GP_TYPE_APPLET)

static void
setup_applet (SiApplet *self)
{
  self->menu_bar = si_menu_bar_new ();
  gtk_container_add (GTK_CONTAINER (self), self->menu_bar);
  gtk_widget_show (self->menu_bar);

  g_object_bind_property (self,
                          "enable-tooltips",
                          self->menu_bar,
                          "enable-tooltips",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (self,
                          "position",
                          self->menu_bar,
                          "position",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);
}

static void
si_applet_constructed (GObject *object)
{
  G_OBJECT_CLASS (si_applet_parent_class)->constructed (object);
  setup_applet (SI_APPLET (object));
}

static void
si_applet_class_init (SiAppletClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = si_applet_constructed;
}

static void
si_applet_init (SiApplet *self)
{
  GpAppletFlags flags;

  flags = GP_APPLET_FLAGS_EXPAND_MINOR;

  gp_applet_set_flags (GP_APPLET (self), flags);
}
