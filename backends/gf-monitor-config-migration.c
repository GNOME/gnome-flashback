/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013, 2017 Red Hat Inc.
 * Copyright (C) 2017 Alberts Muktupāvels
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
 *
 * Adapted from mutter:
 * - src/backends/meta-monitor-config-migration.c
 */

/*
 * Portions of this file are derived from:
 * - gnome-desktop/libgnome-desktop/gnome-rr-config.c
 *
 * Copyright 2007, 2008, Red Hat, Inc.
 * Copyright 2010 Giovanni Campagna
 *
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

#include "config.h"

#include <gio/gio.h>
#include <string.h>

#include "gf-monitor-config-manager-private.h"
#include "gf-monitor-config-migration-private.h"
#include "gf-monitor-config-store-private.h"
#include "gf-monitor-spec-private.h"

typedef struct
{
  gchar *connector;
  gchar *vendor;
  gchar *product;
  gchar *serial;
} GfOutputKey;

typedef struct
{
  gboolean           enabled;
  GfRectangle        rect;
  gfloat             refresh_rate;
  GfMonitorTransform transform;

  gboolean           is_primary;
  gboolean           is_presentation;
  gboolean           is_underscanning;
} GfOutputConfig;

typedef struct
{
  GfOutputKey    *keys;
  GfOutputConfig *outputs;
  guint           n_outputs;
} GfLegacyMonitorsConfig;

enum
{
  GF_MONITORS_CONFIG_MIGRATION_ERROR_NOT_TILED,
  GF_MONITORS_CONFIG_MIGRATION_ERROR_NOT_MAIN_TILE
} GfConfigMigrationError;

#define GF_MONITORS_CONFIG_MIGRATION_ERROR (gf_monitors_config_migration_error_quark ())
static GQuark gf_monitors_config_migration_error_quark (void);

G_DEFINE_QUARK (gf-monitors-config-migration-error-quark,
                gf_monitors_config_migration_error)

typedef struct
{
  GfOutputKey    *output_key;
  GfOutputConfig *output_config;
} MonitorTile;

typedef enum
{
  STATE_INITIAL,
  STATE_MONITORS,
  STATE_CONFIGURATION,
  STATE_OUTPUT,
  STATE_OUTPUT_FIELD,
  STATE_CLONE
} ParserState;

typedef struct
{
  ParserState     state;
  gint            unknown_count;

  GArray         *key_array;
  GArray         *output_array;
  GfOutputKey     key;
  GfOutputConfig  output;

  gchar          *output_field;

  GHashTable     *configs;
} ConfigParser;

static GfLegacyMonitorsConfig *
legacy_config_new (void)
{
  return g_new0 (GfLegacyMonitorsConfig, 1);
}

static void
legacy_config_free (gpointer data)
{
  GfLegacyMonitorsConfig *config = data;

  g_free (config->keys);
  g_free (config->outputs);
  g_free (config);
}

static gulong
output_key_hash (const GfOutputKey *key)
{
  return (g_str_hash (key->connector) ^
          g_str_hash (key->vendor) ^
          g_str_hash (key->product) ^
          g_str_hash (key->serial));
}

static gboolean
output_key_equal (const GfOutputKey *one,
                  const GfOutputKey *two)
{
  return (strcmp (one->connector, two->connector) == 0 &&
          strcmp (one->vendor, two->vendor) == 0 &&
          strcmp (one->product, two->product) == 0 &&
          strcmp (one->serial, two->serial) == 0);
}

static guint
legacy_config_hash (gconstpointer data)
{
  const GfLegacyMonitorsConfig *config = data;
  guint i, hash;

  hash = 0;
  for (i = 0; i < config->n_outputs; i++)
    hash ^= output_key_hash (&config->keys[i]);

  return hash;
}

static gboolean
legacy_config_equal (gconstpointer one,
                     gconstpointer two)
{
  const GfLegacyMonitorsConfig *c_one = one;
  const GfLegacyMonitorsConfig *c_two = two;
  guint i;
  gboolean ok;

  if (c_one->n_outputs != c_two->n_outputs)
    return FALSE;

  ok = TRUE;
  for (i = 0; i < c_one->n_outputs && ok; i++)
    ok = output_key_equal (&c_one->keys[i], &c_two->keys[i]);

  return ok;
}

static void
free_output_key (GfOutputKey *key)
{
  g_free (key->connector);
  g_free (key->vendor);
  g_free (key->product);
  g_free (key->serial);
}

static void
handle_start_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **attribute_names,
                      const gchar         **attribute_values,
                      gpointer              user_data,
                      GError              **error)
{
  ConfigParser *parser = user_data;

  switch (parser->state)
    {
      case STATE_INITIAL:
        {
          gchar *version;

          if (strcmp (element_name, "monitors") != 0)
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                           "Invalid document element %s", element_name);
              return;
            }

          if (!g_markup_collect_attributes (element_name,
                                            attribute_names,
                                            attribute_values,
                                            error,
                                            G_MARKUP_COLLECT_STRING,
                                            "version", &version,
                                            G_MARKUP_COLLECT_INVALID))
            return;

          if (strcmp (version, "1") != 0)
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid or unsupported version %s", version);
              return;
            }

          parser->state = STATE_MONITORS;
          return;
        }

      case STATE_MONITORS:
        {
          if (strcmp (element_name, "configuration") != 0)
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                           "Invalid toplevel element %s", element_name);
              return;
            }

          parser->key_array = g_array_new (FALSE, FALSE,
                                           sizeof (GfOutputKey));
          parser->output_array = g_array_new (FALSE, FALSE,
                                              sizeof (GfOutputConfig));
          parser->state = STATE_CONFIGURATION;
          return;
        }

      case STATE_CONFIGURATION:
        {
          if (strcmp (element_name, "clone") == 0 &&
              parser->unknown_count == 0)
            {
              parser->state = STATE_CLONE;
            }
          else if (strcmp (element_name, "output") == 0 &&
                   parser->unknown_count == 0)
            {
              gchar *name;

              if (!g_markup_collect_attributes (element_name,
                                                attribute_names,
                                                attribute_values,
                                                error,
                                                G_MARKUP_COLLECT_STRING,
                                                "name", &name,
                                                G_MARKUP_COLLECT_INVALID))
                return;

              memset (&parser->key, 0, sizeof (GfOutputKey));
              memset (&parser->output, 0, sizeof (GfOutputConfig));

              parser->key.connector = g_strdup (name);
              parser->state = STATE_OUTPUT;
            }
          else
            {
              parser->unknown_count++;
            }

          return;
        }

      case STATE_OUTPUT:
        {
          if ((strcmp (element_name, "vendor") == 0 ||
               strcmp (element_name, "product") == 0 ||
               strcmp (element_name, "serial") == 0 ||
               strcmp (element_name, "width") == 0 ||
               strcmp (element_name, "height") == 0 ||
               strcmp (element_name, "rate") == 0 ||
               strcmp (element_name, "x") == 0 ||
               strcmp (element_name, "y") == 0 ||
               strcmp (element_name, "rotation") == 0 ||
               strcmp (element_name, "reflect_x") == 0 ||
               strcmp (element_name, "reflect_y") == 0 ||
               strcmp (element_name, "primary") == 0 ||
               strcmp (element_name, "presentation") == 0 ||
               strcmp (element_name, "underscanning") == 0) &&
              parser->unknown_count == 0)
            {
              parser->state = STATE_OUTPUT_FIELD;

              parser->output_field = g_strdup (element_name);
            }
          else
            {
              parser->unknown_count++;
            }

          return;
        }

      case STATE_CLONE:
      case STATE_OUTPUT_FIELD:
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Unexpected element %s", element_name);
          return;
        }

      default:
        g_assert_not_reached ();
    }
}

static void
handle_end_element (GMarkupParseContext  *context,
                    const gchar          *element_name,
                    gpointer              user_data,
                    GError              **error)
{
  ConfigParser *parser = user_data;

  switch (parser->state)
    {
      case STATE_MONITORS:
        {
          parser->state = STATE_INITIAL;
          return;
        }

      case STATE_CONFIGURATION:
        {
          if (strcmp (element_name, "configuration") == 0 &&
              parser->unknown_count == 0)
            {
              GfLegacyMonitorsConfig *config = legacy_config_new ();

              g_assert (parser->key_array->len == parser->output_array->len);

              config->n_outputs = parser->key_array->len;
              config->keys = (void *) g_array_free (parser->key_array, FALSE);
              config->outputs = (void *) g_array_free (parser->output_array, FALSE);

              g_hash_table_replace (parser->configs, config, config);

              parser->key_array = NULL;
              parser->output_array = NULL;
              parser->state = STATE_MONITORS;
            }
          else
            {
              parser->unknown_count--;

              g_assert (parser->unknown_count >= 0);
            }

          return;
        }

      case STATE_OUTPUT:
        {
          if (strcmp (element_name, "output") == 0 && parser->unknown_count == 0)
            {
              if (parser->key.vendor == NULL ||
                  parser->key.product == NULL ||
                  parser->key.serial == NULL)
                {
                  /* Disconnected output, ignore */
                  free_output_key (&parser->key);
                }
              else
                {
                  if (parser->output.rect.width == 0 ||
                      parser->output.rect.height == 0)
                    parser->output.enabled = FALSE;
                  else
                    parser->output.enabled = TRUE;

                  g_array_append_val (parser->key_array, parser->key);
                  g_array_append_val (parser->output_array, parser->output);
                }

              memset (&parser->key, 0, sizeof (GfOutputKey));
              memset (&parser->output, 0, sizeof (GfOutputConfig));

              parser->state = STATE_CONFIGURATION;
            }
          else
            {
              parser->unknown_count--;

              g_assert (parser->unknown_count >= 0);
            }

          return;
        }

      case STATE_CLONE:
        {
          parser->state = STATE_CONFIGURATION;
          return;
        }

      case STATE_OUTPUT_FIELD:
        {
          g_free (parser->output_field);
          parser->output_field = NULL;

          parser->state = STATE_OUTPUT;
          return;
        }

      case STATE_INITIAL:
      default:
        g_assert_not_reached ();
    }
}

static void
read_int (const gchar  *text,
          gsize         text_len,
          gint         *field,
          GError      **error)
{
  gchar buf[64];
  gint64 v;
  gchar *end;

  strncpy (buf, text, text_len);
  buf[MIN (63, text_len)] = 0;

  v = g_ascii_strtoll (buf, &end, 10);

  /* Limit reasonable values (actual limits are a lot smaller that these) */
  if (*end || v < 0 || v > G_MAXINT16)
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                 "Expected a number, got %s", buf);
  else
    *field = v;
}

static void
read_float (const gchar  *text,
            gsize         text_len,
            gfloat       *field,
            GError      **error)
{
  gchar buf[64];
  gfloat v;
  gchar *end;

  strncpy (buf, text, text_len);
  buf[MIN (63, text_len)] = 0;

  v = g_ascii_strtod (buf, &end);

  /* Limit reasonable values (actual limits are a lot smaller that these) */
  if (*end)
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                 "Expected a number, got %s", buf);
  else
    *field = v;
}

static gboolean
read_bool (const gchar  *text,
           gsize         text_len,
           GError      **error)
{
  if (strncmp (text, "no", text_len) == 0)
    return FALSE;
  else if (strncmp (text, "yes", text_len) == 0)
    return TRUE;
  else
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                 "Invalid boolean value %.*s", (int)text_len, text);

  return FALSE;
}

static gboolean
is_all_whitespace (const gchar *text,
                   gsize        text_len)
{
  gsize i;

  for (i = 0; i < text_len; i++)
    if (!g_ascii_isspace (text[i]))
      return FALSE;

  return TRUE;
}

static void
handle_text (GMarkupParseContext  *context,
             const gchar          *text,
             gsize                 text_len,
             gpointer              user_data,
             GError              **error)
{
  ConfigParser *parser = user_data;

  switch (parser->state)
    {
      case STATE_MONITORS:
        {
          if (!is_all_whitespace (text, text_len))
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Unexpected content at this point");
          return;
        }

      case STATE_CONFIGURATION:
        {
          if (parser->unknown_count == 0)
            {
              if (!is_all_whitespace (text, text_len))
                g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                             "Unexpected content at this point");
            }
          else
            {
              /* Handling unknown element, ignore */
            }

          return;
        }

      case STATE_OUTPUT:
        {
          if (parser->unknown_count == 0)
            {
              if (!is_all_whitespace (text, text_len))
                g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                             "Unexpected content at this point");
            }
          else
            {
              /* Handling unknown element, ignore */
            }
          return;
        }

      case STATE_CLONE:
        {
          /* Ignore the clone flag */
          return;
        }

      case STATE_OUTPUT_FIELD:
        {
          if (strcmp (parser->output_field, "vendor") == 0)
            parser->key.vendor = g_strndup (text, text_len);
          else if (strcmp (parser->output_field, "product") == 0)
            parser->key.product = g_strndup (text, text_len);
          else if (strcmp (parser->output_field, "serial") == 0)
            parser->key.serial = g_strndup (text, text_len);
          else if (strcmp (parser->output_field, "width") == 0)
            read_int (text, text_len, &parser->output.rect.width, error);
          else if (strcmp (parser->output_field, "height") == 0)
            read_int (text, text_len, &parser->output.rect.height, error);
          else if (strcmp (parser->output_field, "rate") == 0)
            read_float (text, text_len, &parser->output.refresh_rate, error);
          else if (strcmp (parser->output_field, "x") == 0)
            read_int (text, text_len, &parser->output.rect.x, error);
          else if (strcmp (parser->output_field, "y") == 0)
            read_int (text, text_len, &parser->output.rect.y, error);
          else if (strcmp (parser->output_field, "rotation") == 0)
            {
              if (strncmp (text, "normal", text_len) == 0)
                parser->output.transform = GF_MONITOR_TRANSFORM_NORMAL;
              else if (strncmp (text, "left", text_len) == 0)
                parser->output.transform = GF_MONITOR_TRANSFORM_90;
              else if (strncmp (text, "upside_down", text_len) == 0)
                parser->output.transform = GF_MONITOR_TRANSFORM_180;
              else if (strncmp (text, "right", text_len) == 0)
                parser->output.transform = GF_MONITOR_TRANSFORM_270;
              else
                g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                             "Invalid rotation type %.*s", (gint) text_len, text);
            }
          else if (strcmp (parser->output_field, "reflect_x") == 0)
            parser->output.transform += read_bool (text, text_len, error) ? GF_MONITOR_TRANSFORM_FLIPPED : 0;
          else if (strcmp (parser->output_field, "reflect_y") == 0)
            {
              if (read_bool (text, text_len, error))
                g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                             "Y reflection is not supported");
            }
          else if (strcmp (parser->output_field, "primary") == 0)
            parser->output.is_primary = read_bool (text, text_len, error);
          else if (strcmp (parser->output_field, "presentation") == 0)
            parser->output.is_presentation = read_bool (text, text_len, error);
          else if (strcmp (parser->output_field, "underscanning") == 0)
            parser->output.is_underscanning = read_bool (text, text_len, error);
          else
            g_assert_not_reached ();
          return;
        }

      case STATE_INITIAL:
      default:
        g_assert_not_reached ();
    }
}

static const GMarkupParser config_parser =
  {
    .start_element = handle_start_element,
    .end_element = handle_end_element,
    .text = handle_text,
  };

static GHashTable *
load_config_file (GFile   *file,
                  GError **error)
{
  gchar *contents;
  gsize size;
  ConfigParser parser = { 0 };
  GMarkupParseContext *context;

  contents = NULL;
  size = 0;

  if (!g_file_load_contents (file, NULL, &contents, &size, NULL, error))
    return FALSE;

  parser.state = STATE_INITIAL;
  parser.unknown_count = 0;

  parser.configs = g_hash_table_new_full (legacy_config_hash,
                                          legacy_config_equal,
                                          legacy_config_free,
                                          NULL);

  context = g_markup_parse_context_new (&config_parser,
                                        G_MARKUP_TREAT_CDATA_AS_TEXT |
                                        G_MARKUP_PREFIX_ERROR_POSITION,
                                        &parser, NULL);

  if (!g_markup_parse_context_parse (context, contents, size, error))
    {
      if (parser.key_array)
        g_array_free (parser.key_array, TRUE);

      if (parser.output_array)
        g_array_free (parser.output_array, TRUE);

      free_output_key (&parser.key);
      g_free (parser.output_field);

      g_hash_table_destroy (parser.configs);

      g_markup_parse_context_free (context);
      g_free (contents);

      return NULL;
    }

  g_markup_parse_context_free (context);
  g_free (contents);

  return parser.configs;
}

static GfMonitorConfig *
create_monitor_config (GfOutputKey     *output_key,
                       GfOutputConfig  *output_config,
                       gint             mode_width,
                       gint             mode_height,
                       GError         **error)
{
  GfMonitorModeSpec *mode_spec;
  GfMonitorSpec *monitor_spec;
  GfMonitorConfig *monitor_config;

  mode_spec = g_new0 (GfMonitorModeSpec, 1);

  mode_spec->width = mode_width;
  mode_spec->height = mode_height;
  mode_spec->refresh_rate = output_config->refresh_rate;
  mode_spec->flags = GF_CRTC_MODE_FLAG_NONE;

  if (!gf_verify_monitor_mode_spec (mode_spec, error))
    {
      g_free (mode_spec);
      return NULL;
    }

  monitor_spec = g_new0 (GfMonitorSpec, 1);

  monitor_spec->connector = output_key->connector;
  monitor_spec->vendor = output_key->vendor;
  monitor_spec->product = output_key->product;
  monitor_spec->serial = output_key->serial;

  monitor_config = g_new0 (GfMonitorConfig, 1);

  monitor_config->monitor_spec = monitor_spec;
  monitor_config->mode_spec = mode_spec;
  monitor_config->enable_underscanning = output_config->is_underscanning;

  if (!gf_verify_monitor_config (monitor_config, error))
    {
      gf_monitor_config_free (monitor_config);
      return NULL;
    }

  return monitor_config;
}

static GfMonitorConfig *
try_derive_tiled_monitor_config (GfLegacyMonitorsConfig  *config,
                                 GfOutputKey             *output_key,
                                 GfOutputConfig          *output_config,
                                 GfMonitorConfigStore    *config_store,
                                 GfRectangle             *out_layout,
                                 GError                 **error)
{
  MonitorTile top_left_tile = { 0 };
  MonitorTile top_right_tile = { 0 };
  MonitorTile bottom_left_tile = { 0 };
  MonitorTile bottom_right_tile = { 0 };
  MonitorTile origin_tile = { 0 };
  GfMonitorTransform transform = output_config->transform;
  guint i;
  gint max_x = 0;
  gint min_x = INT_MAX;
  gint max_y = 0;
  gint min_y = INT_MAX;
  gint mode_width = 0;
  gint mode_height = 0;
  GfMonitorConfig *monitor_config;

  /*
   * In order to derive a monitor configuration for a tiled monitor,
   * try to find the origin tile, then combine the discovered output
   * tiles to given the configured transform a monitor mode.
   *
   * If the origin tile is not the main tile (tile always enabled
   * even for non-tiled modes), this will fail, but since infermation
   * about tiling is lost, there is no way to discover it.
   */

  for (i = 0; i < config->n_outputs; i++)
    {
      GfOutputKey *other_output_key = &config->keys[i];
      GfOutputConfig *other_output_config = &config->outputs[i];
      GfRectangle *rect;

      if (strcmp (output_key->vendor, other_output_key->vendor) != 0 ||
          strcmp (output_key->product, other_output_key->product) != 0 ||
          strcmp (output_key->serial, other_output_key->serial) != 0)
        continue;

      rect = &other_output_config->rect;

      min_x = MIN (min_x, rect->x);
      min_y = MIN (min_y, rect->y);
      max_x = MAX (max_x, rect->x + rect->width);
      max_y = MAX (max_y, rect->y + rect->height);

      if (min_x == rect->x && min_y == rect->y)
        {
          top_left_tile = (MonitorTile) {
            .output_key = other_output_key,
            .output_config = other_output_config
          };
        }

      if (max_x == rect->x + rect->width && min_y == rect->y)
        {
          top_right_tile = (MonitorTile) {
            .output_key = other_output_key,
            .output_config = other_output_config
          };
        }

      if (min_x == rect->x && max_y == rect->y + rect->height)
        {
          bottom_left_tile = (MonitorTile) {
            .output_key = other_output_key,
            .output_config = other_output_config
          };
        }

      if (max_x == rect->x + rect->width && max_y == rect->y + rect->height)
        {
          bottom_right_tile = (MonitorTile) {
            .output_key = other_output_key,
            .output_config = other_output_config
          };
        }
    }

  if (top_left_tile.output_key == bottom_right_tile.output_key)
    {
      g_set_error_literal (error,
                           GF_MONITORS_CONFIG_MIGRATION_ERROR,
                           GF_MONITORS_CONFIG_MIGRATION_ERROR_NOT_TILED,
                           "Not a tiled monitor");

      return NULL;
    }

  switch (transform)
    {
      case GF_MONITOR_TRANSFORM_NORMAL:
        origin_tile = top_left_tile;
        mode_width = max_x - min_x;
        mode_height = max_y - min_y;
        break;

      case GF_MONITOR_TRANSFORM_90:
        origin_tile = bottom_left_tile;
        mode_width = max_y - min_y;
        mode_height = max_x - min_x;
        break;

      case GF_MONITOR_TRANSFORM_180:
        origin_tile = bottom_right_tile;
        mode_width = max_x - min_x;
        mode_height = max_y - min_y;
        break;

      case GF_MONITOR_TRANSFORM_270:
        origin_tile = top_right_tile;
        mode_width = max_y - min_y;
        mode_height = max_x - min_x;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED:
        origin_tile = bottom_left_tile;
        mode_width = max_x - min_x;
        mode_height = max_y - min_y;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED_90:
        origin_tile = bottom_right_tile;
        mode_width = max_y - min_y;
        mode_height = max_x - min_x;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED_180:
        origin_tile = top_right_tile;
        mode_width = max_x - min_x;
        mode_height = max_y - min_y;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED_270:
        origin_tile = top_left_tile;
        mode_width = max_y - min_y;
        mode_height = max_x - min_x;
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  g_assert (origin_tile.output_key);
  g_assert (origin_tile.output_config);

  if (origin_tile.output_key != output_key)
    {
      g_set_error_literal (error,
                           GF_MONITORS_CONFIG_MIGRATION_ERROR,
                           GF_MONITORS_CONFIG_MIGRATION_ERROR_NOT_MAIN_TILE,
                           "Not the main tile");
      return NULL;
    }

  monitor_config = create_monitor_config (origin_tile.output_key,
                                          origin_tile.output_config,
                                          mode_width, mode_height,
                                          error);

  if (!monitor_config)
    return NULL;

  *out_layout = (GfRectangle) {
    .x = min_x,
    .y = min_y,
    .width = max_x - min_x,
    .height = max_y - min_y
  };

  return monitor_config;
}

static GfMonitorConfig *
derive_monitor_config (GfOutputKey     *output_key,
                       GfOutputConfig  *output_config,
                       GfRectangle     *out_layout,
                       GError         **error)
{
  gint mode_width;
  gint mode_height;
  GfMonitorConfig *monitor_config;

  if (gf_monitor_transform_is_rotated (output_config->transform))
    {
      mode_width = output_config->rect.height;
      mode_height = output_config->rect.width;
    }
  else
    {
      mode_width = output_config->rect.width;
      mode_height = output_config->rect.height;
    }

  monitor_config = create_monitor_config (output_key, output_config,
                                          mode_width, mode_height,
                                          error);

  if (!monitor_config)
    return NULL;

  *out_layout = output_config->rect;

  return monitor_config;
}

static GfLogicalMonitorConfig *
ensure_logical_monitor (GList          **logical_monitor_configs,
                        GfOutputConfig  *output_config,
                        GfRectangle     *layout)
{
  GfLogicalMonitorConfig *new_logical_monitor_config;
  GList *l;

  for (l = *logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config = l->data;

      if (gf_rectangle_equal (&logical_monitor_config->layout, layout))
        return logical_monitor_config;
    }

  new_logical_monitor_config = g_new0 (GfLogicalMonitorConfig, 1);

  new_logical_monitor_config->layout = *layout;
  new_logical_monitor_config->monitor_configs = NULL;
  new_logical_monitor_config->transform = output_config->transform;
  new_logical_monitor_config->scale = -1.0;
  new_logical_monitor_config->is_primary = output_config->is_primary;
  new_logical_monitor_config->is_presentation = output_config->is_presentation;

  *logical_monitor_configs = g_list_append (*logical_monitor_configs,
                                            new_logical_monitor_config);

  return new_logical_monitor_config;
}

static GList *
derive_logical_monitor_configs (GfLegacyMonitorsConfig  *config,
                                GfMonitorConfigStore    *config_store,
                                GError                 **error)
{
  GList *logical_monitor_configs;
  guint i;

  logical_monitor_configs = NULL;

  for (i = 0; i < config->n_outputs; i++)
    {
      GfOutputKey *output_key;
      GfOutputConfig *output_config;
      GfMonitorConfig *monitor_config;
      GfRectangle layout;
      GfLogicalMonitorConfig *logical_monitor_config;

      output_key = &config->keys[i];
      output_config = &config->outputs[i];
      monitor_config = NULL;

      if (!output_config->enabled)
        continue;

      if (output_key->vendor && g_strcmp0 (output_key->vendor, "unknown") != 0 &&
          output_key->product && g_strcmp0 (output_key->product, "unknown") != 0 &&
          output_key->serial && g_strcmp0 (output_key->serial, "unknown") != 0)
        {
          monitor_config = try_derive_tiled_monitor_config (config,
                                                            output_key,
                                                            output_config,
                                                            config_store,
                                                            &layout,
                                                            error);

          if (!monitor_config)
            {
              if ((*error)->domain == GF_MONITORS_CONFIG_MIGRATION_ERROR)
                {
                  gint error_code = (*error)->code;

                  g_clear_error (error);

                  switch (error_code)
                    {
                      case GF_MONITORS_CONFIG_MIGRATION_ERROR_NOT_TILED:
                        break;

                      case GF_MONITORS_CONFIG_MIGRATION_ERROR_NOT_MAIN_TILE:
                      default:
                        continue;
                    }
                }
              else
                {
                  g_list_free_full (logical_monitor_configs,
                                    (GDestroyNotify) gf_monitor_config_free);
                  return NULL;
                }
            }
        }

      if (!monitor_config)
        {
          monitor_config = derive_monitor_config (output_key, output_config,
                                                  &layout, error);
        }

      if (!monitor_config)
        {
          g_list_free_full (logical_monitor_configs,
                            (GDestroyNotify) gf_monitor_config_free);
          return NULL;
        }

      logical_monitor_config =
        ensure_logical_monitor (&logical_monitor_configs,
                                output_config, &layout);

      logical_monitor_config->monitor_configs =
        g_list_append (logical_monitor_config->monitor_configs, monitor_config);
    }

  if (!logical_monitor_configs)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Empty configuration");
      return NULL;
    }

  return logical_monitor_configs;
}

static char *
generate_config_name (GfLegacyMonitorsConfig *config)
{
  gchar **output_strings;
  guint i;
  gchar *key_name;

  output_strings = g_new0 (gchar *, config->n_outputs + 1);
  for (i = 0; i < config->n_outputs; i++)
    {
      GfOutputKey *output_key = &config->keys[i];

      output_strings[i] = g_strdup_printf ("%s:%s:%s:%s",
                                           output_key->connector,
                                           output_key->vendor,
                                           output_key->product,
                                           output_key->serial);
    }

  key_name = g_strjoinv (", ", output_strings);
  g_strfreev (output_strings);

  return key_name;
}

static GList *
find_disabled_monitor_specs (GfLegacyMonitorsConfig *legacy_config)
{
  GList *disabled_monitors = NULL;
  guint i;

  for (i = 0; i < legacy_config->n_outputs; i++)
    {
      GfOutputKey *output_key = &legacy_config->keys[i];
      GfOutputConfig *output_config = &legacy_config->outputs[i];
      GfMonitorSpec *monitor_spec;

      if (output_config->enabled)
        continue;

      monitor_spec = g_new0 (GfMonitorSpec, 1);
      *monitor_spec = (GfMonitorSpec) {
        .connector = output_key->connector,
        .vendor = output_key->vendor,
        .product = output_key->product,
        .serial = output_key->serial
      };

      disabled_monitors = g_list_prepend (disabled_monitors, monitor_spec);
    }

  return disabled_monitors;
}

static void
migrate_config (gpointer key,
                gpointer value,
                gpointer user_data)
{
  GfLegacyMonitorsConfig *legacy_config;
  GfMonitorConfigStore *config_store;
  GfMonitorManager *monitor_manager;
  GList *logical_monitor_configs;
  GError *error;
  GList *disabled_monitor_specs;
  GfLogicalMonitorLayoutMode layout_mode;
  GfMonitorsConfig *config;

  legacy_config = key;
  config_store = user_data;
  monitor_manager = gf_monitor_config_store_get_monitor_manager (config_store);

  error = NULL;
  logical_monitor_configs = derive_logical_monitor_configs (legacy_config,
                                                            config_store,
                                                            &error);

  if (!logical_monitor_configs)
    {
      gchar *config_name;

      config_name = generate_config_name (legacy_config);
      g_warning ("Failed to migrate monitor configuration for %s: %s",
                 config_name, error->message);

      g_free (config_name);
      return;
    }

  disabled_monitor_specs = find_disabled_monitor_specs (legacy_config);
  layout_mode = GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;

  config = gf_monitors_config_new_full (logical_monitor_configs,
                                        disabled_monitor_specs,
                                        layout_mode,
                                        GF_MONITORS_CONFIG_FLAG_MIGRATED);

  if (!gf_verify_monitors_config (config, monitor_manager, &error))
    {
      gchar *config_name;

      config_name = generate_config_name (legacy_config);
      g_warning ("Ignoring invalid monitor configuration for %s: %s",
                 config_name, error->message);

      g_free (config_name);
      g_object_unref (config);
      return;
    }

  gf_monitor_config_store_add (config_store, config);
}

gboolean
gf_migrate_old_monitors_config (GfMonitorConfigStore  *config_store,
                                GFile                 *in_file,
                                GError               **error)
{
  GHashTable *configs;

  configs = load_config_file (in_file, error);
  if (!configs)
    return FALSE;

  g_hash_table_foreach (configs, migrate_config, config_store);
  g_hash_table_destroy (configs);

  return TRUE;
}

gboolean
gf_migrate_old_user_monitors_config (GfMonitorConfigStore  *config_store,
                                     GError               **error)
{
  const gchar *config_dir;
  gchar *user_file_path;
  GFile *user_file;
  gchar *backup_path;
  GFile *backup_file;
  gboolean migrated;

  config_dir = g_get_user_config_dir ();

  user_file_path = g_build_filename (config_dir, "monitors.xml", NULL);
  user_file = g_file_new_for_path (user_file_path);

  backup_path = g_build_filename (config_dir, "monitors-v1-backup.xml", NULL);
  backup_file = g_file_new_for_path (backup_path);

  if (!g_file_copy (user_file, backup_file,
                    G_FILE_COPY_OVERWRITE | G_FILE_COPY_BACKUP,
                    NULL, NULL, NULL,
                    error))
    {
      g_warning ("Failed to make a backup of monitors.xml: %s", (*error)->message);
      g_clear_error (error);
    }

  migrated = gf_migrate_old_monitors_config (config_store, user_file, error);

  g_free (user_file_path);
  g_object_unref (user_file);

  g_free (backup_path);
  g_object_unref (backup_file);

  return migrated;
}

gboolean
gf_finish_monitors_config_migration (GfMonitorManager  *monitor_manager,
                                     GfMonitorsConfig  *config,
                                     GError           **error)
{
  GfMonitorConfigManager *config_manager;
  GfMonitorConfigStore *config_store;
  GfLogicalMonitorLayoutMode layout_mode;
  GList *l;

  config_manager = monitor_manager->config_manager;
  config_store = gf_monitor_config_manager_get_store (config_manager);

  layout_mode = gf_monitor_manager_get_default_layout_mode (monitor_manager);

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config;
      GfMonitorConfig *monitor_config;
      GfMonitorSpec *monitor_spec;
      GfMonitor *monitor;
      GfMonitorModeSpec *monitor_mode_spec;
      GfMonitorMode *monitor_mode;
      gfloat scale;

      logical_monitor_config = l->data;
      monitor_config = logical_monitor_config->monitor_configs->data;
      monitor_spec = monitor_config->monitor_spec;
      monitor = gf_monitor_manager_get_monitor_from_spec (monitor_manager, monitor_spec);
      monitor_mode_spec = monitor_config->mode_spec;
      monitor_mode = gf_monitor_get_mode_from_spec (monitor, monitor_mode_spec);

      if (!monitor_mode)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Mode not available on monitor");
          return FALSE;
        }

      scale = gf_monitor_manager_calculate_monitor_mode_scale (monitor_manager,
                                                               layout_mode,
                                                               monitor,
                                                               monitor_mode);

      logical_monitor_config->scale = scale;
    }

  config->layout_mode = layout_mode;
  config->flags &= ~GF_MONITORS_CONFIG_FLAG_MIGRATED;

  if (!gf_verify_monitors_config (config, monitor_manager, error))
    return FALSE;

  gf_monitor_config_store_add (config_store, config);

  return TRUE;
}
