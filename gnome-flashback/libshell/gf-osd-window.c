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
#include "gf-osd-window.h"

#define HIDE_TIMEOUT 1500

struct _GfOsdWindow
{
  GfPopupWindow  parent;

  GdkRectangle   monitor;

  guint          hide_timeout_id;

  gdouble        max_level;
  gdouble        level;

  GtkWidget     *icon_image;
  gint           icon_size;

  GtkWidget     *label;

  GtkWidget     *level_bar;
};

G_DEFINE_TYPE (GfOsdWindow, gf_osd_window, GF_TYPE_POPUP_WINDOW)

static void
update_level_bar (GfOsdWindow *window)
{
  gdouble level;
  GtkLevelBar *level_bar;

  if (window->level < 0.0)
    {
      gtk_widget_hide (window->level_bar);
      return;
    }

  level = MAX (0.0, MIN (window->level, window->max_level));
  level_bar = GTK_LEVEL_BAR (window->level_bar);

  gtk_level_bar_set_max_value (level_bar, window->max_level);
  gtk_level_bar_set_value (level_bar, level);

  if (window->max_level > 1.0)
    gtk_level_bar_add_offset_value (level_bar, "overdrive", window->max_level);
  else
    gtk_level_bar_remove_offset_value (level_bar, "overdrive");

  gtk_widget_show (window->level_bar);
}

static void
fade_finished_cb (GfPopupWindow *window)
{
  gf_osd_window_hide (GF_OSD_WINDOW (window));
}

static gboolean
hide_timeout_cb (gpointer user_data)
{
  GfOsdWindow *window;

  window = GF_OSD_WINDOW (user_data);

  gf_popup_window_fade_start (GF_POPUP_WINDOW (window));

  window->hide_timeout_id = 0;

  return FALSE;
}

static void
add_hide_timeout (GfOsdWindow *window)
{
  window->hide_timeout_id = g_timeout_add (HIDE_TIMEOUT,
                                           (GSourceFunc) hide_timeout_cb,
                                           window);
}

static void
remove_hide_timeout (GfOsdWindow *window)
{
  if (window->hide_timeout_id > 0)
    {
      g_source_remove (window->hide_timeout_id);
      window->hide_timeout_id = 0;
    }

  gf_popup_window_fade_cancel (GF_POPUP_WINDOW (window));
}

static void
gf_osd_window_finalize (GObject *object)
{
  GfOsdWindow *window;

  window = GF_OSD_WINDOW (object);

  remove_hide_timeout (window);

  G_OBJECT_CLASS (gf_osd_window_parent_class)->finalize (object);
}

static void
gf_osd_window_realize (GtkWidget *widget)
{
  cairo_region_t *region;

  GTK_WIDGET_CLASS (gf_osd_window_parent_class)->realize (widget);

  region = cairo_region_create ();
  gtk_widget_input_shape_combine_region (widget, region);
  cairo_region_destroy (region);
}

static void
gf_osd_window_class_init (GfOsdWindowClass *window_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (window_class);
  widget_class = GTK_WIDGET_CLASS (window_class);

  object_class->finalize = gf_osd_window_finalize;

  widget_class->realize = gf_osd_window_realize;
}

static void
gf_osd_window_init (GfOsdWindow *window)
{
  GtkWidget *box;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width (GTK_CONTAINER (window), 20);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show (box);

  window->max_level = 1.0;
  window->level = -1;

  window->icon_image = gtk_image_new ();
  gtk_widget_set_halign (window->icon_image, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (window->icon_image, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), window->icon_image, TRUE, FALSE, 0);

  window->label = gtk_label_new ("");
  gtk_widget_set_halign (window->label, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), window->label, FALSE, FALSE, 0);

  window->level_bar = gtk_level_bar_new_for_interval (0.0, window->max_level);
  gtk_box_pack_start (GTK_BOX (box), window->level_bar, FALSE, FALSE, 0);
  gtk_widget_set_halign (window->level_bar, GTK_ALIGN_FILL);

  g_signal_connect (window, "fade-finished",
                    G_CALLBACK (fade_finished_cb), NULL);

  gtk_widget_set_name (GTK_WIDGET (window), "gf-osd-window");
}

GfOsdWindow *
gf_osd_window_new (gint monitor)
{
  GfOsdWindow *window;
  GdkDisplay *display;
  GdkMonitor *gdk_monitor;
  gint width;
  gint height;
  gint size;

  display = gdk_display_get_default ();
  window = g_object_new (GF_TYPE_OSD_WINDOW,
                         "type", GTK_WINDOW_POPUP,
                         NULL);

  gdk_monitor = gdk_display_get_monitor (display, monitor);
  gdk_monitor_get_workarea (gdk_monitor, &window->monitor);

  width = window->monitor.width;
  height = window->monitor.height;
  size = 110 * MAX (1, MIN (width / 640.0, height / 480.0));

  window->icon_size = size / 2;

  gtk_window_resize (GTK_WINDOW (window), size, size);

  return window;
}

void
gf_osd_window_set_icon (GfOsdWindow *window,
                        GIcon       *icon)
{
  if (icon == NULL)
    {
      gtk_widget_hide (window->icon_image);
      return;
    }

  gtk_image_set_pixel_size (GTK_IMAGE (window->icon_image),
                            window->icon_size);
  gtk_image_set_from_gicon (GTK_IMAGE (window->icon_image),
                            icon, GTK_ICON_SIZE_DIALOG);
  gtk_widget_show (window->icon_image);
}

void
gf_osd_window_set_label (GfOsdWindow *window,
                         const gchar *label)
{
  if (label == NULL)
    {
      gtk_widget_hide (window->label);
      return;
    }

  gtk_label_set_text (GTK_LABEL (window->label), label);
  gtk_widget_show (window->label);
}

void
gf_osd_window_set_level (GfOsdWindow *window,
                         gdouble      level)
{
  window->level = level;

  update_level_bar (window);
}

void
gf_osd_window_set_max_level (GfOsdWindow *window,
                             gdouble      max_level)
{
  window->max_level = max_level;

  update_level_bar (window);
}

void
gf_osd_window_show (GfOsdWindow *window)
{
  gint width;
  gint height;
  GdkRectangle rect;
  gint x;
  gint y;

  gtk_window_get_size (GTK_WINDOW (window), &width, &height);

  rect = window->monitor;
  x = ((rect.width - width) / 2) + rect.x;
  y = ((rect.height - height) / 4 * 3) + rect.y;

  gtk_window_move (GTK_WINDOW (window), x, y);
  gtk_widget_show (GTK_WIDGET (window));
  remove_hide_timeout (window);
  add_hide_timeout (window);
}

void
gf_osd_window_hide (GfOsdWindow *window)
{
  gtk_widget_hide (GTK_WIDGET (window));
  remove_hide_timeout (window);
}
