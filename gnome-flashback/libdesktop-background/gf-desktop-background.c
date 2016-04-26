/*
 * Copyright (C) 2014 - 2015 Alberts Muktupāvels
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

#include "gf-background-window.h"
#include "gf-desktop-background.h"

#define DESKTOP_BG "org.gnome.desktop.background"
#define GNOME_FLASHBACK_BG "org.gnome.gnome-flashback.desktop-background"

struct _GfDesktopBackground
{
  GObject           parent;

  GnomeBG          *bg;
  GnomeBGCrossfade *fade;

  GSettings        *gnome_settings;
  GSettings        *background_settings;

  GtkWidget        *background;

  cairo_surface_t  *surface;
  gint              width;
  gint              height;

  guint             change_idle_id;
};

G_DEFINE_TYPE (GfDesktopBackground, gf_desktop_background, G_TYPE_OBJECT)

static void
free_fade (GfDesktopBackground *background)
{
  g_clear_object (&background->fade);
}

static void
free_surface (GfDesktopBackground *background)
{
  if (background->surface == NULL)
    return;

  cairo_surface_destroy (background->surface);
  background->surface = NULL;
}

static void
background_unrealize (GfDesktopBackground *background)
{
  free_surface (background);

  background->width = 0;
  background->height = 0;
}

static void
init_fade (GfDesktopBackground *background)
{
  GdkScreen *screen;
  gboolean fade;

  screen = gdk_screen_get_default ();
  fade = g_settings_get_boolean (background->background_settings, "fade");

  if (!fade)
    return;

  if (background->fade == NULL)
    {
      GdkWindow *window;
      gint window_width;
      gint window_height;
      gint screen_width;
      gint screen_height;

      window = gtk_widget_get_window (background->background);
      window_width = gdk_window_get_width (window);
      window_height = gdk_window_get_height (window);
      screen_width = gdk_screen_get_width (screen);
      screen_height = gdk_screen_get_height (screen);

      if (window_width == screen_width && window_height == screen_height)
        {
          background->fade = gnome_bg_crossfade_new (window_width,
                                                     window_height);

          g_signal_connect_swapped (background->fade, "finished",
                                    G_CALLBACK (free_fade), background);
        }
    }

  if (background->fade != NULL &&
      !gnome_bg_crossfade_is_started (background->fade))
    {
      cairo_surface_t *surface;

      if (background->surface == NULL)
        surface = gnome_bg_get_surface_from_root (screen);
      else
        surface = cairo_surface_reference (background->surface);

      gnome_bg_crossfade_set_start_surface (background->fade, surface);
      cairo_surface_destroy (surface);
    }
}

static void
background_ensure_realized (GfDesktopBackground *background)
{
  GdkScreen *screen;
  gint width;
  gint height;
  GdkWindow *window;

  screen = gdk_screen_get_default ();
  width = gdk_screen_get_width (screen);
  height = gdk_screen_get_height (screen);
  window = gtk_widget_get_window (background->background);

  if (width == background->width && height == background->height)
    return;

  free_surface (background);

  background->surface = gnome_bg_create_surface (background->bg, window,
                                                 width, height, TRUE);

  background->width = width;
  background->height = height;
}

static void
on_fade_finished (GnomeBGCrossfade *fade,
                  GdkWindow        *window,
                  gpointer          user_data)
{
  GfDesktopBackground *background;
  GdkScreen *screen;

  background = GF_DESKTOP_BACKGROUND (user_data);
  screen = gdk_window_get_screen (window);

  background_ensure_realized (background);

  if (background->surface != NULL)
    gnome_bg_set_surface_as_root (screen, background->surface);
}

static gboolean
fade_to_surface (GfDesktopBackground *background,
                 GdkWindow           *window,
                 cairo_surface_t     *surface)
{
  if (background->fade == NULL)
    return FALSE;

  if (!gnome_bg_crossfade_set_end_surface (background->fade, surface))
    return FALSE;

  if (!gnome_bg_crossfade_is_started (background->fade))
    {
      gnome_bg_crossfade_start (background->fade, window);

      g_signal_connect (background->fade, "finished",
                        G_CALLBACK (on_fade_finished), background);
    }

	return gnome_bg_crossfade_is_started (background->fade);
}

static void
background_set_up (GfDesktopBackground *background)
{
  GdkWindow *window;

  background_ensure_realized (background);

  if (background->surface == NULL)
    return;

  window = gtk_widget_get_window (background->background);

  if (!fade_to_surface (background, window, background->surface))
    {
      GdkScreen *screen;
      cairo_pattern_t *pattern;

      screen = gdk_screen_get_default ();
      pattern = cairo_pattern_create_for_surface (background->surface);

      gdk_window_set_background_pattern (window, pattern);
      cairo_pattern_destroy (pattern);

      gnome_bg_set_surface_as_root (screen, background->surface);
    }
}

static gboolean
background_changed_cb (gpointer user_data)
{
  GfDesktopBackground *background;

  background = GF_DESKTOP_BACKGROUND (user_data);

  background_unrealize (background);
  background_set_up (background);

  gtk_widget_show (background->background);
  gtk_widget_queue_draw (background->background);

  background->change_idle_id = 0;
  return G_SOURCE_REMOVE;
}

static void
queue_background_change (GfDesktopBackground *background)
{
  if (background->change_idle_id != 0)
    g_source_remove (background->change_idle_id);

  background->change_idle_id = g_idle_add (background_changed_cb, background);
}

static void
desktop_background_changed (GnomeBG  *bg,
                            gpointer  user_data)
{
  GfDesktopBackground *background;

  background = GF_DESKTOP_BACKGROUND (user_data);

  init_fade (background);
  queue_background_change (background);
}

static void
desktop_background_transitioned (GnomeBG  *bg,
                                 gpointer  user_data)
{
  GfDesktopBackground *background;

  background = GF_DESKTOP_BACKGROUND (user_data);

  init_fade (background);
  queue_background_change (background);
}

static gboolean
desktop_background_change_event (GSettings *settings,
                                 gpointer   keys,
                                 gint       n_keys,
                                 gpointer   user_data)
{
  GfDesktopBackground *background;

  background = GF_DESKTOP_BACKGROUND (user_data);

  gnome_bg_load_from_preferences (background->bg, background->gnome_settings);

  return TRUE;
}

static void
size_allocate (GtkWidget     *widget,
               GtkAllocation *allocation,
               gpointer       user_data)
{
  GfDesktopBackground *background;

  background = GF_DESKTOP_BACKGROUND (user_data);

  if (background->width == allocation->width &&
      background->height == allocation->height)
    {
      GdkWindow *window;
      cairo_pattern_t *pattern;

      window = gtk_widget_get_window (background->background);
      pattern = cairo_pattern_create_for_surface (background->surface);

      gdk_window_set_background_pattern (window, pattern);
      cairo_pattern_destroy (pattern);

      return;
    }

  queue_background_change (background);
}

static void
gf_desktop_background_finalize (GObject *object)
{
  GfDesktopBackground *background;

  background = GF_DESKTOP_BACKGROUND (object);

  g_signal_handlers_disconnect_by_func (background->gnome_settings,
                                        desktop_background_change_event,
                                        background);

  g_clear_object (&background->bg);
  g_clear_object (&background->gnome_settings);

  free_surface (background);
  free_fade (background);

  g_clear_object (&background->gnome_settings);
  g_clear_object (&background->background_settings);

  gtk_widget_destroy (background->background);

  G_OBJECT_CLASS (gf_desktop_background_parent_class)->finalize (object);
}

static void
gf_desktop_background_class_init (GfDesktopBackgroundClass *background_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (background_class);

  object_class->finalize = gf_desktop_background_finalize;
}

static void
gf_desktop_background_init (GfDesktopBackground *background)
{
  background->bg = gnome_bg_new ();
  background->gnome_settings = g_settings_new (DESKTOP_BG);
  background->background_settings = g_settings_new (GNOME_FLASHBACK_BG);

  g_signal_connect (background->bg, "changed",
                    G_CALLBACK (desktop_background_changed), background);
  g_signal_connect (background->bg, "transitioned",
                    G_CALLBACK (desktop_background_transitioned), background);

  g_signal_connect (background->gnome_settings, "change-event",
                    G_CALLBACK (desktop_background_change_event), background);

  background->background = gf_background_window_new ();
  g_signal_connect (background->background, "size-allocate",
                    G_CALLBACK (size_allocate), background);

  gnome_bg_load_from_preferences (background->bg, background->gnome_settings);
}

GfDesktopBackground *
gf_desktop_background_new (void)
{
  return g_object_new (GF_TYPE_DESKTOP_BACKGROUND, NULL);
}
