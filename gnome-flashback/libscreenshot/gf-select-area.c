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
 * https://git.gnome.org/browse/gnome-screenshot/tree/src/screenshot-area-selection.c
 * Copyright (C) 2001 - 2006 Jonathan Blandford, 2008 Cosimo Cecchi
 */

#include "config.h"

#include <gtk/gtk.h>

#include "gf-select-area.h"

struct _GfSelectArea
{
  GObject    parent;

  GtkWidget *window;
  gboolean   composited;

  gboolean   selecting;
  gboolean   selected;

  gint       x;
  gint       y;
  gint       width;
  gint       height;
};

G_DEFINE_TYPE (GfSelectArea, gf_select_area, G_TYPE_OBJECT)

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   user_data)
{
  GfSelectArea *select_area;
  GtkStyleContext *context;
  gint width;
  gint height;

  select_area = GF_SELECT_AREA (user_data);

  if (select_area->composited == FALSE)
    return TRUE;

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_paint (cr);

  context = gtk_widget_get_style_context (widget);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_render_frame (context, cr, 0, 0, width, height);

  return TRUE;
}

static gboolean
key_press_event_cb (GtkWidget   *widget,
                    GdkEventKey *event,
                    gpointer     user_data)
{
  if (event->keyval == GDK_KEY_Escape)
    gtk_main_quit ();

  return TRUE;
}

static gboolean
button_press_event_cb (GtkWidget      *widget,
                       GdkEventButton *event,
                       gpointer        user_data)
{
  GfSelectArea *select_area;

  select_area = GF_SELECT_AREA (user_data);

  if (select_area->selecting)
    return TRUE;

  select_area->selecting = TRUE;
  select_area->x = event->x_root;
  select_area->y = event->y_root;

  return TRUE;
}

static gboolean
button_release_event_cb (GtkWidget      *widget,
                         GdkEventButton *event,
                         gpointer        user_data)
{
  GfSelectArea *select_area;

  select_area = GF_SELECT_AREA (user_data);

  select_area->width = ABS (select_area->x - event->x_root);
  select_area->height = ABS (select_area->y - event->y_root);
  select_area->x = MIN (select_area->x, event->x_root);
  select_area->y = MIN (select_area->y, event->y_root);

  if (select_area->width != 0 && select_area->height != 0)
    select_area->selected = TRUE;

  gtk_main_quit ();

  return TRUE;
}

static gboolean
motion_notify_event_cb (GtkWidget      *widget,
                        GdkEventMotion *event,
                        gpointer        user_data)
{
  GfSelectArea *select_area;
  gint x;
  gint y;
  gint width;
  gint height;
  GtkWindow *window;

  select_area = GF_SELECT_AREA (user_data);

  if (select_area->selecting == FALSE)
    return TRUE;

  x = MIN (select_area->x, event->x_root);
  y = MIN (select_area->y, event->y_root);
  width = ABS (select_area->x - event->x_root);
  height = ABS (select_area->y - event->y_root);

  window = GTK_WINDOW (select_area->window);

  if (width <= 0 || height <= 0)
    {
      gtk_window_move (window, -100, -100);
      gtk_window_resize (window, 10, 10);

      return TRUE;
    }

  gtk_window_move (window, x, y);
  gtk_window_resize (window, width, height);

  if (select_area->composited == FALSE)
    {
      GdkWindow *gdk_window;

      gdk_window = gtk_widget_get_window (select_area->window);

      if (width > 2 && height > 2)
        {
          cairo_region_t *region;
          cairo_rectangle_int_t rectangle;

          rectangle.x = 0;
          rectangle.y = 0;
          rectangle.width = width;
          rectangle.height = height;

          region = cairo_region_create_rectangle (&rectangle);

          rectangle.x++;
          rectangle.y++;
          rectangle.width -= 2;
          rectangle.height -= 2;

          cairo_region_subtract_rectangle (region, &rectangle);

          gdk_window_shape_combine_region (gdk_window, region, 0, 0);
          cairo_region_destroy (region);
        }
      else
        {
          gdk_window_shape_combine_region (gdk_window, NULL, 0, 0);
        }
    }

  return TRUE;
}

static void
setup_window (GfSelectArea *select_area)
{
  GdkScreen *screen;
  GdkVisual *visual;
  GtkWindow *window;

  screen = gdk_screen_get_default ();
  visual = gdk_screen_get_rgba_visual (screen);

  if (gdk_screen_is_composited (screen) && visual != NULL)
    {
      GtkStyleContext *context;

      gtk_widget_set_visual (select_area->window, visual);
      select_area->composited = TRUE;

      context = gtk_widget_get_style_context (select_area->window);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_RUBBERBAND);
    }

  g_signal_connect (select_area->window, "draw",
                    G_CALLBACK (draw_cb), select_area);
  g_signal_connect (select_area->window, "key-press-event",
                    G_CALLBACK (key_press_event_cb), select_area);
  g_signal_connect (select_area->window, "button-press-event",
                    G_CALLBACK (button_press_event_cb), select_area);
  g_signal_connect (select_area->window, "button-release-event",
                    G_CALLBACK (button_release_event_cb), select_area);
  g_signal_connect (select_area->window, "motion-notify-event",
                    G_CALLBACK (motion_notify_event_cb), select_area);

  window = GTK_WINDOW (select_area->window);

  gtk_window_move (window, -100, -100);
  gtk_window_resize (window, 10, 10);
  gtk_widget_show (select_area->window);
}

static void
gf_select_area_dispose (GObject *object)
{
  GfSelectArea *select_area;

  select_area = GF_SELECT_AREA (object);

  if (select_area->window != NULL)
    {
      gtk_widget_destroy (select_area->window);
      select_area->window = NULL;
    }

  G_OBJECT_CLASS (gf_select_area_parent_class)->dispose (object);
}

static void
gf_select_area_class_init (GfSelectAreaClass *select_area_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (select_area_class);

  object_class->dispose = gf_select_area_dispose;
}

static void
gf_select_area_init (GfSelectArea *select_area)
{
  select_area->window = gtk_window_new (GTK_WINDOW_POPUP);
}

GfSelectArea *
gf_select_area_new (void)
{
  return g_object_new (GF_TYPE_SELECT_AREA, NULL);
}

gboolean
gf_select_area_select (GfSelectArea *select_area,
                       gint         *x,
                       gint         *y,
                       gint         *width,
                       gint         *height)
{
  GdkDisplay *display;
  GdkCursor *cursor;
  GdkSeat *seat;
  GdkWindow *window;
  GdkSeatCapabilities capabilities;
  GdkGrabStatus status;

  *x = *y = *width = *height = 0;

  setup_window (select_area);

  display = gdk_display_get_default ();
  cursor = gdk_cursor_new_for_display (display, GDK_CROSSHAIR);
  seat = gdk_display_get_default_seat (display);
  window = gtk_widget_get_window (select_area->window);

  capabilities = GDK_SEAT_CAPABILITY_POINTER |
                 GDK_SEAT_CAPABILITY_KEYBOARD;

  status = gdk_seat_grab (seat, window, capabilities, FALSE, cursor,
                          NULL, NULL, NULL);

  if (status != GDK_GRAB_SUCCESS)
    {
      g_object_unref (cursor);
      return FALSE;
    }

  gtk_main ();

  gdk_seat_ungrab (seat);

  if (select_area->selected == FALSE)
    return FALSE;

  *x = select_area->x;
  *y = select_area->y;
  *width = select_area->width;
  *height = select_area->height;

  return TRUE;
}
