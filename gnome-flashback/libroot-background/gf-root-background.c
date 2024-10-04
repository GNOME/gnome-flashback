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
#include "gf-root-background.h"

#include "libcommon/gf-bg.h"

#include <gtk/gtk.h>

struct _GfRootBackground
{
  GObject    parent;

  gulong     monitors_changed_id;
  gulong     size_changed_id;

  guint      change_idle_id;

  GfBG      *bg;
};

G_DEFINE_TYPE (GfRootBackground, gf_root_background, G_TYPE_OBJECT)

static void
get_display_size (GdkDisplay *display,
                  int        *width,
                  int        *height)
{
  GdkRectangle rect;
  int n_monitors;
  int i;

  rect = (GdkRectangle) {};
  display = gdk_display_get_default ();
  n_monitors = gdk_display_get_n_monitors (display);

  for (i = 0; i < n_monitors; i++)
    {
      GdkMonitor *monitor;
      GdkRectangle geometry;

      monitor = gdk_display_get_monitor (display, i);

      gdk_monitor_get_geometry (monitor, &geometry);
      gdk_rectangle_union (&rect, &geometry, &rect);
    }

  *width = rect.width;
  *height = rect.height;
}

static void
set_background (GfRootBackground *self)
{
  GdkDisplay *display;
  GdkScreen *screen;
  GdkWindow *root;
  gint width;
  gint height;
  cairo_surface_t *surface;

  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);
  root = gdk_screen_get_root_window (screen);

  get_display_size (display, &width, &height);

  surface = gf_bg_create_surface (self->bg, root, width, height, TRUE);

  gf_bg_set_surface_as_root (display, surface);
  cairo_surface_destroy (surface);
}

static void
changed_cb (GfBG             *bg,
            GfRootBackground *self)
{
  set_background (self);
}

static void
transitioned_cb (GfBG             *bg,
                 GfRootBackground *self)
{
  set_background (self);
}

static gboolean
change_cb (gpointer user_data)
{
  GfRootBackground *self;

  self = GF_ROOT_BACKGROUND (user_data);

  set_background (self);
  self->change_idle_id = 0;

  return G_SOURCE_REMOVE;
}

static void
queue_change (GfRootBackground *self)
{
  if (self->change_idle_id != 0)
    return;

  self->change_idle_id = g_idle_add (change_cb, self);
}

static void
monitors_changed_cb (GdkScreen        *screen,
                     GfRootBackground *self)
{
  queue_change (self);
}

static void
size_changed_cb (GdkScreen        *screen,
                 GfRootBackground *self)
{
  queue_change (self);
}

static void
gf_root_background_dispose (GObject *object)
{
  GfRootBackground *self;
  GdkScreen *screen;

  self = GF_ROOT_BACKGROUND (object);
  screen = gdk_screen_get_default ();

  if (self->monitors_changed_id != 0)
    {
      g_signal_handler_disconnect (screen, self->monitors_changed_id);
      self->monitors_changed_id = 0;
    }

  if (self->size_changed_id != 0)
    {
      g_signal_handler_disconnect (screen, self->size_changed_id);
      self->size_changed_id = 0;
    }

  if (self->change_idle_id != 0)
    {
      g_source_remove (self->change_idle_id);
      self->change_idle_id = 0;
    }

  g_clear_object (&self->bg);

  G_OBJECT_CLASS (gf_root_background_parent_class)->dispose (object);
}

static void
gf_root_background_class_init (GfRootBackgroundClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_root_background_dispose;
}

static void
gf_root_background_init (GfRootBackground *self)
{
  GdkScreen *screen;

  screen = gdk_screen_get_default ();

  self->monitors_changed_id =
    g_signal_connect (screen, "monitors-changed",
                      G_CALLBACK (monitors_changed_cb), self);

  self->size_changed_id =
    g_signal_connect (screen, "size-changed",
                      G_CALLBACK (size_changed_cb), self);

  self->bg = gf_bg_new ("org.gnome.desktop.background");

  g_signal_connect (self->bg, "changed",
                    G_CALLBACK (changed_cb), self);

  g_signal_connect (self->bg, "transitioned",
                    G_CALLBACK (transitioned_cb), self);

  gf_bg_load_from_preferences (self->bg);
}

GfRootBackground *
gf_root_background_new (void)
{
  return g_object_new (GF_TYPE_ROOT_BACKGROUND, NULL);
}
