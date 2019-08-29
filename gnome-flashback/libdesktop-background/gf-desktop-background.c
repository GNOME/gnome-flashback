/*
 * Copyright (C) 2014 - 2016 Alberts Muktupāvels
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

#include <gtk/gtk.h>
#include <libgnome-desktop/gnome-bg.h>

#include "gf-desktop-background.h"

struct _GfDesktopBackground
{
  GObject           parent;

  gulong            monitors_changed_id;
  gulong            size_changed_id;
  gulong            change_event_id;

  guint             redraw_idle_id;

  GnomeBG          *bg;
  GnomeBGCrossfade *fade;

  GSettings        *settings;
};

G_DEFINE_TYPE (GfDesktopBackground, gf_desktop_background, G_TYPE_OBJECT)

static void
fade_finished_cb (GfDesktopBackground *background)
{
  g_clear_object (&background->fade);
}

static void
draw_background (GfDesktopBackground *background,
                 gboolean             fade)
{
  GdkScreen *screen;
  GdkWindow *root;
  gint width;
  gint height;
  cairo_surface_t *surface;

  screen = gdk_screen_get_default ();
  root = gdk_screen_get_root_window (screen);
  width = gdk_window_get_width (root);
  height = gdk_window_get_height (root);

  surface = gnome_bg_create_surface (background->bg, root, width, height, TRUE);

  if (fade)
    {
      if (background->fade != NULL)
        g_object_unref (background->fade);

      background->fade = gnome_bg_set_surface_as_root_with_crossfade (screen,
                                                                      surface);

      g_signal_connect_swapped (background->fade, "finished",
                                G_CALLBACK (fade_finished_cb), background);
    }
  else
    {
      gnome_bg_set_surface_as_root (screen, surface);
    }

  cairo_surface_destroy (surface);
}

static gboolean
change_event_cb (GSettings           *settings,
                 gpointer             keys,
                 gint                 n_keys,
                 GfDesktopBackground *background)
{
  gnome_bg_load_from_preferences (background->bg, background->settings);

  return TRUE;
}

static void
changed_cb (GnomeBG             *bg,
            GfDesktopBackground *background)
{
  GSettings *settings;
  gboolean fade;

  settings = g_settings_new ("org.gnome.gnome-flashback.desktop-background");
  fade = g_settings_get_boolean (settings, "fade");

  draw_background (background, fade);
  g_object_unref (settings);
}

static void
transitioned_cb (GnomeBG             *bg,
                 GfDesktopBackground *background)
{
  draw_background (background, FALSE);
}

static gboolean
redraw_cb (gpointer user_data)
{
  GfDesktopBackground *background;

  background = GF_DESKTOP_BACKGROUND (user_data);

  draw_background (background, FALSE);

  background->redraw_idle_id = 0;
  return G_SOURCE_REMOVE;
}

static void
queue_redraw (GfDesktopBackground *background)
{
  if (background->redraw_idle_id != 0)
    return;

  background->redraw_idle_id = g_idle_add (redraw_cb, background);
}

static void
monitors_changed_cb (GdkScreen           *screen,
                     GfDesktopBackground *background)
{
  queue_redraw (background);
}

static void
size_changed_cb (GdkScreen           *screen,
                 GfDesktopBackground *background)
{
  queue_redraw (background);
}

static void
gf_desktop_background_dispose (GObject *object)
{
  GfDesktopBackground *background;
  GdkScreen *screen;

  background = GF_DESKTOP_BACKGROUND (object);
  screen = gdk_screen_get_default ();

  if (background->monitors_changed_id != 0)
    {
      g_signal_handler_disconnect (screen, background->monitors_changed_id);
      background->monitors_changed_id = 0;
    }

  if (background->size_changed_id != 0)
    {
      g_signal_handler_disconnect (screen, background->size_changed_id);
      background->size_changed_id = 0;
    }

  if (background->change_event_id != 0)
    {
      g_signal_handler_disconnect (background->settings, background->change_event_id);
      background->change_event_id = 0;
    }

  if (background->redraw_idle_id != 0)
    {
      g_source_remove (background->redraw_idle_id);
      background->redraw_idle_id = 0;
    }

  g_clear_object (&background->bg);
  g_clear_object (&background->fade);
  g_clear_object (&background->settings);

  G_OBJECT_CLASS (gf_desktop_background_parent_class)->dispose (object);
}

static void
gf_desktop_background_class_init (GfDesktopBackgroundClass *background_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (background_class);

  object_class->dispose = gf_desktop_background_dispose;
}

static void
gf_desktop_background_init (GfDesktopBackground *background)
{
  GdkScreen *screen;

  screen = gdk_screen_get_default ();

  background->monitors_changed_id =
    g_signal_connect (screen, "monitors-changed",
                      G_CALLBACK (monitors_changed_cb), background);

  background->size_changed_id =
    g_signal_connect (screen, "size-changed",
                      G_CALLBACK (size_changed_cb), background);

  background->bg = gnome_bg_new ();

  g_signal_connect (background->bg, "changed",
                    G_CALLBACK (changed_cb), background);

  g_signal_connect (background->bg, "transitioned",
                    G_CALLBACK (transitioned_cb), background);

  background->settings = g_settings_new ("org.gnome.desktop.background");

  background->change_event_id =
    g_signal_connect (background->settings, "change-event",
                      G_CALLBACK (change_event_cb), background);

  gnome_bg_load_from_preferences (background->bg, background->settings);
}

GfDesktopBackground *
gf_desktop_background_new (void)
{
  return g_object_new (GF_TYPE_DESKTOP_BACKGROUND, NULL);
}
