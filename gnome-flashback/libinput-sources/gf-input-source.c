/*
 * Copyright (C) 2015 Sebastian Geiger
 * Copyright (C) 2016 Alberts Muktupāvels
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

#include <cairo.h>
#include <gdk/gdk.h>
#include <locale.h>
#include <pango/pango.h>
#include <string.h>

#include "gf-ibus-manager.h"
#include "gf-input-source.h"

struct _GfInputSource
{
  GObject        parent;

  GfIBusManager *ibus_manager;

  gchar         *type;
  gchar         *id;
  gchar         *display_name;
  gchar         *short_name;
  guint          index;

  gchar         *xkb_id;

  IBusPropList  *prop_list;
};

enum
{
  SIGNAL_CHANGED,
  SIGNAL_ACTIVATE,

  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

enum
{
  PROP_0,

  PROP_IBUS_MANAGER,

  PROP_TYPE,
  PROP_ID,
  PROP_DISPLAY_NAME,
  PROP_SHORT_NAME,
  PROP_INDEX,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfInputSource, gf_input_source, G_TYPE_OBJECT)

static PangoLayout *
get_pango_layout (const gchar *text,
                  const gchar *font_family,
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
  pango_font_description_set_size (font_desc, font_size * PANGO_SCALE);
  pango_font_description_set_weight (font_desc, PANGO_WEIGHT_NORMAL);
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

  layout = get_pango_layout (text, font_family, font_size);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 16, 16);
  cr = cairo_create (surface);

  pango_layout_get_pixel_size (layout, &width, &height);

  scale = MIN (1.0, 14.0 / width);
  cairo_move_to (cr, (16 - width * scale) / 2.0, (16 - height * scale) / 2.0);
  cairo_scale (cr, scale, scale);

  pango_cairo_layout_path (cr, layout);
  path = cairo_copy_path (cr);
  cairo_get_matrix (cr, matrix);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
  g_object_unref (layout);

  return path;
}

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

static gchar *
generate_path_description (const gchar *text,
                           const gchar *font_family,
                           gint         font_size)
{
  cairo_path_t *path;
  cairo_matrix_t matrix;
  GString *string;

  path = get_cairo_path (text, font_family, font_size, &matrix);
  string = cairo_path_to_string (path, &matrix);
  cairo_path_destroy (path);

  return g_string_free (string, FALSE);
}

static gchar *
get_xkb_id (GfInputSource *source)
{
  IBusEngineDesc *engine_desc;
  const gchar *layout_variant;
  const gchar *layout;

  engine_desc = gf_ibus_manager_get_engine_desc (source->ibus_manager,
                                                 source->id);

  if (!engine_desc)
    return g_strdup (source->id);

  layout = ibus_engine_desc_get_layout (engine_desc);
  layout_variant = ibus_engine_desc_get_layout_variant (engine_desc);

  if (layout_variant && strlen (layout_variant) > 0)
    return g_strdup_printf ("%s+%s", layout, layout_variant);
  else
    return g_strdup (layout);
}

static void
gf_input_source_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GfInputSource *source;

  source = GF_INPUT_SOURCE (object);

  switch (prop_id)
    {
      case PROP_IBUS_MANAGER:
        g_value_set_object (value, source->ibus_manager);
        break;

      case PROP_TYPE:
        g_value_set_string (value, source->type);
        break;

      case PROP_ID:
        g_value_set_string (value, source->id);
        break;

      case PROP_DISPLAY_NAME:
        g_value_set_string (value, source->display_name);
        break;

      case PROP_SHORT_NAME:
        g_value_set_string (value, source->short_name);
        break;

      case PROP_INDEX:
        g_value_set_uint (value, source->index);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gf_input_source_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GfInputSource *source;

  source = GF_INPUT_SOURCE (object);

  switch (prop_id)
    {
      case PROP_IBUS_MANAGER:
        source->ibus_manager = g_value_get_object (value);
        break;

      case PROP_TYPE:
        source->type = g_value_dup_string (value);
        break;

      case PROP_ID:
        source->id = g_value_dup_string (value);
        break;

      case PROP_DISPLAY_NAME:
        source->display_name = g_value_dup_string (value);
        break;

      case PROP_SHORT_NAME:
        source->short_name = g_value_dup_string (value);
        break;

      case PROP_INDEX:
        source->index = g_value_get_uint (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gf_input_source_dispose (GObject *object)
{
  GfInputSource *source;

  source = GF_INPUT_SOURCE (object);

  g_clear_object (&source->prop_list);

  G_OBJECT_CLASS (gf_input_source_parent_class)->dispose (object);
}

static void
gf_input_source_finalize (GObject *object)
{
  GfInputSource *source;

  source = GF_INPUT_SOURCE (object);

  g_free (source->type);
  g_free (source->id);
  g_free (source->display_name);
  g_free (source->short_name);
  g_free (source->xkb_id);

  G_OBJECT_CLASS (gf_input_source_parent_class)->finalize (object);
}

static void
gf_input_source_constructed (GObject *object)
{
  GfInputSource *source;

  source = GF_INPUT_SOURCE (object);

  G_OBJECT_CLASS (gf_input_source_parent_class)->constructed (object);

  source->xkb_id = get_xkb_id (source);
}

static void
gf_input_source_class_init (GfInputSourceClass *source_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (source_class);

  object_class->constructed = gf_input_source_constructed;
  object_class->dispose = gf_input_source_dispose;
  object_class->finalize = gf_input_source_finalize;
  object_class->get_property = gf_input_source_get_property;
  object_class->set_property = gf_input_source_set_property;

  signals[SIGNAL_ACTIVATE] =
    g_signal_new ("activate", G_OBJECT_CLASS_TYPE (source_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                  G_TYPE_BOOLEAN);

  signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", G_OBJECT_CLASS_TYPE (source_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  properties[PROP_IBUS_MANAGER] =
    g_param_spec_object ("ibus-manager", "IBus Manager",
                         "The instance of IBus Manager used by the input-sources module",
                         GF_TYPE_IBUS_MANAGER,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_TYPE] =
    g_param_spec_string ("type", "type", "The type of the input source",
                         NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_ID] =
    g_param_spec_string ("id", "ID", "The ID of the input source",
                         NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name", "Display name",
                         "The display name of the input source",
                         NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_SHORT_NAME] =
    g_param_spec_string ("short-name", "Short name",
                         "The short name of the input source",
                         NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_INDEX] =
    g_param_spec_uint ("index", "Index", "The index of the input source",
                       0, G_MAXUINT, 0,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gf_input_source_init (GfInputSource *source)
{
}

GfInputSource *
gf_input_source_new (GfIBusManager *ibus_manager,
                     const gchar   *type,
                     const gchar   *id,
                     const gchar   *display_name,
                     const gchar   *short_name,
                     guint          index)
{
  return g_object_new (GF_TYPE_INPUT_SOURCE,
                       "ibus-manager", ibus_manager,
                       "type", type,
                       "id", id,
                       "display-name", display_name,
                       "short-name", short_name,
                       "index", index,
                       NULL);
}

const gchar *
gf_input_source_get_source_type (GfInputSource *source)
{
  return source->type;
}

const gchar *
gf_input_source_get_id (GfInputSource *source)
{
  return source->id;
}

const gchar *
gf_input_source_get_display_name (GfInputSource *source)
{
  return source->display_name;
}

const gchar *
gf_input_source_get_short_name (GfInputSource *source)
{
  return source->short_name;
}

void
gf_input_source_set_short_name  (GfInputSource *source,
                                 const gchar   *short_name)
{
  if (g_strcmp0 (source->short_name, short_name) == 0)
    return;

  g_free (source->short_name);
  source->short_name = g_strdup (short_name);

  g_signal_emit (source, signals[SIGNAL_CHANGED], 0);
}

guint
gf_input_source_get_index (GfInputSource *source)
{
  return source->index;
}

const gchar *
gf_input_source_get_xkb_id (GfInputSource *source)
{
  return source->xkb_id;
}

void
gf_input_source_activate (GfInputSource *source,
                          gboolean       interactive)
{
  g_signal_emit (source, signals[SIGNAL_ACTIVATE], 0, interactive);
}

IBusPropList *
gf_input_source_get_properties (GfInputSource *source)
{
  return source->prop_list;
}

void
gf_input_source_set_properties (GfInputSource *source,
                                IBusPropList  *prop_list)
{
  g_clear_object (&source->prop_list);

  if (prop_list != NULL)
    source->prop_list = g_object_ref (prop_list);
}

GString *
gf_input_source_generate_svg (GfInputSource *source,
                              const gchar   *font_family,
                              gint           font_size,
                              const gchar   *bg_color,
                              const gchar   *fg_color,
                              gboolean       symbolic)
{
  const gchar *short_name;
  gchar *path_d;
  GString *svg;

  short_name = gf_input_source_get_short_name (source);
  path_d = generate_path_description (short_name, font_family, font_size);
  svg = g_string_new ("<?xml version='1.0' encoding='utf-8' standalone='no'?>");

  g_string_append (svg,
                   "<svg xmlns='http://www.w3.org/2000/svg' "
                   "width='16' height='16' viewBox='0 0 16 16'>");

  if (symbolic)
    {
      g_string_append (svg, "<defs><mask id='m'>");
      g_string_append (svg, "<rect width='16' height='16' style='fill:#ffffff'/>");
      g_string_append_printf (svg, "<path d='%s' style='fill:#000000'/>", path_d);
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
