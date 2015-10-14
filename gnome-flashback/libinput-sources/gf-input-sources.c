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

#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#define _XOPEN_SOURCE
#include <math.h>

#include "gf-ibus-manager.h"
#include "gf-input-sources.h"
#include "gf-input-source-manager.h"
#include "gf-input-source.h"

#define INPUT_SOURCES_SCHEMA "org.gnome.gnome-flashback.input-sources"
#define GNOME_DESKTOP_SCHEMA "org.gnome.desktop.interface"

struct _GfInputSources
{
  GObject               parent;

  GfIBusManager        *ibus_manager;
  GfInputSourceManager *input_source_manager;

  GSettings            *input_sources_settings;
  GSettings            *gnome_desktop_settings;

  GfInputSource        *current_source;
  GtkStatusIcon        *status_icon;
};

G_DEFINE_TYPE (GfInputSources, gf_input_sources, G_TYPE_OBJECT)

static void
draw_background (GfInputSources *sources,
                 cairo_t        *cr,
                 gint            size)
{
  gdouble x;
  gdouble y;
  gdouble width;
  gdouble height;
  gdouble radius;
  gdouble degrees;
  gchar *color;
  GdkRGBA rgba;

  x = size * 0.04;
  y = size * 0.04;
  width = size - x * 2;
  height = size - y * 2;

  radius = height / 10;
  degrees = M_PI / 180.0;

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + width - radius, y + radius,
             radius, -90 * degrees, 0 * degrees);
  cairo_arc (cr, x + width - radius, y + height - radius,
             radius, 0 * degrees, 90 * degrees);
  cairo_arc (cr, x + radius, y + height - radius,
             radius, 90 * degrees, 180 * degrees);
  cairo_arc (cr, x + radius, y + radius,
             radius, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);

  color = g_settings_get_string (sources->input_sources_settings,
                                 "status-icon-bg-color");

  gdk_rgba_parse (&rgba, color);
  g_free (color);

  gdk_cairo_set_source_rgba (cr, &rgba);
  cairo_fill_preserve (cr);
}

static void
draw_text (GfInputSources *sources,
           cairo_t        *cr,
           gint            size)
{
  gchar *font_name;
  PangoFontDescription *font_desc;
  gdouble font_size;
  PangoLayout *layout;
  const gchar *short_name;
  gint text_width;
  gint text_height;
  gdouble factor;
  gdouble center;
  gdouble x;
  gdouble y;
  gchar *color;
  GdkRGBA rgba;

  font_name = g_settings_get_string (sources->gnome_desktop_settings,
                                     "font-name");

  font_desc = pango_font_description_from_string (font_name);
  g_free (font_name);

  pango_font_description_set_weight (font_desc, PANGO_WEIGHT_MEDIUM);

  font_size = PANGO_SCALE * size * 0.5;
  pango_font_description_set_absolute_size (font_desc, font_size);

  layout = pango_cairo_create_layout (cr);

  pango_layout_set_font_description (layout, font_desc);
  pango_font_description_free (font_desc);

  short_name = gf_input_source_get_short_name (sources->current_source);
  pango_layout_set_text (layout, short_name, -1);

  pango_layout_get_pixel_size (layout, &text_width, &text_height);

  factor = MIN ((size - (size * 0.1) * 2) / text_width, 1.0);
  cairo_scale (cr, factor, factor);

  center = size / 2.0;
  x = center - text_width * factor / 2.0;
  y = center - text_height * factor / 2.0;
  cairo_move_to (cr, x, y);

  color = g_settings_get_string (sources->input_sources_settings,
                                 "status-icon-fg-color");

  gdk_rgba_parse (&rgba, color);
  g_free (color);

  gdk_cairo_set_source_rgba (cr, &rgba);

  pango_cairo_show_layout (cr, layout);
  g_object_unref (layout);
}

static void
update_status_icon_pixbuf (GfInputSources *sources)
{
  gint size;
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkPixbuf *pixbuf;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  size = gtk_status_icon_get_size (sources->status_icon);
  G_GNUC_END_IGNORE_DEPRECATIONS

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);
  cr = cairo_create (surface);

  draw_background (sources, cr, size);
  draw_text (sources, cr, size);

  cairo_destroy (cr);

  pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, size, size);
  cairo_surface_destroy (surface);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_from_pixbuf (sources->status_icon, pixbuf);
  G_GNUC_END_IGNORE_DEPRECATIONS

  g_object_unref (pixbuf);
}

static void
update_status_icon (GfInputSources *sources)
{
  GfInputSourceManager *manager;
  GfInputSource *source;
  const gchar *display_name;

  manager = sources->input_source_manager;

  g_clear_object (&sources->current_source);
  source = gf_input_source_manager_get_current_source (manager);

  if (source == NULL)
    {
      g_clear_object (&sources->status_icon);
      return;
    }

  sources->current_source = g_object_ref (source);

  if (sources->status_icon == NULL)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      sources->status_icon = gtk_status_icon_new ();
      G_GNUC_END_IGNORE_DEPRECATIONS

      g_signal_connect_swapped (sources->status_icon, "size-changed",
                                G_CALLBACK (update_status_icon), sources);
    }

  display_name = gf_input_source_get_display_name (source);
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_tooltip_text (sources->status_icon, display_name);
  G_GNUC_END_IGNORE_DEPRECATIONS

  update_status_icon_pixbuf (sources);
}

static void
sources_changed_cb (GfInputSourceManager *manager,
                    gpointer              user_data)
{
}

static void
current_source_changed_cb (GfInputSourceManager *manager,
                           GfInputSource        *old_source,
                           gpointer              user_data)
{
  GfInputSources *sources;

  sources = GF_INPUT_SOURCES (user_data);

  update_status_icon (sources);
}

static void
status_icon_bg_color_changed_cb (GSettings *settings,
                                 gchar     *key,
                                 gpointer   user_data)
{
  GfInputSources *sources;

  sources = GF_INPUT_SOURCES (user_data);

  update_status_icon (sources);
}

static void
status_icon_fg_color_changed_cb (GSettings *settings,
                                 gchar     *key,
                                 gpointer   user_data)
{
  GfInputSources *sources;

  sources = GF_INPUT_SOURCES (user_data);

  update_status_icon (sources);
}

static void
font_name_changed_cb (GSettings *settings,
                      gchar     *key,
                      gpointer   user_data)
{
  GfInputSources *sources;

  sources = GF_INPUT_SOURCES (user_data);

  update_status_icon (sources);
}

static void
gf_input_sources_dispose (GObject *object)
{
  GfInputSources *sources;

  sources = GF_INPUT_SOURCES (object);

  g_clear_object (&sources->ibus_manager);
  g_clear_object (&sources->input_source_manager);

  g_clear_object (&sources->input_sources_settings);
  g_clear_object (&sources->gnome_desktop_settings);

  g_clear_object (&sources->current_source);
  g_clear_object (&sources->status_icon);

  G_OBJECT_CLASS (gf_input_sources_parent_class)->dispose (object);
}

static void
gf_input_sources_class_init (GfInputSourcesClass *sources_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (sources_class);

  object_class->dispose = gf_input_sources_dispose;
}

static void
gf_input_sources_init (GfInputSources *sources)
{
  sources->ibus_manager = gf_ibus_manager_new ();
  sources->input_source_manager = gf_input_source_manager_new (sources->ibus_manager);

  sources->input_sources_settings = g_settings_new (INPUT_SOURCES_SCHEMA);
  sources->gnome_desktop_settings = g_settings_new (GNOME_DESKTOP_SCHEMA);

  g_signal_connect (sources->input_source_manager, "sources-changed",
                    G_CALLBACK (sources_changed_cb), sources);

  g_signal_connect (sources->input_source_manager, "current-source-changed",
                    G_CALLBACK (current_source_changed_cb), sources);

  {
    g_signal_connect (sources->input_sources_settings,
                      "changed::status-icon-bg-color",
                      G_CALLBACK (status_icon_bg_color_changed_cb),
                      sources);

    g_signal_connect (sources->input_sources_settings,
                      "changed::status-icon-fg-color",
                      G_CALLBACK (status_icon_fg_color_changed_cb),
                      sources);

    g_signal_connect (sources->gnome_desktop_settings, "changed::font-name",
                      G_CALLBACK (font_name_changed_cb), sources);

    update_status_icon (sources);
  }

  gf_input_source_manager_reload (sources->input_source_manager);
}

GfInputSources *
gf_input_sources_new (void)
{
  return g_object_new (GF_TYPE_INPUT_SOURCES, NULL);
}
