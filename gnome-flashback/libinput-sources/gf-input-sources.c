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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgnome-desktop/gnome-xkb-info.h>
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

  if (size <= 0)
    return;

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
build_prop_section (GfInputSources *sources,
                    GtkMenu        *menu)
{
  IBusPropList *prop_list;

  prop_list = gf_input_source_get_properties (sources->current_source);

  if (!prop_list)
    return;

  /* FIXME: */
}

static void
watch_child (GPid     pid,
             gint     status,
             gpointer user_data)
{
}

static void
spawn_kayboard_display (const gchar *description)
{
  gchar **argv;
  GSpawnFlags flags;
  GPid pid;
  GError *error;

  argv = g_new0 (gchar *, 4);
  flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;
  error = NULL;

  argv[0] = g_strdup ("gkbd-keyboard-display");
  argv[1] = g_strdup ("-l");
  argv[2] = g_strdup (description);
  argv[3] = NULL;

  g_spawn_async (NULL, argv, NULL, flags, NULL, NULL, &pid, &error);
  g_strfreev (argv);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }

  g_child_watch_add (pid, watch_child, NULL);
}

static void
activate_cb (GtkMenuItem   *menuitem,
             GfInputSource *source)
{
  gf_input_source_activate (source, TRUE);
}

static void
show_layout_cb (GtkMenuItem *menuitem,
                gpointer     user_data)
{
  GfInputSources *sources;
  GfInputSource *source;
  const gchar *type;
  const gchar *id;
  GnomeXkbInfo *info;
  const gchar *xkb_layout;
  const gchar *xkb_variant;
  gchar *description;

  sources = GF_INPUT_SOURCES (user_data);
  source = sources->current_source;

  type = gf_input_source_get_source_type (source);
  id = gf_input_source_get_id (source);

  info = NULL;
  xkb_layout = "";
  xkb_variant = "";

  if (g_strcmp0 (type, INPUT_SOURCE_TYPE_XKB) == 0)
    {
      info = gnome_xkb_info_new ();

      gnome_xkb_info_get_layout_info (info, id, NULL, NULL,
                                      &xkb_layout, &xkb_variant);
    }
  else if (g_strcmp0 (type, INPUT_SOURCE_TYPE_IBUS) == 0)
    {
      IBusEngineDesc *engine_desc;

      engine_desc = gf_ibus_manager_get_engine_desc (sources->ibus_manager, id);

      if (engine_desc)
        {
          xkb_layout = ibus_engine_desc_get_layout (engine_desc);
          xkb_variant = ibus_engine_desc_get_layout_variant (engine_desc);
        }
    }

  if (!xkb_layout || *xkb_layout == '\0')
    return;

  if (xkb_variant && *xkb_variant != '\0')
    description = g_strdup_printf ("%s\t%s", xkb_layout, xkb_variant);
  else
    description = g_strdup (xkb_layout);

  g_clear_object (&info);

  spawn_kayboard_display (description);
  g_free (description);
}

static void
status_icon_activate_cb (GtkStatusIcon *status_icon,
                         gpointer       user_data)
{
  GfInputSources *sources;
  GtkWidget *menu;
  GfInputSourceManager *manager;
  GList *input_sources;
  GList *is;
  GtkWidget *item;
  GtkWidget *separator;

  sources = GF_INPUT_SOURCES (user_data);

  menu = gtk_menu_new ();

  manager = sources->input_source_manager;
  input_sources = gf_input_source_manager_get_input_sources (manager);

  for (is = input_sources; is != NULL; is = g_list_next (is))
    {
      GfInputSource *source;
      GtkWidget *hbox;
      const gchar *text;
      GtkWidget *label;

      source = GF_INPUT_SOURCE (is->data);

      item = gtk_check_menu_item_new ();

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_add (GTK_CONTAINER (item), hbox);

      text = gf_input_source_get_display_name (source);
      label = gtk_label_new (text);
      gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 10);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);

      text = gf_input_source_get_short_name (source);
      label = gtk_label_new (text);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);

      if (source == sources->current_source)
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
      gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (item), TRUE);

      g_signal_connect (item, "activate", G_CALLBACK (activate_cb), source);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

  g_list_free (input_sources);

  build_prop_section (sources, GTK_MENU (menu));

  separator = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), separator);

  item = gtk_menu_item_new_with_label (_("Show Keyboard Layout"));
  g_signal_connect (item, "activate", G_CALLBACK (show_layout_cb), sources);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  gtk_widget_show_all (menu);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                  gtk_status_icon_position_menu, status_icon,
                  0, gtk_get_current_event_time ());

  G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
update_status_icon (GfInputSources *sources)
{
  GfInputSourceManager *manager;
  GfInputSource *source;
  GList *input_sources;
  IBusPropList *prop_list;
  const gchar *display_name;

  manager = sources->input_source_manager;

  g_clear_object (&sources->current_source);
  source = gf_input_source_manager_get_current_source (manager);

  if (source == NULL)
    {
      g_clear_object (&sources->status_icon);
      return;
    }

  input_sources = gf_input_source_manager_get_input_sources (manager);
  prop_list = gf_input_source_get_properties (source);

  if (g_list_length (input_sources) < 2 && !prop_list)
    {
      g_list_free (input_sources);
      return;
    }
  else
    {
      g_list_free (input_sources);
    }

  sources->current_source = g_object_ref (source);

  if (sources->status_icon == NULL)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      sources->status_icon = gtk_status_icon_new ();
      G_GNUC_END_IGNORE_DEPRECATIONS

      g_signal_connect_swapped (sources->status_icon, "size-changed",
                                G_CALLBACK (update_status_icon), sources);
      g_signal_connect (sources->status_icon, "activate",
                        G_CALLBACK (status_icon_activate_cb), sources);
    }

  display_name = gf_input_source_get_display_name (source);
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_title (sources->status_icon, _("Keyboard"));
  gtk_status_icon_set_tooltip_text (sources->status_icon, display_name);
  G_GNUC_END_IGNORE_DEPRECATIONS

  update_status_icon_pixbuf (sources);
}

static void
sources_changed_cb (GfInputSourceManager *manager,
                    gpointer              user_data)
{
  GfInputSources *sources;

  sources = GF_INPUT_SOURCES (user_data);

  update_status_icon (sources);
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
