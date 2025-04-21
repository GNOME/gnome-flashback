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
#include "si-input-sources.h"

#include <glib/gi18n-lib.h>
#include <libgnome-panel/gp-image-menu-item.h>
#include <locale.h>
#include <utime.h>

#include "dbus/gf-input-sources-gen.h"
#include "si-desktop-menu-item.h"

struct _SiInputSources
{
  SiIndicator        parent;

  char              *icon_theme_path;

  GtkWidget         *menu;

  guint              name_id;

  GSettings         *settings;

  GCancellable      *cancellable;

  GfInputSourcesGen *input_sources;

  char              *icon_text;
  char              *icon_file;
};

G_DEFINE_TYPE (SiInputSources, si_input_sources, SI_TYPE_INDICATOR)

static GString *
cairo_path_to_string (cairo_path_t   *path,
                      cairo_matrix_t *matrix)
{
  char *locale;
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

            g_string_append_printf (string,
                                    "C %f,%f %f,%f %f,%f ",
                                    x1,
                                    y1,
                                    x2,
                                    y2,
                                    x3,
                                    y3);
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
get_pango_layout (const char *text,
                  const char *font_family,
                  int         font_weight,
                  int         font_size)
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
get_cairo_path (const char     *text,
                const char     *font_family,
                int             font_weight,
                int             font_size,
                cairo_matrix_t *matrix)
{
  PangoLayout *layout;
  cairo_surface_t *surface;
  cairo_t *cr;
  int width;
  int height;
  double scale;
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

static char *
generate_path_description (const char *text,
                           const char *font_family,
                           int         font_weight,
                           int         font_size)
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
generate_svg (const char *text,
              const char *font_family,
              int         font_weight,
              int         font_size,
              const char *bg_color,
              const char *fg_color,
              gboolean    symbolic)
{
  char *path_d;
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
      g_string_append_printf (svg,
                              "<path d='%s' style='fill:%s'/>",
                              path_d,
                              fg_color);
    }

  g_free (path_d);

  return g_string_append (svg, "</svg>");
}

static void
ensure_file_exists (const char *icon_theme_path,
                    const char *icon_name,
                    const char *text,
                    const char *font_family,
                    int         font_weight,
                    int         font_size,
                    const char *bg_color,
                    const char *fg_color,
                    gboolean    symbolic)
{
  char *filename;
  char *path;
  GFile *file;
  GFile *parent;
  GString *svg;
  GError *error;

  filename = g_strdup_printf ("%s.svg", icon_name);
  path = g_build_filename (icon_theme_path,
                           "hicolor",
                           "scalable",
                           "status",
                           filename,
                           NULL);

  g_free (filename);

  if (g_file_test (path, G_FILE_TEST_EXISTS))
    {
      g_free (path);
      return;
    }

  file = g_file_new_for_path (path);
  g_free (path);

  parent = g_file_get_parent (file);

  svg = generate_svg (text,
                      font_family,
                      font_weight,
                      font_size,
                      bg_color,
                      fg_color,
                      symbolic);

  g_file_make_directory_with_parents (parent, NULL, NULL);

  error = NULL;
  if (!g_file_replace_contents (file,
                                svg->str,
                                svg->len,
                                NULL,
                                FALSE,
                                G_FILE_CREATE_NONE,
                                NULL,
                                NULL,
                                &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  utime (icon_theme_path, NULL);
  gtk_icon_theme_rescan_if_needed (gtk_icon_theme_get_default ());

  g_string_free (svg, TRUE);
  g_object_unref (parent);
  g_object_unref (file);
}

static gchar *
generate_icon_name (const char *text,
                    const char *font_family,
                    int         font_weight,
                    int         font_size,
                    const char *bg_color,
                    const char *fg_color,
                    gboolean    symbolic)
{
  char *str;
  char *hash;
  GString *icon_name;

  str = g_strdup_printf ("%s-%s-%d-%d-%s-%s",
                         text,
                         font_family,
                         font_weight,
                         font_size,
                         bg_color,
                         fg_color);

  hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, str, -1);
  g_free (str);

  icon_name = g_string_new (hash);
  g_free (hash);

  if (symbolic)
    g_string_append (icon_name, "-symbolic");

  return g_string_free (icon_name, FALSE);
}

static gchar *
get_icon_name (SiInputSources *self)
{
  char *font_family;
  int font_weight;
  int font_size;
  char *bg_color;
  char *fg_color;
  GpApplet *applet;
  gboolean symbolic;
  char *icon_name;

  font_family = g_settings_get_string (self->settings, "icon-font-family");
  font_weight = g_settings_get_int (self->settings, "icon-font-weight");
  font_size = 8;
  bg_color = g_settings_get_string (self->settings, "icon-bg-color");
  fg_color = g_settings_get_string (self->settings, "icon-fg-color");

  applet = si_indicator_get_applet (SI_INDICATOR (self));
  symbolic = gp_applet_get_prefer_symbolic_icons (applet);

  icon_name = generate_icon_name (self->icon_text,
                                  font_family,
                                  font_weight,
                                  font_size,
                                  bg_color,
                                  fg_color,
                                  symbolic);

  ensure_file_exists (self->icon_theme_path,
                      icon_name,
                      self->icon_text,
                      font_family,
                      font_weight,
                      font_size,
                      bg_color,
                      fg_color,
                      symbolic);

  g_free (font_family);
  g_free (bg_color);
  g_free (fg_color);

  return icon_name;
}

static void
update_icon (SiInputSources *self)
{
  gboolean use_ibus_icon;

  if (self->icon_text == NULL && self->icon_file == NULL)
    return;

  use_ibus_icon = g_settings_get_boolean (self->settings,
                                          "use-ibus-icon-if-available");

  if (use_ibus_icon && self->icon_file != NULL)
    {
      si_indicator_set_icon_filename (SI_INDICATOR (self), self->icon_file);
    }
  else
    {
      char *icon_name;

      icon_name = get_icon_name (self);

      si_indicator_set_icon_name (SI_INDICATOR (self), icon_name);
      g_free (icon_name);
    }
}

static void
update_indicator_icon (SiInputSources *self,
                       GVariant       *current_source)
{
  GVariantDict dict;
  const char *icon_text;
  const char *icon_file;
  const char *tooltip;
  GtkWidget *item;

  g_variant_dict_init (&dict, current_source);

  if (!g_variant_dict_lookup (&dict, "icon-text", "&s", &icon_text))
    icon_text = NULL;

  if (!g_variant_dict_lookup (&dict, "icon-file", "&s", &icon_file))
    icon_file = NULL;

  if (!g_variant_dict_lookup (&dict, "tooltip", "&s", &tooltip))
    tooltip = NULL;

  g_clear_pointer (&self->icon_text, g_free);
  self->icon_text = g_strdup (icon_text);

  g_clear_pointer (&self->icon_file, g_free);
  self->icon_file = g_strdup (icon_file);

  item = si_indicator_get_menu_item (SI_INDICATOR (self));
  gtk_widget_set_tooltip_text (item , tooltip);

  update_icon (self);
}

static void
settings_changed_cb (GSettings      *settings,
                     const char     *key,
                     SiInputSources *self)
{
  update_icon (self);
}

static void
prefer_symbolic_icons_cb (GObject        *object,
                          GParamSpec     *pspec,
                          SiInputSources *self)
{
  update_icon (self);
}

static void
activate_cb (GObject      *object,
             GAsyncResult *res,
             gpointer      user_data)
{
  GError *error;

  error = NULL;
  gf_input_sources_gen_call_activate_finish (GF_INPUT_SOURCES_GEN (object),
                                             res,
                                             &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }
}

static void
item_activate_cb (GtkMenuItem    *item,
                  SiInputSources *self)
{
  guint *index;

  if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
    return;

  g_cancellable_cancel (self->cancellable);

  g_object_unref (self->cancellable);
  self->cancellable = g_cancellable_new ();

  index = g_object_get_data (G_OBJECT (item), "index");

  gf_input_sources_gen_call_activate (self->input_sources,
                                      *index,
                                      self->cancellable,
                                      activate_cb,
                                      self);
}

static int
append_input_sources (SiInputSources *self,
                      GVariant       *input_sources)
{
  GVariantIter iter;
  GSList *group;
  GVariant *child;

  g_variant_iter_init (&iter, input_sources);

  group = NULL;
  while ((child = g_variant_iter_next_value (&iter)))
    {
      guint index;
      const char *short_name;
      const char *display_name;
      gboolean active;
      GtkWidget *item;
      GtkWidget *hbox;
      GtkWidget *label;
      guint *data;

      g_variant_get (child,
                     "(u&s&sb)",
                     &index,
                     &short_name,
                     &display_name,
                     &active);

      item = gtk_radio_menu_item_new (group);
      gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), item);
      gtk_widget_show (item);

      group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), active);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_add (GTK_CONTAINER (item), hbox);
      gtk_widget_show (hbox);

      label = gtk_label_new (display_name);
      gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_widget_show (label);

      label = gtk_label_new (short_name);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_widget_show (label);

      data = g_new0 (guint, 1);
      g_object_set_data_full (G_OBJECT (item), "index", data, g_free);
      *data = index;

      g_signal_connect (item, "activate", G_CALLBACK (item_activate_cb), self);

      g_variant_unref (child);
    }

  return g_variant_iter_n_children (&iter);
}

static void
activate_property_cb (GObject      *object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GError *error;

  error = NULL;
  gf_input_sources_gen_call_activate_property_finish (GF_INPUT_SOURCES_GEN (object),
                                                      res,
                                                      &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }
}

static void
property_activate_cb (GtkMenuItem    *item,
                      SiInputSources *self)
{
  const char *key;

  key = g_object_get_data (G_OBJECT (item), "key");

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();

  gf_input_sources_gen_call_activate_property (self->input_sources,
                                               key,
                                               self->cancellable,
                                               activate_property_cb,
                                               self);
}

static void
append_properties_to_menu (SiInputSources *self,
                           GVariantIter   *iter,
                           GtkWidget      *menu)
{
  GVariant *child;

  while ((child = g_variant_iter_next_value (iter)))
    {
      const char *key;
      GVariant *ret;
      GVariantDict *dict;
      const char *type;
      const char *label;
      const char *tooltip;
      GtkWidget *item;

      g_variant_get (child, "(&s@a{sv})", &key, &ret);

      dict = g_variant_dict_new (ret);
      g_variant_unref (ret);

      if (!g_variant_dict_lookup (dict, "type", "&s", &type))
        {
          g_variant_dict_unref (dict);
          g_variant_unref (child);
          continue;
        }

      if (!g_variant_dict_lookup (dict, "label", "&s", &label))
        label = "";

      if (!g_variant_dict_lookup (dict, "tooltip", "&s", &tooltip))
        tooltip = NULL;

      if (g_strcmp0 (type, "toggle") == 0)
        {
          item = gtk_check_menu_item_new ();
        }
      else if (g_strcmp0 (type, "radio") == 0)
        {
          item = gtk_radio_menu_item_new (NULL);
        }
      else if (g_strcmp0 (type, "separator") == 0)
        {
          item = gtk_separator_menu_item_new ();
        }
      else
        {
          item = gtk_menu_item_new ();
        }

      gtk_menu_item_set_label (GTK_MENU_ITEM (item), label);
      gtk_widget_set_tooltip_text (item, tooltip);

      if (g_strcmp0 (type, "menu") == 0)
        {
          GtkWidget *submenu;
          GVariant *variant;
          GVariantIter subiter;

          submenu = gtk_menu_new ();
          gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

          variant = g_variant_dict_lookup_value (dict,
                                                 "menu",
                                                 G_VARIANT_TYPE ("a(sa{sv})"));

          if (variant != NULL)
            {
              g_variant_iter_init (&subiter, variant);
              append_properties_to_menu (self, &subiter, submenu);
              g_variant_unref (variant);
            }
          else
            {
              gtk_widget_hide (item);
            }
        }
      else if (g_strcmp0 (type, "toggle") == 0 ||
               g_strcmp0 (type, "radio") == 0)
        {
          const char *state;

          if (!g_variant_dict_lookup (dict, "state", "&s", &state))
            state = NULL;

          if (g_strcmp0 (state, "checked") == 0)
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
        }

      if (g_strcmp0 (type, "menu") != 0 &&
          g_strcmp0 (type, "separator") != 0)
        {
          g_object_set_data_full (G_OBJECT (item),
                                  "key",
                                  g_strdup (key),
                                  g_free);

          g_signal_connect (item,
                            "activate",
                            G_CALLBACK (property_activate_cb),
                            self);
        }

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      g_variant_dict_unref (dict);
      g_variant_unref (child);
    }
}

static int
append_properties (SiInputSources *self,
                   GVariant       *current_source)
{
  GVariantDict dict;
  GVariant *properties;
  GVariantIter iter;
  int items;

  g_variant_dict_init (&dict, current_source);

  properties = g_variant_dict_lookup_value (&dict,
                                            "properties",
                                            G_VARIANT_TYPE ("a(sa{sv})"));

  if (properties == NULL)
    return 0;

  items = g_variant_iter_init (&iter, properties);

  if (items > 0)
    {
      GtkWidget *separator;

      separator = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), separator);
      gtk_widget_show (separator);

      append_properties_to_menu (self, &iter, self->menu);
    }

  g_variant_unref (properties);

  return items;
}

static void
watch_child (GPid     pid,
             gint     status,
             gpointer user_data)
{
}

static void
spawn_keyboard_display (const char *description)
{
  char **argv;
  GSpawnFlags flags;
  GPid pid;
  GError *error;

  argv = g_new0 (gchar *, 3);
  flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;
  error = NULL;

  argv[0] = g_strdup ("tecla");
  argv[1] = g_strdup (description);
  argv[2] = NULL;

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
show_layout_cb (GtkMenuItem    *menuitem,
                SiInputSources *self)
{
  const char *description;

  description = g_object_get_data (G_OBJECT (menuitem), "description");
  if (description == NULL)
    return;

  spawn_keyboard_display (description);
}

static void
append_show_layout_item (SiInputSources *self,
                         GVariant       *current_source)
{
  GVariantDict dict;
  const char *layout;
  const char *layout_variant;
  GtkWidget *item;

  g_variant_dict_init (&dict, current_source);

  if (!g_variant_dict_lookup (&dict, "layout", "&s", &layout))
    layout = NULL;

  if (!g_variant_dict_lookup (&dict, "layout-variant", "&s", &layout_variant))
    layout_variant = NULL;

  item = gtk_menu_item_new_with_label (_("Show Keyboard Layout"));
  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), item);
  gtk_widget_show (item);

  g_signal_connect (item, "activate", G_CALLBACK (show_layout_cb), self);

  if (layout != NULL && *layout != '\0')
    {
      char *description;

      if (layout_variant != NULL && *layout_variant != '\0')
        description = g_strdup_printf ("%s\t%s", layout, layout_variant);
      else
        description = g_strdup (layout);

      g_object_set_data_full (G_OBJECT (item),
                              "description",
                              description,
                              g_free);
    }
  else
    {
      gtk_widget_set_sensitive (item, FALSE);
    }
}

static void
append_settings_item (SiInputSources *self)
{
  GtkWidget *item;

  item = si_desktop_menu_item_new (_("Region & Language Settings"),
                                   "gnome-region-panel.desktop");

  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), item);
  gtk_widget_show (item);
}

static void
remove_item_cb (GtkWidget *widget,
                gpointer   data)
{
  gtk_widget_destroy (widget);
}

static int
update_indicator_menu (SiInputSources *self,
                       GVariant       *input_sources,
                       GVariant       *current_source)
{
  int count;
  GtkWidget *separator;

  gtk_container_foreach (GTK_CONTAINER (self->menu), remove_item_cb, NULL);

  count = append_input_sources (self, input_sources);
  count += append_properties (self, current_source);

  separator = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), separator);
  gtk_widget_show (separator);

  append_show_layout_item (self, current_source);
  append_settings_item (self);

  return count;
}

static void
update_indicator (SiInputSources *self,
                  GVariant       *input_sources,
                  GVariant       *current_source)
{
  int count;
  GtkWidget *menu_item;

  update_indicator_icon (self, current_source);
  count = update_indicator_menu (self, input_sources, current_source);

  menu_item = si_indicator_get_menu_item (SI_INDICATOR (self));
  gtk_widget_set_visible (menu_item, count > 1);
}

static void
get_input_sources_cb (GObject      *object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GError *error;
  GVariant *input_sources;
  GVariant *current_source;
  SiInputSources *self;

  error = NULL;
  gf_input_sources_gen_call_get_input_sources_finish (GF_INPUT_SOURCES_GEN (object),
                                                      &input_sources,
                                                      &current_source,
                                                      res,
                                                      &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = SI_INPUT_SOURCES (user_data);

  update_indicator (self, input_sources, current_source);

  g_variant_unref (input_sources);
  g_variant_unref (current_source);
}

static void
changed_cb (GfInputSourcesGen *input_sources,
            SiInputSources    *self)
{
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();

  gf_input_sources_gen_call_get_input_sources (self->input_sources,
                                               self->cancellable,
                                               get_input_sources_cb,
                                               self);
}

static void
input_sources_ready_cb (GObject      *object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  GError *error;
  GfInputSourcesGen *input_sources;
  SiInputSources *self;

  error = NULL;
  input_sources = gf_input_sources_gen_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = SI_INPUT_SOURCES (user_data);
  self->input_sources = input_sources;

  g_signal_connect (self->input_sources, "changed",
                    G_CALLBACK (changed_cb), self);

  gf_input_sources_gen_call_get_input_sources (self->input_sources,
                                               self->cancellable,
                                               get_input_sources_cb,
                                               self);
}

static void
name_appeared_handler_cb (GDBusConnection *connection,
                          const gchar     *name,
                          const gchar     *name_owner,
                          gpointer         user_data)
{
  SiInputSources *self;

  self = SI_INPUT_SOURCES (user_data);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();

  gf_input_sources_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          "org.gnome.Flashback.InputSources",
                                          "/org/gnome/Flashback/InputSources",
                                          self->cancellable,
                                          input_sources_ready_cb,
                                          self);
}

static void
name_vanished_handler_cb (GDBusConnection *connection,
                          const gchar     *name,
                          gpointer         user_data)
{
  SiInputSources *self;
  GtkWidget *menu_item;

  self = SI_INPUT_SOURCES (user_data);

  g_clear_object (&self->input_sources);

  menu_item = si_indicator_get_menu_item (SI_INDICATOR (self));
  gtk_widget_hide (menu_item);
}

static void
si_input_sources_constructed (GObject *object)
{
  SiInputSources *self;
  GtkWidget *menu_item;
  GpApplet *applet;
  const char *schema;

  self = SI_INPUT_SOURCES (object);

  G_OBJECT_CLASS (si_input_sources_parent_class)->constructed (object);

  menu_item = si_indicator_get_menu_item (SI_INDICATOR (self));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), self->menu);

  applet = si_indicator_get_applet (SI_INDICATOR (self));
  schema = "org.gnome.gnome-flashback.system-indicators.input-sources";

  self->settings = gp_applet_settings_new (applet, schema);

  g_signal_connect (self->settings,
                    "changed",
                    G_CALLBACK (settings_changed_cb),
                    self);

  g_signal_connect (applet,
                    "notify::prefer-symbolic-icons",
                    G_CALLBACK (prefer_symbolic_icons_cb),
                    self);
}

static void
si_input_sources_dispose (GObject *object)
{
  SiInputSources *self;

  self = SI_INPUT_SOURCES (object);

  if (self->name_id != 0)
    {
      g_bus_unwatch_name (self->name_id);
      self->name_id = 0;
    }

  g_clear_object (&self->settings);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->input_sources);

  G_OBJECT_CLASS (si_input_sources_parent_class)->dispose (object);
}

static void
si_input_sources_finalize (GObject *object)
{
  SiInputSources *self;

  self = SI_INPUT_SOURCES (object);

  g_clear_pointer (&self->icon_theme_path, g_free);
  g_clear_pointer (&self->icon_text, g_free);
  g_clear_pointer (&self->icon_file, g_free);

  G_OBJECT_CLASS (si_input_sources_parent_class)->finalize (object);
}

static void
si_input_sources_class_init (SiInputSourcesClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = si_input_sources_constructed;
  object_class->dispose = si_input_sources_dispose;
  object_class->finalize = si_input_sources_finalize;
}

static void
si_input_sources_init (SiInputSources *self)
{
  self->icon_theme_path = g_build_filename (g_get_user_cache_dir (),
                                            "gnome-flashback",
                                            "system-indicators",
                                            "icons",
                                            NULL);

  gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                     self->icon_theme_path);

  self->menu = gtk_menu_new ();

  self->name_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                    "org.gnome.Flashback.InputSources",
                                    G_BUS_NAME_WATCHER_FLAGS_NONE,
                                    name_appeared_handler_cb,
                                    name_vanished_handler_cb,
                                    self,
                                    NULL);
}

SiIndicator *
si_input_sources_new (GpApplet *applet)
{
  return g_object_new (SI_TYPE_INPUT_SOURCES,
                       "applet", applet,
                       NULL);
}
