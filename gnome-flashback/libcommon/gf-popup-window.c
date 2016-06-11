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

#include "config.h"
#include "gf-popup-window.h"

typedef struct _GfPopupWindowPrivate GfPopupWindowPrivate;
struct _GfPopupWindowPrivate
{
  gboolean composited;
  guint    fade_id;
};

enum
{
  SIGNAL_FADE_FINISHED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GfPopupWindow, gf_popup_window, GTK_TYPE_WINDOW)

static gboolean
fade_out_cb (gpointer user_data)
{
  GfPopupWindow *window;
  GfPopupWindowPrivate *priv;
  GtkWidget *widget;
  gdouble opacity;

  window = GF_POPUP_WINDOW (user_data);
  priv = gf_popup_window_get_instance_private (window);
  widget = GTK_WIDGET (window);
  opacity = gtk_widget_get_opacity (widget);

  opacity -= 0.04;
  if (!priv->composited || opacity < 0.00)
    {
      gtk_widget_set_opacity (widget, 1.0);

      priv->fade_id = 0;
      g_signal_emit (window, signals[SIGNAL_FADE_FINISHED], 0);

      return G_SOURCE_REMOVE;
    }

  gtk_widget_set_opacity (widget, opacity);
  gtk_widget_queue_draw (widget);

  return G_SOURCE_CONTINUE;
}

static void
gf_popup_window_finalize (GObject *object)
{
  GfPopupWindow *window;
  GfPopupWindowPrivate *priv;

  window = GF_POPUP_WINDOW (object);
  priv = gf_popup_window_get_instance_private (window);

  if (priv->fade_id > 0)
    {
      g_source_remove (priv->fade_id);
      priv->fade_id = 0;
    }

  G_OBJECT_CLASS (gf_popup_window_parent_class)->finalize (object);
}

static cairo_surface_t *
get_background_surface (GtkWidget *widget,
                        cairo_t   *cr,
                        gint       width,
                        gint       height)
{
  cairo_surface_t *surface;
  cairo_t *cr_local;
  GtkStyleContext *context;

  surface = cairo_surface_create_similar (cairo_get_target (cr),
                                          CAIRO_CONTENT_COLOR_ALPHA,
                                          width, height);

  if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
    {
      if (surface)
        cairo_surface_destroy (surface);

      return NULL;
    }

  cr_local = cairo_create (surface);

  if (cairo_status (cr_local) != CAIRO_STATUS_SUCCESS)
    {
      cairo_surface_destroy (surface);

      if (cr_local)
        cairo_destroy (cr_local);

      return NULL;
    }

  context = gtk_widget_get_style_context (widget);

  gtk_render_background (context, cr_local, 0, 0, width, height);
  gtk_render_frame (context, cr_local, 0, 0, width, height);

  cairo_destroy (cr_local);

  return surface;
}

static void
gf_popup_window_composited_changed (GtkWidget *widget)
{
  GfPopupWindow *window;
  GfPopupWindowPrivate *priv;
  GdkScreen *screen;
  GtkStyleContext *context;

  window = GF_POPUP_WINDOW (widget);
  priv = gf_popup_window_get_instance_private (window);

  screen = gtk_widget_get_screen (widget);
  context = gtk_widget_get_style_context (widget);
  priv->composited = gdk_screen_is_composited (screen);

  if (priv->composited)
    gtk_style_context_remove_class (context, "solid");
  else
    gtk_style_context_add_class (context, "solid");
}

static gboolean
gf_popup_window_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  gint width;
  gint height;
  cairo_surface_t *surface;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  surface = get_background_surface (widget, cr, width, height);

  if (surface == NULL)
    return TRUE;

  cairo_rectangle (cr, 0, 0, width, height);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
  cairo_fill (cr);

  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);

  cairo_surface_destroy (surface);

  return GTK_WIDGET_CLASS (gf_popup_window_parent_class)->draw (widget, cr);
}

static void
gf_popup_window_realize (GtkWidget *widget)
{
  GfPopupWindow *window;
  GfPopupWindowPrivate *priv;
  GdkScreen *screen;
  GtkStyleContext *context;
  GdkVisual *visual;

  window = GF_POPUP_WINDOW (widget);
  priv = gf_popup_window_get_instance_private (window);

  screen = gtk_widget_get_screen (widget);
  context = gtk_widget_get_style_context (widget);

  priv->composited = gdk_screen_is_composited (screen);
  visual = gdk_screen_get_rgba_visual (screen);

  if (priv->composited)
    gtk_style_context_remove_class (context, "solid");
  else
    gtk_style_context_add_class (context, "solid");

  if (visual == NULL)
    visual = gdk_screen_get_system_visual (screen);

  gtk_widget_set_visual (widget, visual);

  GTK_WIDGET_CLASS (gf_popup_window_parent_class)->realize (widget);
}

static void
gf_popup_window_class_init (GfPopupWindowClass *window_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (window_class);
  widget_class = GTK_WIDGET_CLASS (window_class);

  object_class->finalize = gf_popup_window_finalize;

  widget_class->composited_changed = gf_popup_window_composited_changed;
  widget_class->draw = gf_popup_window_draw;
  widget_class->realize = gf_popup_window_realize;

  signals[SIGNAL_FADE_FINISHED] =
    g_signal_new ("fade-finished", G_OBJECT_CLASS_TYPE (window_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, "gf-popup-window");
}

static void
gf_popup_window_init (GfPopupWindow *window)
{
  GtkWindow *gtk_window;
  GtkWidget *widget;

  gtk_window = GTK_WINDOW (window);
  widget = GTK_WIDGET (window);

  gtk_window_set_decorated (gtk_window, FALSE);
  gtk_window_set_focus_on_map (gtk_window, FALSE);
  gtk_window_set_type_hint (gtk_window, GDK_WINDOW_TYPE_HINT_NOTIFICATION);
  gtk_window_set_skip_pager_hint (gtk_window, TRUE);;
  gtk_window_set_skip_taskbar_hint (gtk_window, TRUE);

  gtk_widget_set_app_paintable (widget, TRUE);
}

void
gf_popup_window_fade_start (GfPopupWindow *window)
{
  GfPopupWindowPrivate *priv;
  GtkWidget *widget;

  priv = gf_popup_window_get_instance_private (window);
  widget = GTK_WIDGET (window);

  if (priv->fade_id != 0)
    g_source_remove (priv->fade_id);

  gtk_widget_set_opacity (widget, 1.0);

  priv->fade_id = g_timeout_add (10, fade_out_cb, window);
  g_source_set_name_by_id (priv->fade_id, "[gnome-flashback] fade_out_cb");
}

void
gf_popup_window_fade_cancel (GfPopupWindow *window)
{
  GfPopupWindowPrivate *priv;
  GtkWidget *widget;

  priv = gf_popup_window_get_instance_private (window);
  widget = GTK_WIDGET (window);

  if (priv->fade_id != 0)
    {
      g_source_remove (priv->fade_id);
      priv->fade_id = 0;
    }

  gtk_widget_set_opacity (widget, 1.0);
}
