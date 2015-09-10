/*
 * Copyright (C) 2015 Alberts Muktupāvels
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
 *
 * Based on code in gnome-screenshot:
 * https://git.gnome.org/browse/gnome-screenshot/tree/src/cheese-flash.c
 * Copyright (C) 2008 Alexander “weej” Jones, 2008 Thomas Perl,
 *               2009 daniel g. siegel
 */

#include "config.h"

#include <gtk/gtk.h>

#include "gf-flashspot.h"

#define FLASH_DURATION 100
#define FLASH_ANIMATION_RATE 200

struct _GfFlashspot
{
  GObject    parent;

  GtkWidget *window;

  guint      flash_id;
  guint      fade_id;
};

enum
{
  SIGNAL_FINISHED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfFlashspot, gf_flashspot, G_TYPE_OBJECT)

static gboolean
fade (gpointer user_data)
{
  GfFlashspot *flashspot;
  gdouble opacity;

  flashspot = GF_FLASHSPOT (user_data);
  opacity = gtk_widget_get_opacity (flashspot->window);

  opacity -= 0.02;

  gtk_widget_set_opacity (flashspot->window, opacity);

  if (opacity <= 0.01)
    {
      gtk_widget_hide (flashspot->window);

      g_signal_emit (flashspot, signals[SIGNAL_FINISHED], 0);

      flashspot->fade_id = 0;
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static gboolean
start_fade (gpointer user_data)
{
  GfFlashspot *flashspot;
  GdkScreen *screen;

  flashspot = GF_FLASHSPOT (user_data);
  screen = gtk_widget_get_screen (flashspot->window);

  if (!gdk_screen_is_composited (screen))
    {
      gtk_widget_hide (flashspot->window);

      g_signal_emit (flashspot, signals[SIGNAL_FINISHED], 0);

      flashspot->flash_id = 0;
      return G_SOURCE_REMOVE;
    }

  flashspot->fade_id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                           1000.0 / FLASH_ANIMATION_RATE,
                                           fade,
                                           g_object_ref (flashspot),
                                           g_object_unref);

  flashspot->flash_id = 0;
  return G_SOURCE_REMOVE;
}

static void
gf_flashspot_dispose (GObject *object)
{
  GfFlashspot *flashspot;

  flashspot = GF_FLASHSPOT (object);

  if (flashspot->window != NULL)
    {
      gtk_widget_destroy (flashspot->window);
      flashspot->window = NULL;
    }

  if (flashspot->flash_id > 0)
    {
      g_source_remove (flashspot->flash_id);
      flashspot->flash_id = 0;
    }

  if (flashspot->fade_id > 0)
    {
      g_source_remove (flashspot->fade_id);
      flashspot->fade_id = 0;
    }

  G_OBJECT_CLASS (gf_flashspot_parent_class)->dispose (object);
}

static void
gf_flashspot_class_init (GfFlashspotClass *flashspot_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (flashspot_class);

  object_class->dispose = gf_flashspot_dispose;

  signals[SIGNAL_FINISHED] =
    g_signal_new ("finished",
                  G_OBJECT_CLASS_TYPE (flashspot_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
gf_flashspot_init (GfFlashspot *flashspot)
{
  GtkWindow *window;
  GdkScreen *screen;
  GdkVisual *visual;
  cairo_region_t *input_region;

  flashspot->window = gtk_window_new (GTK_WINDOW_POPUP);

  window = GTK_WINDOW (flashspot->window);
  screen = gtk_widget_get_screen (flashspot->window);
  visual = gdk_screen_get_rgba_visual (screen);

  if (visual == NULL)
    visual = gdk_screen_get_system_visual (screen);

  gtk_window_set_decorated (window, FALSE);
  gtk_window_set_skip_taskbar_hint (window, TRUE);
  gtk_window_set_skip_pager_hint (window, TRUE);
  gtk_window_set_keep_above (window, TRUE);
  gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_NOTIFICATION);
  gtk_window_set_accept_focus (window, FALSE);
  gtk_window_set_focus_on_map (window, FALSE);

  gtk_widget_set_visual (flashspot->window, visual);
  gtk_widget_realize (flashspot->window);

  input_region = cairo_region_create ();
  gdk_window_input_shape_combine_region (gtk_widget_get_window (flashspot->window),
                                         input_region, 0, 0);
  cairo_region_destroy (input_region);
}

GfFlashspot *
gf_flashspot_new (void)
{
  return g_object_new (GF_TYPE_FLASHSPOT, NULL);
}

void
gf_flashspot_fire (GfFlashspot *flashspot,
                   gint         x,
                   gint         y,
                   gint         width,
                   gint         height)
{
  GtkWindow *window;

  if (flashspot->flash_id > 0)
    g_source_remove (flashspot->flash_id);

  if (flashspot->fade_id > 0)
    g_source_remove (flashspot->fade_id);

  window = GTK_WINDOW (flashspot->window);

  gtk_window_move (window, x, y);
  gtk_window_resize (window, width, height);

  gtk_widget_set_opacity (flashspot->window, 0.99);
  gtk_widget_show_all (flashspot->window);

  flashspot->flash_id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                            FLASH_DURATION,
                                            start_fade,
                                            g_object_ref (flashspot),
                                            g_object_unref);
}
