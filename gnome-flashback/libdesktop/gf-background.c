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
#include "gf-background.h"

#include "gf-desktop-window.h"
#include "libcommon/gf-bg.h"

typedef struct
{
  cairo_surface_t *start;
  cairo_surface_t *end;

  double           start_time;
  double           total_duration;
  gboolean         is_first_frame;
  double           percent_done;

  guint            timeout_id;
} FadeData;

struct _GfBackground
{
  GObject          parent;

  GtkWidget       *window;

  GSettings       *settings2;

  GfBG            *bg;

  guint            change_id;

  FadeData        *fade_data;
  cairo_surface_t *surface;
};

enum
{
  PROP_0,

  PROP_WINDOW,

  LAST_PROP
};

static GParamSpec *background_properties[LAST_PROP] = { NULL };

enum
{
  READY,
  CHANGED,

  LAST_SIGNAL
};

static guint background_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfBackground, gf_background, G_TYPE_OBJECT)

static void
free_fade_data (FadeData *data)
{
  if (data->timeout_id != 0)
    {
      g_source_remove (data->timeout_id);
      data->timeout_id = 0;
    }

  g_clear_pointer (&data->start, cairo_surface_destroy);
  g_clear_pointer (&data->end, cairo_surface_destroy);
  g_free (data);
}

static gboolean
fade_cb (gpointer user_data)
{
  GfBackground *self;
  FadeData *fade;
  double current_time;
  double percent_done;

  self = GF_BACKGROUND (user_data);
  fade = self->fade_data;

  current_time = g_get_real_time () / (double) G_USEC_PER_SEC;
  percent_done = (current_time - fade->start_time) / fade->total_duration;
  percent_done = CLAMP (percent_done, 0.0, 1.0);

  if (fade->is_first_frame && percent_done > 0.33)
    {
      fade->total_duration *= 1.5;
      fade->is_first_frame = FALSE;

      return fade_cb (self);
    }

  gtk_widget_queue_draw (self->window);

  fade->is_first_frame = FALSE;
  fade->percent_done = percent_done;

  if (percent_done < 0.99)
    return G_SOURCE_CONTINUE;

  self->fade_data->timeout_id = 0;

  g_clear_pointer (&self->surface, cairo_surface_destroy);
  self->surface = cairo_surface_reference (fade->end);

  g_clear_pointer (&self->fade_data, free_fade_data);

  gf_bg_set_surface_as_root (gtk_widget_get_display (self->window),
                             self->surface);

  g_signal_emit (self, background_signals[CHANGED], 0);

  return G_SOURCE_REMOVE;
}

static void
change (GfBackground *self,
        gboolean      fade)
{
  GdkDisplay *display;
  GdkScreen *screen;
  GdkWindow *root;
  int width;
  int height;

  display = gtk_widget_get_display (self->window);
  screen = gtk_widget_get_screen (self->window);
  root = gdk_screen_get_root_window (screen);

  width = gf_desktop_window_get_width (GF_DESKTOP_WINDOW (self->window));
  height = gf_desktop_window_get_height (GF_DESKTOP_WINDOW (self->window));

  g_clear_pointer (&self->fade_data, free_fade_data);

  if (fade)
    {
      FadeData *data;

      g_assert (self->fade_data == NULL);
      self->fade_data = data = g_new0 (FadeData, 1);

      if (self->surface != NULL)
        data->start = cairo_surface_reference (self->surface);
      else
        data->start = gf_bg_get_surface_from_root (display, width, height);

      data->end = gf_bg_create_surface (self->bg, root, width, height, TRUE);

      data->start_time = g_get_real_time () / (double) G_USEC_PER_SEC;
      data->total_duration = .75;
      data->is_first_frame = TRUE;
      data->percent_done = .0;

      data->timeout_id = g_timeout_add (1000 / 60.0, fade_cb, self);
    }
  else
    {
      g_clear_pointer (&self->surface, cairo_surface_destroy);
      self->surface = gf_bg_create_surface (self->bg,
                                            root,
                                            width,
                                            height,
                                            TRUE);

      gf_bg_set_surface_as_root (display, self->surface);

      g_signal_emit (self, background_signals[CHANGED], 0);
    }

  g_signal_emit (self, background_signals[READY], 0);
  gtk_widget_queue_draw (self->window);
}

typedef struct
{
  GfBackground *background;
  gboolean      fade;
} ChangeData;

static gboolean
change_cb (gpointer user_data)
{
  ChangeData *data;

  data = (ChangeData *) user_data;

  change (data->background, data->fade);
  data->background->change_id = 0;

  return G_SOURCE_REMOVE;
}

static void
queue_change (GfBackground *self,
              gboolean      fade)
{
  ChangeData *data;

  if (self->change_id != 0)
    return;

  data = g_new (ChangeData, 1);

  data->background = self;
  data->fade = fade;

  self->change_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                     change_cb, data, g_free);

  g_source_set_name_by_id (self->change_id, "[gnome-flashback] change_cb");
}

static gboolean
draw_cb (GtkWidget    *widget,
         cairo_t      *cr,
         GfBackground *self)
{
  if (self->fade_data != NULL)
    {
      cairo_set_source_surface (cr, self->fade_data->start, 0, 0);
      cairo_paint (cr);

      cairo_set_source_surface (cr, self->fade_data->end, 0, 0);
      cairo_paint_with_alpha (cr, self->fade_data->percent_done);
    }
  else if (self->surface != NULL)
    {
      cairo_set_source_surface (cr, self->surface, 0, 0);
      cairo_paint (cr);
    }

  return FALSE;
}

static void
size_changed_cb (GfDesktopWindow *window,
                 GfBackground    *self)
{
  queue_change (self, FALSE);
}

static void
changed_cb (GfBG         *bg,
            GfBackground *self)
{
  gboolean fade;

  fade = g_settings_get_boolean (self->settings2, "fade");
  queue_change (self, fade);
}

static void
transitioned_cb (GfBG         *bg,
                 GfBackground *self)
{
  queue_change (self, FALSE);
}

static void
gf_background_constructed (GObject *object)
{
  GfBackground *self;

  self = GF_BACKGROUND (object);

  G_OBJECT_CLASS (gf_background_parent_class)->constructed (object);

  self->settings2 = g_settings_new ("org.gnome.gnome-flashback.desktop.background");

  self->bg = gf_bg_new ("org.gnome.desktop.background");

  g_signal_connect_object (self->window, "draw",
                           G_CALLBACK (draw_cb),
                           self, 0);

  g_signal_connect_object (self->window, "size-changed",
                           G_CALLBACK (size_changed_cb),
                           self, 0);

  g_signal_connect (self->bg, "changed",
                    G_CALLBACK (changed_cb),
                    self);

  g_signal_connect (self->bg, "transitioned",
                    G_CALLBACK (transitioned_cb),
                    self);

  gf_bg_load_from_preferences (self->bg);
}

static void
gf_background_dispose (GObject *object)
{
  GfBackground *self;

  self = GF_BACKGROUND (object);

  g_clear_object (&self->settings2);
  g_clear_object (&self->bg);

  G_OBJECT_CLASS (gf_background_parent_class)->dispose (object);
}

static void
gf_background_finalize (GObject *object)
{
  GfBackground *self;

  self = GF_BACKGROUND (object);

  if (self->change_id != 0)
    {
      g_source_remove (self->change_id);
      self->change_id = 0;
    }

  g_clear_pointer (&self->fade_data, free_fade_data);
  g_clear_pointer (&self->surface, cairo_surface_destroy);

  G_OBJECT_CLASS (gf_background_parent_class)->finalize (object);
}

static void
gf_background_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GfBackground *self;

  self = GF_BACKGROUND (object);

  switch (property_id)
    {
      case PROP_WINDOW:
        g_assert (self->window == NULL);
        self->window = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  background_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "window",
                         "window",
                         GTK_TYPE_WIDGET,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     background_properties);
}

static void
install_signals (void)
{
  background_signals[READY] =
    g_signal_new ("ready", GF_TYPE_BACKGROUND, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  background_signals[CHANGED] =
    g_signal_new ("changed", GF_TYPE_BACKGROUND, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gf_background_class_init (GfBackgroundClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = gf_background_constructed;
  object_class->dispose = gf_background_dispose;
  object_class->finalize = gf_background_finalize;
  object_class->set_property = gf_background_set_property;

  install_properties (object_class);
  install_signals ();
}

static void
gf_background_init (GfBackground *self)
{
}

GfBackground *
gf_background_new (GtkWidget *window)
{
  return g_object_new (GF_TYPE_BACKGROUND,
                       "window", window,
                       NULL);
}

GdkRGBA *
gf_background_get_average_color (GfBackground *self)
{
  return gf_bg_get_average_color_from_surface (self->surface);
}
