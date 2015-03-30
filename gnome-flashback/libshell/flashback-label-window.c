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
 */

#include <config.h>
#include <math.h>
#include <gdk/gdk.h>
#include "flashback-label-window.h"

#define HIDE_TIMEOUT 1500
#define FADE_TIMEOUT 10

struct _FlashbackLabelWindow
{
  GtkWindow     parent;

  GdkRectangle  monitor;

  guint         hide_timeout_id;
  guint         fade_timeout_id;

  gdouble       fade_out_alpha;

  GtkWidget    *label;
};

G_DEFINE_TYPE (FlashbackLabelWindow, flashback_label_window, GTK_TYPE_WINDOW)

static cairo_surface_t *
flashback_label_window_draw_real (FlashbackLabelWindow *window,
                                  cairo_t              *cr1,
                                  gint                  width,
                                  gint                  height)
{
  cairo_surface_t *surface;
  cairo_t *cr2;
  GtkStyleContext *context;

  surface = cairo_surface_create_similar (cairo_get_target (cr1),
                                          CAIRO_CONTENT_COLOR_ALPHA,
                                          width, height);

  if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
    {
      if (surface)
        cairo_surface_destroy (surface);
      return NULL;
    }

  cr2 = cairo_create (surface);

  if (cairo_status (cr2) != CAIRO_STATUS_SUCCESS)
    {
      cairo_surface_destroy (surface);
      return NULL;
    }

  context = gtk_widget_get_style_context (GTK_WIDGET (window));
  gtk_render_background (context, cr2, 0, 0, width, height);
  gtk_render_frame (context, cr2, 0, 0, width, height);

  cairo_destroy (cr2);

  return surface;
}

static gboolean
flashback_label_window_draw (GtkWidget *widget,
                             cairo_t   *cr)
{
  FlashbackLabelWindow *window;
  gint width;
  gint height;
  cairo_surface_t *surface;

  window = FLASHBACK_LABEL_WINDOW (widget);

  gtk_window_get_size (GTK_WINDOW (widget), &width, &height);

  surface = flashback_label_window_draw_real (window, cr, width, height);

  if (surface == NULL)
    return TRUE;

  cairo_rectangle (cr, 0, 0, width, height);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
  cairo_fill (cr);

  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint_with_alpha (cr, window->fade_out_alpha);

  cairo_surface_destroy (surface);

  return GTK_WIDGET_CLASS (flashback_label_window_parent_class)->draw (widget, cr);
}

static gboolean
fade_timeout_cb (gpointer user_data)
{
  FlashbackLabelWindow *window;

  window = FLASHBACK_LABEL_WINDOW (user_data);

  if (window->fade_out_alpha <= 0.0)
    {
      window->fade_timeout_id = 0;
      gtk_widget_destroy (GTK_WIDGET (window));

      return FALSE;
    }

  window->fade_out_alpha -= 0.10;

  gtk_widget_queue_draw (GTK_WIDGET (window));

  return TRUE;
}

static void
remove_fade_timeout (FlashbackLabelWindow *window)
{
  if (window->fade_timeout_id > 0)
    {
      g_source_remove (window->fade_timeout_id);
      window->fade_timeout_id = 0;
      window->fade_out_alpha = 1.0;
    }
}

static void
flashback_label_window_finalize (GObject *object)
{
  FlashbackLabelWindow *window;

  window = FLASHBACK_LABEL_WINDOW (object);

  remove_fade_timeout (window);

  G_OBJECT_CLASS (flashback_label_window_parent_class)->finalize (object);
}

static void
flashback_label_window_realize (GtkWidget *widget)
{
  GdkScreen *screen;
  GdkVisual *visual;
  cairo_region_t *region;

  screen = gtk_widget_get_screen (widget);
  visual = gdk_screen_get_rgba_visual (screen);

  if (visual == NULL)
    visual = gdk_screen_get_system_visual (screen);

  gtk_widget_set_visual (widget, visual);

  GTK_WIDGET_CLASS (flashback_label_window_parent_class)->realize (widget);

  region = cairo_region_create ();
  gtk_widget_input_shape_combine_region (widget, region);
  cairo_region_destroy (region);
}

static void
flashback_label_window_class_init (FlashbackLabelWindowClass *window_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (window_class);
  widget_class = GTK_WIDGET_CLASS (window_class);

  object_class->finalize = flashback_label_window_finalize;

  widget_class->draw = flashback_label_window_draw;
  widget_class->realize = flashback_label_window_realize;
}

static void
flashback_label_window_init (FlashbackLabelWindow *window)
{
  GtkWidget *box;

  window->fade_out_alpha = 1.0;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width (GTK_CONTAINER (window), 20);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show (box);

  window->label = gtk_label_new ("");
  gtk_widget_set_halign (window->label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (window->label, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), window->label, TRUE, FALSE, 0);
  gtk_widget_show (window->label);
}

FlashbackLabelWindow *
flashback_label_window_new (gint         monitor,
                            const gchar *label)
{
  FlashbackLabelWindow *window;
  GdkScreen *screen;
  gint width;
  gint height;
  gint size;

  screen = gdk_screen_get_default ();
  window = g_object_new (FLASHBACK_TYPE_LABEL_WINDOW,
                         "type", GTK_WINDOW_POPUP,
                         "type-hint", GDK_WINDOW_TYPE_HINT_NOTIFICATION,
                         "app-paintable", TRUE,
                         "decorated", FALSE,
                         "skip-taskbar-hint", TRUE,
                         "skip-pager-hint", TRUE,
                         "focus-on-map", FALSE,
                         NULL);

  gdk_screen_get_monitor_workarea (screen, monitor, &window->monitor);

  width = window->monitor.width;
  height = window->monitor.height;
  size = 60 * MAX (1, MIN (width / 640.0, height / 480.0));

  gtk_window_resize (GTK_WINDOW (window), size, size);

  gtk_label_set_text (GTK_LABEL (window->label), label);

  return window;
}

void
flashback_label_window_show (FlashbackLabelWindow *window)
{
  gint width;
  gint height;
  GdkRectangle rect;
  GtkTextDirection dir;
  gint x;
  gint y;

  gtk_window_get_size (GTK_WINDOW (window), &width, &height);

  rect = window->monitor;
  dir = gtk_widget_get_direction (GTK_WIDGET (window));

  if (dir == GTK_TEXT_DIR_NONE)
    dir = gtk_widget_get_default_direction ();

  if (dir == GTK_TEXT_DIR_RTL)
    x = rect.x + (rect.width - width - 20);
  else
    x = rect.x + 20;
  y = rect.y + 20;

  gtk_window_move (GTK_WINDOW (window), x, y);
  gtk_widget_show (GTK_WIDGET (window));
  remove_fade_timeout (window);
}

void
flashback_label_window_hide (FlashbackLabelWindow *window)
{
  window->fade_timeout_id = g_timeout_add (FADE_TIMEOUT,
                                           (GSourceFunc) fade_timeout_cb,
                                           window);
}
