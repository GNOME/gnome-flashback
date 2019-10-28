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
#include "gf-desktop-window.h"

#include "gf-background.h"
#include "gf-icon-view.h"

struct _GfDesktopWindow
{
  GtkWindow     parent;

  gboolean      draw_background;
  GfBackground *background;

  gboolean      show_icons;
  GtkWidget    *icon_view;

  gboolean      ready;
};

enum
{
  PROP_0,

  PROP_DRAW_BACKGROUND,
  PROP_SHOW_ICONS,

  LAST_PROP
};

static GParamSpec *window_properties[LAST_PROP] = { NULL };

enum
{
  READY,

  LAST_SIGNAL
};

static guint window_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfDesktopWindow, gf_desktop_window, GTK_TYPE_WINDOW)

static void
emit_ready (GfDesktopWindow *self)
{
  if (self->ready)
    return;

  g_signal_emit (self, window_signals[READY], 0);
  self->ready = TRUE;
}

static void
ready_cb (GfBackground    *background,
          GfDesktopWindow *self)
{
  emit_ready (self);
}

static void
draw_background_changed (GfDesktopWindow *self)
{
  if (self->draw_background)
    {
      g_assert (self->background == NULL);
      self->background = gf_background_new (GTK_WIDGET (self));

      g_signal_connect (self->background, "ready", G_CALLBACK (ready_cb), self);
    }
  else
    {
      g_clear_object (&self->background);
    }
}

static void
show_icons_changed (GfDesktopWindow *self)
{
  if (self->show_icons)
    {
      g_assert (self->icon_view == NULL);
      self->icon_view = gf_icon_view_new ();

      gtk_container_add (GTK_CONTAINER (self), self->icon_view);
      gtk_widget_show (self->icon_view);
    }
  else if (self->icon_view != NULL)
    {
      gtk_container_remove (GTK_CONTAINER (self), self->icon_view);
      self->icon_view = NULL;
    }
}

static void
set_draw_background (GfDesktopWindow *self,
                     gboolean         draw_background)
{
  if (self->draw_background == draw_background)
    return;

  self->draw_background = draw_background;
  draw_background_changed (self);
}

static void
set_show_icons (GfDesktopWindow *self,
                gboolean         show_icons)
{
  if (self->show_icons == show_icons)
    return;

  self->show_icons = show_icons;
  show_icons_changed (self);
}

static void
gf_desktop_window_constructed (GObject *object)
{
  GfDesktopWindow *self;
  GParamSpecBoolean *spec;

  self = GF_DESKTOP_WINDOW (object);

  G_OBJECT_CLASS (gf_desktop_window_parent_class)->constructed (object);

  spec = (GParamSpecBoolean *) window_properties[PROP_DRAW_BACKGROUND];
  if (self->draw_background == spec->default_value)
    draw_background_changed (self);

  spec = (GParamSpecBoolean *) window_properties[PROP_SHOW_ICONS];
  if (self->show_icons == spec->default_value)
    show_icons_changed (self);
}

static void
gf_desktop_window_dispose (GObject *object)
{
  GfDesktopWindow *self;

  self = GF_DESKTOP_WINDOW (object);

  g_clear_object (&self->background);

  G_OBJECT_CLASS (gf_desktop_window_parent_class)->dispose (object);
}

static void
gf_desktop_window_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GfDesktopWindow *self;

  self = GF_DESKTOP_WINDOW (object);

  switch (property_id)
    {
      case PROP_DRAW_BACKGROUND:
        set_draw_background (self, g_value_get_boolean (value));
        break;

      case PROP_SHOW_ICONS:
        set_show_icons (self, g_value_get_boolean (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_desktop_window_realize (GtkWidget *widget)
{
  GdkScreen *screen;
  GdkVisual *visual;

  screen = gtk_widget_get_screen (widget);
  visual = gdk_screen_get_rgba_visual (screen);

  if (visual != NULL)
    gtk_widget_set_visual (widget, visual);

  GTK_WIDGET_CLASS (gf_desktop_window_parent_class)->realize (widget);
}

static void
install_properties (GObjectClass *object_class)
{
  window_properties[PROP_DRAW_BACKGROUND] =
    g_param_spec_boolean ("draw-background",
                          "draw-background",
                          "draw-background",
                          TRUE,
                          G_PARAM_CONSTRUCT | G_PARAM_WRITABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);

  window_properties[PROP_SHOW_ICONS] =
    g_param_spec_boolean ("show-icons",
                          "show-icons",
                          "show-icons",
                          TRUE,
                          G_PARAM_CONSTRUCT | G_PARAM_WRITABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, window_properties);
}

static void
install_signals (void)
{
  window_signals[READY] =
    g_signal_new ("ready", GF_TYPE_DESKTOP_WINDOW, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gf_desktop_window_class_init (GfDesktopWindowClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);

  object_class->constructed = gf_desktop_window_constructed;
  object_class->dispose = gf_desktop_window_dispose;
  object_class->set_property = gf_desktop_window_set_property;

  widget_class->realize = gf_desktop_window_realize;

  install_properties (object_class);
  install_signals ();
}

static void
gf_desktop_window_init (GfDesktopWindow *self)
{
  GParamSpecBoolean *spec;

  spec = (GParamSpecBoolean *) window_properties[PROP_DRAW_BACKGROUND];
  self->draw_background = spec->default_value;

  spec = (GParamSpecBoolean *) window_properties[PROP_SHOW_ICONS];
  self->show_icons =  spec->default_value;
}

GtkWidget *
gf_desktop_window_new (gboolean draw_background,
                       gboolean show_icons)
{
  return g_object_new (GF_TYPE_DESKTOP_WINDOW,
                       "type", GTK_WINDOW_TOPLEVEL,
                       "type-hint", GDK_WINDOW_TYPE_HINT_DESKTOP,
                       "draw-background", draw_background,
                       "show-icons", show_icons,
                       NULL);
}

gboolean
gf_desktop_window_is_ready (GfDesktopWindow *self)
{
  return self->ready;
}
