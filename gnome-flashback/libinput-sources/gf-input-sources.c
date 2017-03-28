/*
 * Copyright (C) 2015-2016 Alberts Muktupāvels
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
#include <locale.h>
#include <utime.h>

#include "gf-ibus-manager.h"
#include "gf-input-sources.h"
#include "gf-input-source-manager.h"
#include "gf-input-source.h"

#define STATUS_ICON_SCHEMA "org.gnome.gnome-flashback.input-sources.status-icon"

struct _GfInputSources
{
  GObject               parent;

  GfIBusManager        *ibus_manager;
  GfInputSourceManager *input_source_manager;

  GSettings            *status_icon_settings;

  gchar                *icon_theme_path;

  GfInputSource        *current_source;
  GtkStatusIcon        *status_icon;
};

G_DEFINE_TYPE (GfInputSources, gf_input_sources, G_TYPE_OBJECT)

static GString *
cairo_path_to_string (cairo_path_t   *path,
                      cairo_matrix_t *matrix)
{
  gchar *locale;
  GString *string;
  gint i;

  locale = g_strdup (setlocale (LC_NUMERIC, NULL));
  setlocale (LC_NUMERIC, "C");

  string = g_string_new (NULL);
  for (i = 0; i < path->num_data; i += path->data[i].header.length)
    {
      cairo_path_data_t *data;
      gdouble x1, y1;
      gdouble x2, y2;
      gdouble x3, y3;

      data = &path->data[i];

      switch (data->header.type)
        {
          case CAIRO_PATH_MOVE_TO:
            x1 = data[1].point.x;
            y1 = data[1].point.y;

            cairo_matrix_transform_point (matrix, &x1, &y1);

            g_string_append_printf (string, "M %f,%f ", x1, y1);
            break;

          case CAIRO_PATH_LINE_TO:
            x1 = data[1].point.x;
            y1 = data[1].point.y;

            cairo_matrix_transform_point (matrix, &x1, &y1);

            g_string_append_printf (string, "L %f,%f ", x1, y1);
            break;

          case CAIRO_PATH_CURVE_TO:
            x1 = data[1].point.x;
            y1 = data[1].point.y;
            x2 = data[2].point.x;
            y2 = data[2].point.y;
            x3 = data[3].point.x;
            y3 = data[3].point.y;

            cairo_matrix_transform_point (matrix, &x1, &y1);
            cairo_matrix_transform_point (matrix, &x2, &y2);
            cairo_matrix_transform_point (matrix, &x3, &y3);

            g_string_append_printf (string, "C %f,%f %f,%f %f,%f ",
                                    x1, y1, x2, y2, x3, y3);
            break;

          case CAIRO_PATH_CLOSE_PATH:
            g_string_append (string, "Z ");
            break;

          default:
            break;
        }
    }

  setlocale (LC_NUMERIC, locale);
  g_free (locale);

  return string;
}

static PangoLayout *
get_pango_layout (const gchar *text,
                  const gchar *font_family,
                  gint         font_weight,
                  gint         font_size)
{
  GdkScreen *screen;
  PangoContext *context;
  PangoFontDescription *font_desc;
  PangoLayout *layout;

  screen = gdk_screen_get_default ();
  context = gdk_pango_context_get_for_screen (screen);
  font_desc = pango_font_description_new ();

  pango_font_description_set_family (font_desc, font_family);
  pango_font_description_set_absolute_size (font_desc, font_size * PANGO_SCALE);
  pango_font_description_set_weight (font_desc, font_weight);
  pango_font_description_set_stretch (font_desc, PANGO_STRETCH_NORMAL);
  pango_font_description_set_style (font_desc, PANGO_STYLE_NORMAL);
  pango_font_description_set_variant (font_desc, PANGO_VARIANT_NORMAL);

  layout = pango_layout_new (context);
  g_object_unref (context);

  pango_layout_set_text (layout, text, -1);
  pango_layout_set_font_description (layout, font_desc);
  pango_font_description_free (font_desc);

  return layout;
}

static cairo_path_t *
get_cairo_path (const gchar    *text,
                const gchar    *font_family,
                gint            font_weight,
                gint            font_size,
                cairo_matrix_t *matrix)
{
  PangoLayout *layout;
  cairo_surface_t *surface;
  cairo_t *cr;
  gint width;
  gint height;
  gdouble scale;
  cairo_path_t *path;

  layout = get_pango_layout (text, font_family, font_weight, font_size);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 16, 16);
  cr = cairo_create (surface);

  pango_layout_get_pixel_size (layout, &width, &height);

  scale = MIN (1.0, MIN (14.0 / width, 14.0 / height));
  cairo_scale (cr, scale, scale);

  cairo_move_to (cr, (16 - width * scale) / 2.0, (16 - height * scale) / 2.0);

  pango_cairo_layout_path (cr, layout);
  path = cairo_copy_path (cr);
  cairo_get_matrix (cr, matrix);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
  g_object_unref (layout);

  return path;
}

static gchar *
generate_path_description (const gchar *text,
                           const gchar *font_family,
                           gint         font_weight,
                           gint         font_size)
{
  cairo_path_t *path;
  cairo_matrix_t matrix;
  GString *string;

  path = get_cairo_path (text, font_family, font_weight, font_size, &matrix);
  string = cairo_path_to_string (path, &matrix);
  cairo_path_destroy (path);

  return g_string_free (string, FALSE);
}

static GString *
generate_svg (const gchar *text,
              const gchar *font_family,
              gint         font_weight,
              gint         font_size,
              const gchar *bg_color,
              const gchar *fg_color,
              gboolean     symbolic)
{
  gchar *path_d;
  GString *svg;

  path_d = generate_path_description (text, font_family, font_weight, font_size);
  svg = g_string_new ("<?xml version='1.0' encoding='utf-8' standalone='no'?>");

  g_string_append (svg,
                   "<svg xmlns='http://www.w3.org/2000/svg' "
                   "width='16' height='16' viewBox='0 0 16 16'>");

  if (symbolic)
    {
      g_string_append (svg, "<defs><mask id='m'>");
      g_string_append (svg,
                       "<rect width='16' height='16' "
                       "style='fill:#ffffff!important'/>");

      g_string_append_printf (svg,
                              "<path d='%s' style='fill:#000000!important'/>",
                              path_d);

      g_string_append (svg, "</mask></defs>");
    }

  g_string_append_printf (svg,
                          "<rect x='0' y='0' width='16' height='16' "
                          "rx='2.0' ry='2.0' mask='%s' style='fill:%s;'/>",
                          symbolic ? "url(#m)" : "none",
                          symbolic ? "#bebebe" : bg_color);

  if (!symbolic)
    {
      g_string_append_printf (svg, "<path d='%s' style='fill:%s'/>",
                              path_d, fg_color);
    }

  g_free (path_d);

  return g_string_append (svg, "</svg>");
}

static void
ensure_file_exists (const gchar *icon_theme_path,
                    const gchar *icon_name,
                    const gchar *text,
                    const gchar *font_family,
                    gint         font_weight,
                    gint         font_size,
                    const gchar *bg_color,
                    const gchar *fg_color,
                    gboolean     symbolic)
{
  gchar *filename;
  gchar *path;

  filename = g_strdup_printf ("%s.svg", icon_name);
  path = g_build_filename (icon_theme_path, "hicolor", "scalable",
                           "status", filename, NULL);

  if (!g_file_test (path, G_FILE_TEST_EXISTS))
    {
      GFile *file;
      GFile *parent;
      GString *svg;

      file = g_file_new_for_path (path);
      parent = g_file_get_parent (file);
      svg = generate_svg (text, font_family, font_weight, font_size,
                          bg_color, fg_color, symbolic);

      g_file_make_directory_with_parents (parent, NULL, NULL);
      g_file_replace_contents (file, svg->str, svg->len, NULL, FALSE,
                               G_FILE_CREATE_NONE, NULL, NULL, NULL);

      utime (icon_theme_path, NULL);
      gtk_icon_theme_rescan_if_needed (gtk_icon_theme_get_default ());

      g_string_free (svg, TRUE);
      g_object_unref (parent);
      g_object_unref (file);
    }

  g_free (filename);
  g_free (path);
}

static gchar *
generate_icon_name (const gchar *text,
                    const gchar *font_family,
                    gint         font_weight,
                    gint         font_size,
                    const gchar *bg_color,
                    const gchar *fg_color,
                    gboolean     symbolic)
{
  gchar *str;
  gchar *hash;
  GString *icon_name;

  str = g_strdup_printf ("%s-%s-%d-%d-%s-%s", text,
                         font_family, font_weight, font_size,
                         bg_color, fg_color);

  hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, str, -1);
  g_free (str);

  icon_name = g_string_new (hash);
  g_free (hash);

  if (symbolic)
    g_string_append (icon_name, "-symbolic");

  return g_string_free (icon_name, FALSE);
}

static gchar *
get_icon_name (GfInputSources *sources)
{
  const gchar *text;
  gint font_size;
  gchar *font_family;
  gint font_weight;
  gchar *bg_color;
  gchar *fg_color;
  gboolean symbolic;
  gchar *icon_name;

  text = gf_input_source_get_short_name (sources->current_source);
  font_size = 8;

  font_family = g_settings_get_string (sources->status_icon_settings, "font-family");
  font_weight = g_settings_get_int (sources->status_icon_settings, "font-weight");
  bg_color = g_settings_get_string (sources->status_icon_settings, "bg-color");
  fg_color = g_settings_get_string (sources->status_icon_settings, "fg-color");
  symbolic = g_settings_get_boolean (sources->status_icon_settings, "symbolic");

  icon_name = generate_icon_name (text, font_family, font_weight, font_size,
                                  bg_color, fg_color, symbolic);

  ensure_file_exists (sources->icon_theme_path, icon_name, text,
                      font_family, font_weight, font_size,
                      bg_color, fg_color, symbolic);

  g_free (font_family);
  g_free (bg_color);
  g_free (fg_color);

  return icon_name;
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
spawn_keyboard_display (const gchar *description)
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

  spawn_keyboard_display (description);
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
  gchar *icon_name;

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
  icon_name = get_icon_name (sources);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_title (sources->status_icon, _("Keyboard"));
  gtk_status_icon_set_tooltip_text (sources->status_icon, display_name);
  gtk_status_icon_set_from_icon_name (sources->status_icon, icon_name);
  G_GNUC_END_IGNORE_DEPRECATIONS

  g_free (icon_name);
}

static void
sources_changed_cb (GfInputSourceManager *manager,
                    GfInputSources       *sources)
{
  update_status_icon (sources);
}

static void
current_source_changed_cb (GfInputSourceManager *manager,
                           GfInputSource        *old_source,
                           GfInputSources       *sources)
{
  update_status_icon (sources);
}

static void
status_icon_settings_changed_cb (GSettings      *settings,
                                 const gchar    *key,
                                 GfInputSources *sources)
{
  update_status_icon (sources);
}

static void
gf_input_sources_dispose (GObject *object)
{
  GfInputSources *sources;

  sources = GF_INPUT_SOURCES (object);

  g_clear_object (&sources->ibus_manager);
  g_clear_object (&sources->input_source_manager);

  g_clear_object (&sources->status_icon_settings);

  g_clear_object (&sources->current_source);
  g_clear_object (&sources->status_icon);

  G_OBJECT_CLASS (gf_input_sources_parent_class)->dispose (object);
}

static void
gf_input_sources_finalize (GObject *object)
{
  GfInputSources *sources;

  sources = GF_INPUT_SOURCES (object);

  g_clear_pointer (&sources->icon_theme_path, g_free);

  G_OBJECT_CLASS (gf_input_sources_parent_class)->finalize (object);
}

static void
gf_input_sources_class_init (GfInputSourcesClass *sources_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (sources_class);

  object_class->dispose = gf_input_sources_dispose;
  object_class->finalize = gf_input_sources_finalize;
}

static void
gf_input_sources_init (GfInputSources *sources)
{
  const gchar *cache_dir;

  sources->ibus_manager = gf_ibus_manager_new ();
  sources->input_source_manager = gf_input_source_manager_new (sources->ibus_manager);

  sources->status_icon_settings = g_settings_new (STATUS_ICON_SCHEMA);

  cache_dir = g_get_user_cache_dir ();
  sources->icon_theme_path = g_build_filename (cache_dir, "gnome-flashback",
                                               "input-sources", "icons",
                                               NULL);

  gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                     sources->icon_theme_path);

  g_signal_connect (sources->input_source_manager, "sources-changed",
                    G_CALLBACK (sources_changed_cb), sources);

  g_signal_connect (sources->input_source_manager, "current-source-changed",
                    G_CALLBACK (current_source_changed_cb), sources);

  g_signal_connect (sources->status_icon_settings, "changed",
                    G_CALLBACK (status_icon_settings_changed_cb), sources);

  gf_input_source_manager_reload (sources->input_source_manager);
}

GfInputSources *
gf_input_sources_new (void)
{
  return g_object_new (GF_TYPE_INPUT_SOURCES, NULL);
}
