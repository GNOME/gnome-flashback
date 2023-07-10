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

#include "gvc-mixer-control.h"
#ifdef HAVE_GNOME_BLUETOOTH
#include "si-bluetooth.h"
#endif
#include "si-input-sources.h"
#include "si-menu-bar.h"
#include "si-power.h"
#include "si-volume.h"

struct _SiApplet
{
  GpApplet         parent;

  GtkWidget       *menu_bar;

  GvcMixerControl *mixer_control;

#ifdef HAVE_GNOME_BLUETOOTH
  SiIndicator     *bluetooth;
#endif
  SiIndicator     *input_sources;
  SiIndicator     *power;
  SiIndicator     *volume_input;
  SiIndicator     *volume_output;
};

G_DEFINE_TYPE (SiApplet, si_applet, GP_TYPE_APPLET)

static void
append_volume (SiApplet *self)
{
  GtkWidget *item;

  self->volume_input = si_volume_new (GP_APPLET (self),
                                      self->mixer_control,
                                      TRUE);

  item = si_indicator_get_menu_item (self->volume_input);
  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu_bar), item);

  self->volume_output = si_volume_new (GP_APPLET (self),
                                       self->mixer_control,
                                       FALSE);

  item = si_indicator_get_menu_item (self->volume_output);
  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu_bar), item);
}

static void
append_power (SiApplet *self)
{
  GtkWidget *item;

  self->power = si_power_new (GP_APPLET (self));

  item = si_indicator_get_menu_item (self->power);
  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu_bar), item);
}

#ifdef HAVE_GNOME_BLUETOOTH
static void
append_bluetooth (SiApplet *self)
{
  GtkWidget *item;

  self->bluetooth = si_bluetooth_new (GP_APPLET (self));

  item = si_indicator_get_menu_item (self->bluetooth);
  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu_bar), item);
}
#endif

static void
append_input_sources (SiApplet *self)
{
  GtkWidget *item;

  self->input_sources = si_input_sources_new (GP_APPLET (self));

  item = si_indicator_get_menu_item (self->input_sources);
  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu_bar), item);
}

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

  self->mixer_control = gvc_mixer_control_new ("GNOME Flashback Volume Control");
  gvc_mixer_control_open (self->mixer_control);

  append_input_sources (self);
  append_volume (self);
#ifdef HAVE_GNOME_BLUETOOTH
  append_bluetooth (self);
#endif
  append_power (self);
}

static void
si_applet_constructed (GObject *object)
{
  G_OBJECT_CLASS (si_applet_parent_class)->constructed (object);
  setup_applet (SI_APPLET (object));
}

static void
si_applet_dispose (GObject *object)
{
  SiApplet *self;

  self = SI_APPLET (object);

  g_clear_object (&self->mixer_control);

#ifdef HAVE_GNOME_BLUETOOTH
  g_clear_object (&self->bluetooth);
#endif
  g_clear_object (&self->input_sources);
  g_clear_object (&self->power);
  g_clear_object (&self->volume_input);
  g_clear_object (&self->volume_output);

  G_OBJECT_CLASS (si_applet_parent_class)->dispose (object);
}

static void
si_applet_class_init (SiAppletClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = si_applet_constructed;
  object_class->dispose = si_applet_dispose;
}

static void
si_applet_init (SiApplet *self)
{
  GpAppletFlags flags;

  flags = GP_APPLET_FLAGS_EXPAND_MINOR;

  gp_applet_set_flags (GP_APPLET (self), flags);
}
