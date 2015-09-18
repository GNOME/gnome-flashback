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
#include "gf-background-window.h"

struct _GfDesktopWindow
{
  GtkWindow parent;
};

G_DEFINE_TYPE (GfDesktopWindow, gf_desktop_window, GTK_TYPE_WINDOW)

static void
screen_changed (GdkScreen *screen,
                gpointer   user_data)
{
  GfDesktopWindow *window;
  gint width;
  gint height;

  window = GF_DESKTOP_WINDOW (user_data);
  width = gdk_screen_get_width (screen);
  height = gdk_screen_get_height (screen);

  g_object_set (window,
                "width-request", width,
                "height-request", height,
                NULL);
}

static void
gf_desktop_window_map (GtkWidget *widget)
{
  GdkWindow *window;

  GTK_WIDGET_CLASS (gf_desktop_window_parent_class)->map (widget);

  window = gtk_widget_get_window (widget);

  gdk_window_lower (window);
}

static void
gf_desktop_window_class_init (GfDesktopWindowClass *window_class)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (window_class);

  widget_class->map = gf_desktop_window_map;
}

static void
gf_desktop_window_init (GfDesktopWindow *window)
{
  GdkScreen *screen;

  screen = gdk_screen_get_default ();

  g_signal_connect_object (screen, "monitors-changed",
                           G_CALLBACK (screen_changed), window,
                           G_CONNECT_AFTER);

  g_signal_connect_object (screen, "size-changed",
                           G_CALLBACK (screen_changed), window,
                           G_CONNECT_AFTER);

  screen_changed (screen, window);
}

GtkWidget *
gf_desktop_window_new (void)
{
  GtkWidget *window;

  window = g_object_new (GF_TYPE_DESKTOP_WINDOW,
                         "accept-focus", FALSE,
                         "app-paintable", TRUE,
                         "decorated", FALSE,
                         "resizable", FALSE,
                         "skip-pager-hint", TRUE,
                         "skip-taskbar-hint", TRUE,
                         "type", GTK_WINDOW_TOPLEVEL,
                         "type-hint", GDK_WINDOW_TYPE_HINT_DESKTOP,
                         NULL);

  gtk_window_set_keep_below (GTK_WINDOW (window), TRUE);
  gtk_widget_realize (window);

  return window;
}
