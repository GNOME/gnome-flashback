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

#include "gf-desktop-window.h"

struct _GfDesktop
{
  GObject    parent;

  GSettings *settings;

  GtkWidget *window;
};

G_DEFINE_TYPE (GfDesktop, gf_desktop, G_TYPE_OBJECT)

static void
ready_cb (GfDesktopWindow *window,
          GfDesktop       *self)
{
  gtk_widget_show (self->window);
}

static void
gf_desktop_dispose (GObject *object)
{
  GfDesktop *self;

  self = GF_DESKTOP (object);

  g_clear_object (&self->settings);
  g_clear_pointer (&self->window, gtk_widget_destroy);

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
  gboolean draw_background;
  gboolean show_icons;
  GError *error;

  self->settings = g_settings_new ("org.gnome.gnome-flashback.desktop");

  draw_background = g_settings_get_boolean (self->settings, "draw-background");
  show_icons = g_settings_get_boolean (self->settings, "show-icons");

  error = NULL;
  self->window = gf_desktop_window_new (draw_background, show_icons, &error);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }

  g_settings_bind (self->settings, "draw-background",
                   self->window, "draw-background",
                   G_SETTINGS_BIND_GET);

  g_settings_bind (self->settings, "show-icons",
                   self->window, "show-icons",
                   G_SETTINGS_BIND_GET);

  if (!gf_desktop_window_is_ready (GF_DESKTOP_WINDOW (self->window)))
    g_signal_connect (self->window, "ready", G_CALLBACK (ready_cb), self);
  else
    gtk_widget_show (self->window);
}

GfDesktop *
gf_desktop_new (void)
{
  return g_object_new (GF_TYPE_DESKTOP, NULL);
}

void
gf_desktop_set_monitor_manager (GfDesktop        *self,
                                GfMonitorManager *monitor_manager)
{
  if (self->window == NULL)
    return;

  gf_desktop_window_set_monitor_manager (GF_DESKTOP_WINDOW (self->window),
                                         monitor_manager);
}
