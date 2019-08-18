/*
 * Copyright (C) 2017 Red Hat
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
 * - src/backends/meta-monitor-config-store.c
 */

#include "config.h"

#include <gio/gio.h>
#include <math.h>
#include <string.h>

#include "gf-logical-monitor-config-private.h"
#include "gf-monitor-config-manager-private.h"
#include "gf-monitor-config-migration-private.h"
#include "gf-monitor-config-private.h"
#include "gf-monitor-config-store-private.h"
#include "gf-monitor-spec-private.h"

#define MONITORS_CONFIG_XML_FORMAT_VERSION 2

#define QUOTE1(a) #a
#define QUOTE(a) QUOTE1(a)

/*
 * Example configuration:
 *
 * <monitors version="2">
 *   <configuration>
 *     <logicalmonitor>
 *       <x>0</x>
 *       <y>0</y>
 *       <scale>1</scale>
 *       <monitor>
 *         <monitorspec>
 *           <connector>LVDS1</connector>
 *           <vendor>Vendor A</vendor>
 *           <product>Product A</product>
 *           <serial>Serial A</serial>
 *         </monitorspec>
 *         <mode>
 *           <width>1920</width>
 *           <height>1080</height>
 *           <rate>60.049972534179688</rate>
 *           <flag>interlace</flag>
 *         </mode>
 *       </monitor>
 *       <transform>
 *         <rotation>right</rotation>
 *         <flipped>no</flipped>
 *       </transform>
 *       <primary>yes</primary>
 *       <presentation>no</presentation>
 *     </logicalmonitor>
 *     <logicalmonitor>
 *       <x>1920</x>
 *       <y>1080</y>
 *       <monitor>
 *         <monitorspec>
 *           <connector>LVDS2</connector>
 *           <vendor>Vendor B</vendor>
 *           <product>Product B</product>
 *           <serial>Serial B</serial>
 *         </monitorspec>
 *         <mode>
 *           <width>1920</width>
 *           <height>1080</height>
 *           <rate>60.049972534179688</rate>
 *         </mode>
 *         <underscanning>yes</underscanning>
 *       </monitor>
 *       <presentation>yes</presentation>
 *     </logicalmonitor>
 *     <disabled>
 *       <monitorspec>
 *         <connector>LVDS3</connector>
 *         <vendor>Vendor C</vendor>
 *         <product>Product C</product>
 *         <serial>Serial C</serial>
 *       </monitorspec>
 *     </disabled>
 *   </configuration>
 * </monitors>
 */

struct _GfMonitorConfigStore
{
  GObject           parent;

  GfMonitorManager *monitor_manager;

  GHashTable       *configs;

  GCancellable     *save_cancellable;

  GFile            *user_file;
  GFile            *custom_read_file;
  GFile            *custom_write_file;
};

enum
{
  GF_MONITOR_CONFIG_STORE_ERROR_NEEDS_MIGRATION
};

#define GF_MONITOR_CONFIG_STORE_ERROR (gf_monitor_config_store_error_quark ())
static GQuark gf_monitor_config_store_error_quark (void);

G_DEFINE_QUARK (gf-monitor-config-store-error-quark,
                gf_monitor_config_store_error)

typedef enum
{
  STATE_INITIAL,
  STATE_MONITORS,
  STATE_CONFIGURATION,
  STATE_MIGRATED,
  STATE_LOGICAL_MONITOR,
  STATE_LOGICAL_MONITOR_X,
  STATE_LOGICAL_MONITOR_Y,
  STATE_LOGICAL_MONITOR_PRIMARY,
  STATE_LOGICAL_MONITOR_PRESENTATION,
  STATE_LOGICAL_MONITOR_SCALE,
  STATE_TRANSFORM,
  STATE_TRANSFORM_ROTATION,
  STATE_TRANSFORM_FLIPPED,
  STATE_MONITOR,
  STATE_MONITOR_SPEC,
  STATE_MONITOR_SPEC_CONNECTOR,
  STATE_MONITOR_SPEC_VENDOR,
  STATE_MONITOR_SPEC_PRODUCT,
  STATE_MONITOR_SPEC_SERIAL,
  STATE_MONITOR_MODE,
  STATE_MONITOR_MODE_WIDTH,
  STATE_MONITOR_MODE_HEIGHT,
  STATE_MONITOR_MODE_RATE,
  STATE_MONITOR_MODE_FLAG,
  STATE_MONITOR_UNDERSCANNING,
  STATE_DISABLED
} ParserState;

typedef struct
{
  ParserState             state;
  GfMonitorConfigStore   *config_store;

  ParserState             monitor_spec_parent_state;

  gboolean                current_was_migrated;
  GList                  *current_logical_monitor_configs;
  GfMonitorSpec          *current_monitor_spec;
  gboolean                current_transform_flipped;
  GfMonitorTransform      current_transform;
  GfMonitorModeSpec      *current_monitor_mode_spec;
  GfMonitorConfig        *current_monitor_config;
  GfLogicalMonitorConfig *current_logical_monitor_config;
  GList                  *current_disabled_monitor_specs;

  GfMonitorsConfigFlag    extra_config_flags;
} ConfigParser;

typedef struct
{
  GfMonitorConfigStore *config_store;
  GString              *buffer;
} SaveData;

enum
{
  PROP_0,

  PROP_MONITOR_MANAGER,

  LAST_PROP
};

static GParamSpec *config_store_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfMonitorConfigStore, gf_monitor_config_store, G_TYPE_OBJECT)

static gboolean
is_system_config (GfMonitorsConfig *config)
{
  return !!(config->flags & GF_MONITORS_CONFIG_FLAG_SYSTEM_CONFIG);
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

          if (!g_str_equal (element_name, "monitors"))
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                           "Invalid document element '%s'", element_name);
              return;
            }

          if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values,
                                            error,
                                            G_MARKUP_COLLECT_STRING, "version", &version,
                                            G_MARKUP_COLLECT_INVALID))
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Missing config file format version");
            }

          if (g_str_equal (version, "1"))
            {
              g_set_error_literal (error,
                                   GF_MONITOR_CONFIG_STORE_ERROR,
                                   GF_MONITOR_CONFIG_STORE_ERROR_NEEDS_MIGRATION,
                                   "monitors.xml has the old format");
              return;
            }

          if (!g_str_equal (version, QUOTE (MONITORS_CONFIG_XML_FORMAT_VERSION)))
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid or unsupported version '%s'", version);
              return;
            }

          parser->state = STATE_MONITORS;
          return;
        }

      case STATE_MONITORS:
        {
          if (!g_str_equal (element_name, "configuration"))
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                           "Invalid toplevel element '%s'", element_name);
              return;
            }

          parser->state = STATE_CONFIGURATION;
          parser->current_was_migrated = FALSE;

          return;
        }

      case STATE_CONFIGURATION:
        {
          if (g_str_equal (element_name, "logicalmonitor"))
            {
              parser->current_logical_monitor_config = g_new0 (GfLogicalMonitorConfig, 1);

              parser->state = STATE_LOGICAL_MONITOR;
            }
          else if (g_str_equal (element_name, "migrated"))
            {
              parser->current_was_migrated = TRUE;

              parser->state = STATE_MIGRATED;
            }
          else if (g_str_equal (element_name, "disabled"))
            {
              parser->state = STATE_DISABLED;
            }
          else
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                           "Invalid configuration element '%s'", element_name);
              return;
            }

          return;
        }

      case STATE_MIGRATED:
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                       "Unexpected element '%s'", element_name);
          return;
        }

      case STATE_LOGICAL_MONITOR:
        {
          if (g_str_equal (element_name, "x"))
            {
              parser->state = STATE_LOGICAL_MONITOR_X;
            }
          else if (g_str_equal (element_name, "y"))
            {
              parser->state = STATE_LOGICAL_MONITOR_Y;
            }
          else if (g_str_equal (element_name, "scale"))
            {
              parser->state = STATE_LOGICAL_MONITOR_SCALE;
            }
          else if (g_str_equal (element_name, "primary"))
            {
              parser->state = STATE_LOGICAL_MONITOR_PRIMARY;
            }
          else if (g_str_equal (element_name, "presentation"))
            {
              parser->state = STATE_LOGICAL_MONITOR_PRESENTATION;
            }
          else if (g_str_equal (element_name, "transform"))
            {
              parser->state = STATE_TRANSFORM;
            }
          else if (g_str_equal (element_name, "monitor"))
            {
              parser->current_monitor_config = g_new0 (GfMonitorConfig, 1);;

              parser->state = STATE_MONITOR;
            }
          else
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                           "Invalid monitor logicalmonitor element '%s'", element_name);
              return;
            }

          return;
        }

      case STATE_LOGICAL_MONITOR_X:
      case STATE_LOGICAL_MONITOR_Y:
      case STATE_LOGICAL_MONITOR_SCALE:
      case STATE_LOGICAL_MONITOR_PRIMARY:
      case STATE_LOGICAL_MONITOR_PRESENTATION:
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                       "Invalid logical monitor element '%s'", element_name);
          return;
        }

      case STATE_TRANSFORM:
        {
          if (g_str_equal (element_name, "rotation"))
            {
              parser->state = STATE_TRANSFORM_ROTATION;
            }
          else if (g_str_equal (element_name, "flipped"))
            {
              parser->state = STATE_TRANSFORM_FLIPPED;
            }

          return;
        }

      case STATE_TRANSFORM_ROTATION:
      case STATE_TRANSFORM_FLIPPED:
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                       "Invalid transform element '%s'", element_name);
          return;
        }

      case STATE_MONITOR:
        {
          if (g_str_equal (element_name, "monitorspec"))
            {
              parser->current_monitor_spec = g_new0 (GfMonitorSpec, 1);

              parser->monitor_spec_parent_state = STATE_MONITOR;
              parser->state = STATE_MONITOR_SPEC;
            }
          else if (g_str_equal (element_name, "mode"))
            {
              parser->current_monitor_mode_spec = g_new0 (GfMonitorModeSpec, 1);

              parser->state = STATE_MONITOR_MODE;
            }
          else if (g_str_equal (element_name, "underscanning"))
            {
              parser->state = STATE_MONITOR_UNDERSCANNING;
            }
          else
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                           "Invalid monitor element '%s'", element_name);
              return;
            }

          return;
        }

      case STATE_MONITOR_SPEC:
        {
          if (g_str_equal (element_name, "connector"))
            {
              parser->state = STATE_MONITOR_SPEC_CONNECTOR;
            }
          else if (g_str_equal (element_name, "vendor"))
            {
              parser->state = STATE_MONITOR_SPEC_VENDOR;
            }
          else if (g_str_equal (element_name, "product"))
            {
              parser->state = STATE_MONITOR_SPEC_PRODUCT;
            }
          else if (g_str_equal (element_name, "serial"))
            {
              parser->state = STATE_MONITOR_SPEC_SERIAL;
            }
          else
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                           "Invalid monitor spec element '%s'", element_name);
              return;
            }

          return;
        }

      case STATE_MONITOR_SPEC_CONNECTOR:
      case STATE_MONITOR_SPEC_VENDOR:
      case STATE_MONITOR_SPEC_PRODUCT:
      case STATE_MONITOR_SPEC_SERIAL:
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                       "Invalid monitor spec element '%s'", element_name);
          return;
        }

      case STATE_MONITOR_MODE:
        {
          if (g_str_equal (element_name, "width"))
            {
              parser->state = STATE_MONITOR_MODE_WIDTH;
            }
          else if (g_str_equal (element_name, "height"))
            {
              parser->state = STATE_MONITOR_MODE_HEIGHT;
            }
          else if (g_str_equal (element_name, "rate"))
            {
              parser->state = STATE_MONITOR_MODE_RATE;
            }
          else if (g_str_equal (element_name, "flag"))
            {
              parser->state = STATE_MONITOR_MODE_FLAG;
            }
          else
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                           "Invalid mode element '%s'", element_name);
              return;
            }

          return;
        }

      case STATE_MONITOR_MODE_WIDTH:
      case STATE_MONITOR_MODE_HEIGHT:
      case STATE_MONITOR_MODE_RATE:
      case STATE_MONITOR_MODE_FLAG:
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                       "Invalid mode sub element '%s'", element_name);
          return;
        }

      case STATE_MONITOR_UNDERSCANNING:
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                       "Invalid element '%s' under underscanning", element_name);
          return;
        }

      case STATE_DISABLED:
        {
          if (!g_str_equal (element_name, "monitorspec"))
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                           "Invalid element '%s' under disabled", element_name);
              return;
            }

          parser->current_monitor_spec = g_new0 (GfMonitorSpec, 1);
          parser->monitor_spec_parent_state = STATE_DISABLED;
          parser->state = STATE_MONITOR_SPEC;

          return;
        }

      default:
        break;
    }
}

static gboolean
derive_logical_monitor_layout (GfLogicalMonitorConfig      *logical_monitor_config,
                               GfLogicalMonitorLayoutMode   layout_mode,
                               GError                     **error)
{
  GfMonitorConfig *monitor_config;
  gint mode_width, mode_height;
  gint width = 0, height = 0;
  GList *l;

  monitor_config = logical_monitor_config->monitor_configs->data;
  mode_width = monitor_config->mode_spec->width;
  mode_height = monitor_config->mode_spec->height;

  for (l = logical_monitor_config->monitor_configs->next; l; l = l->next)
    {
      monitor_config = l->data;

      if (monitor_config->mode_spec->width != mode_width ||
          monitor_config->mode_spec->height != mode_height)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Monitors in logical monitor incompatible");

          return FALSE;
        }
    }

  if (gf_monitor_transform_is_rotated (logical_monitor_config->transform))
    {
      width = mode_height;
      height = mode_width;
    }
  else
    {
      width = mode_width;
      height = mode_height;
    }

  switch (layout_mode)
    {
      case GF_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
        width = roundf (width / logical_monitor_config->scale);
        height = roundf (height / logical_monitor_config->scale);
        break;

      case GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
      default:
        break;
    }

  logical_monitor_config->layout.width = width;
  logical_monitor_config->layout.height = height;

  return TRUE;
}

static void
finish_monitor_spec (ConfigParser *parser)
{
  switch (parser->monitor_spec_parent_state)
    {
      case STATE_MONITOR:
        {
          parser->current_monitor_config->monitor_spec = parser->current_monitor_spec;
          parser->current_monitor_spec = NULL;
          return;
        }

      case STATE_DISABLED:
        {
          parser->current_disabled_monitor_specs =
            g_list_prepend (parser->current_disabled_monitor_specs,
                            parser->current_monitor_spec);
          parser->current_monitor_spec = NULL;

          return;
        }

      case STATE_INITIAL:
      case STATE_MONITORS:
      case STATE_CONFIGURATION:
      case STATE_MIGRATED:
      case STATE_LOGICAL_MONITOR:
      case STATE_LOGICAL_MONITOR_X:
      case STATE_LOGICAL_MONITOR_Y:
      case STATE_LOGICAL_MONITOR_PRIMARY:
      case STATE_LOGICAL_MONITOR_PRESENTATION:
      case STATE_LOGICAL_MONITOR_SCALE:
      case STATE_TRANSFORM:
      case STATE_TRANSFORM_ROTATION:
      case STATE_TRANSFORM_FLIPPED:
      case STATE_MONITOR_SPEC:
      case STATE_MONITOR_SPEC_CONNECTOR:
      case STATE_MONITOR_SPEC_VENDOR:
      case STATE_MONITOR_SPEC_PRODUCT:
      case STATE_MONITOR_SPEC_SERIAL:
      case STATE_MONITOR_MODE:
      case STATE_MONITOR_MODE_WIDTH:
      case STATE_MONITOR_MODE_HEIGHT:
      case STATE_MONITOR_MODE_RATE:
      case STATE_MONITOR_MODE_FLAG:
      case STATE_MONITOR_UNDERSCANNING:
      default:
        g_assert_not_reached ();
        break;
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
      case STATE_LOGICAL_MONITOR_X:
      case STATE_LOGICAL_MONITOR_Y:
      case STATE_LOGICAL_MONITOR_SCALE:
      case STATE_LOGICAL_MONITOR_PRIMARY:
      case STATE_LOGICAL_MONITOR_PRESENTATION:
        {
          parser->state = STATE_LOGICAL_MONITOR;
          return;
        }

      case STATE_TRANSFORM:
        {
          g_assert (g_str_equal (element_name, "transform"));

          parser->current_logical_monitor_config->transform = parser->current_transform;
          if (parser->current_transform_flipped)
            {
              parser->current_logical_monitor_config->transform +=
                GF_MONITOR_TRANSFORM_FLIPPED;
            }

          parser->current_transform = GF_MONITOR_TRANSFORM_NORMAL;
          parser->current_transform_flipped = FALSE;

          parser->state = STATE_LOGICAL_MONITOR;
          return;
        }

      case STATE_TRANSFORM_ROTATION:
      case STATE_TRANSFORM_FLIPPED:
        {
          parser->state = STATE_TRANSFORM;
          return;
        }

      case STATE_MONITOR_SPEC_CONNECTOR:
      case STATE_MONITOR_SPEC_VENDOR:
      case STATE_MONITOR_SPEC_PRODUCT:
      case STATE_MONITOR_SPEC_SERIAL:
        {
          parser->state = STATE_MONITOR_SPEC;
          return;
        }

      case STATE_MONITOR_SPEC:
        {
          g_assert (g_str_equal (element_name, "monitorspec"));

          if (!gf_verify_monitor_spec (parser->current_monitor_spec, error))
            return;

          finish_monitor_spec (parser);

          parser->state = parser->monitor_spec_parent_state;
          return;
        }

      case STATE_MONITOR_MODE_WIDTH:
      case STATE_MONITOR_MODE_HEIGHT:
      case STATE_MONITOR_MODE_RATE:
      case STATE_MONITOR_MODE_FLAG:
        {
          parser->state = STATE_MONITOR_MODE;
          return;
        }

      case STATE_MONITOR_MODE:
        {
          g_assert (g_str_equal (element_name, "mode"));

          if (!gf_verify_monitor_mode_spec (parser->current_monitor_mode_spec, error))
            return;

          parser->current_monitor_config->mode_spec = parser->current_monitor_mode_spec;
          parser->current_monitor_mode_spec = NULL;

          parser->state = STATE_MONITOR;
          return;
        }

      case STATE_MONITOR_UNDERSCANNING:
        {
          g_assert (g_str_equal (element_name, "underscanning"));

          parser->state = STATE_MONITOR;
          return;
        }

      case STATE_MONITOR:
        {
          GfLogicalMonitorConfig *logical_monitor_config;

          g_assert (g_str_equal (element_name, "monitor"));

          if (!gf_verify_monitor_config (parser->current_monitor_config, error))
            return;

          logical_monitor_config = parser->current_logical_monitor_config;

          logical_monitor_config->monitor_configs =
            g_list_append (logical_monitor_config->monitor_configs,
                           parser->current_monitor_config);
          parser->current_monitor_config = NULL;

          parser->state = STATE_LOGICAL_MONITOR;
          return;
        }

      case STATE_LOGICAL_MONITOR:
        {
          GfLogicalMonitorConfig *logical_monitor_config =
            parser->current_logical_monitor_config;

          g_assert (g_str_equal (element_name, "logicalmonitor"));

          if (parser->current_was_migrated)
            logical_monitor_config->scale = -1;
          else if (logical_monitor_config->scale == 0)
            logical_monitor_config->scale = 1;

          parser->current_logical_monitor_configs =
            g_list_append (parser->current_logical_monitor_configs,
                           logical_monitor_config);
          parser->current_logical_monitor_config = NULL;

          parser->state = STATE_CONFIGURATION;
          return;
        }

      case STATE_MIGRATED:
        {
          g_assert (g_str_equal (element_name, "migrated"));

          parser->state = STATE_CONFIGURATION;
          return;
        }

      case STATE_DISABLED:
        {
          g_assert (g_str_equal (element_name, "disabled"));

          parser->state = STATE_CONFIGURATION;
          return;
        }

      case STATE_CONFIGURATION:
        {
          GfMonitorConfigStore *store = parser->config_store;
          GfMonitorsConfig *config;
          GList *l;
          GfLogicalMonitorLayoutMode layout_mode;
          GfMonitorsConfigFlag config_flags = GF_MONITORS_CONFIG_FLAG_NONE;

          g_assert (g_str_equal (element_name, "configuration"));

          if (parser->current_was_migrated)
            layout_mode = GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;
          else
            layout_mode = gf_monitor_manager_get_default_layout_mode (store->monitor_manager);

          for (l = parser->current_logical_monitor_configs; l; l = l->next)
            {
              GfLogicalMonitorConfig *logical_monitor_config = l->data;

              if (!derive_logical_monitor_layout (logical_monitor_config,
                                                  layout_mode,
                                                  error))
                return;

              if (!gf_verify_logical_monitor_config (logical_monitor_config,
                                                     layout_mode,
                                                     store->monitor_manager,
                                                     error))
                return;
            }

          if (parser->current_was_migrated)
            config_flags |= GF_MONITORS_CONFIG_FLAG_MIGRATED;

          config_flags |= parser->extra_config_flags;

          config = gf_monitors_config_new_full (parser->current_logical_monitor_configs,
                                                parser->current_disabled_monitor_specs,
                                                layout_mode, config_flags);

          parser->current_logical_monitor_configs = NULL;
          parser->current_disabled_monitor_specs = NULL;

          if (!gf_verify_monitors_config (config, store->monitor_manager, error))
            {
              g_object_unref (config);
              return;
            }

          g_hash_table_replace (parser->config_store->configs,
                                config->key, config);

          parser->state = STATE_MONITORS;
          return;
        }

      case STATE_MONITORS:
        {
          g_assert (g_str_equal (element_name, "monitors"));

          parser->state = STATE_INITIAL;
          return;
        }

      case STATE_INITIAL:
      default:
        {
          g_assert_not_reached ();
        }
    }
}

static gboolean
read_int (const gchar  *text,
          gsize         text_len,
          gint         *out_value,
          GError      **error)
{
  gchar buf[64];
  int64_t value;
  gchar *end;

  strncpy (buf, text, text_len);
  buf[MIN (63, text_len)] = 0;

  value = g_ascii_strtoll (buf, &end, 10);

  if (*end || value < 0 || value > G_MAXINT16)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Expected a number, got %s", buf);
      return FALSE;
    }
  else
    {
      *out_value = value;
      return TRUE;
    }
}

static gboolean
read_float (const gchar  *text,
            gsize         text_len,
            gfloat       *out_value,
            GError      **error)
{
  gchar buf[64];
  gfloat value;
  gchar *end;

  strncpy (buf, text, text_len);
  buf[MIN (63, text_len)] = 0;

  value = g_ascii_strtod (buf, &end);

  if (*end)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Expected a number, got %s", buf);
      return FALSE;
    }
  else
    {
      *out_value = value;
      return TRUE;
    }
}

static gboolean
read_bool (const gchar  *text,
           gsize         text_len,
           gboolean     *out_value,
           GError      **error)
{
  if (strncmp (text, "no", text_len) == 0)
    {
      *out_value = FALSE;
      return TRUE;
    }
  else if (strncmp (text, "yes", text_len) == 0)
    {
      *out_value = TRUE;
      return TRUE;
    }
  else
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Invalid boolean value '%.*s'", (int) text_len, text);
      return FALSE;
    }
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
      case STATE_INITIAL:
      case STATE_MONITORS:
      case STATE_CONFIGURATION:
      case STATE_MIGRATED:
      case STATE_LOGICAL_MONITOR:
      case STATE_MONITOR:
      case STATE_MONITOR_SPEC:
      case STATE_MONITOR_MODE:
      case STATE_TRANSFORM:
      case STATE_DISABLED:
        {
          if (!is_all_whitespace (text, text_len))
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Unexpected content at this point");
          return;
        }

      case STATE_MONITOR_SPEC_CONNECTOR:
        {
          parser->current_monitor_spec->connector = g_strndup (text, text_len);
          return;
        }

      case STATE_MONITOR_SPEC_VENDOR:
        {
          parser->current_monitor_spec->vendor = g_strndup (text, text_len);
          return;
        }

      case STATE_MONITOR_SPEC_PRODUCT:
        {
          parser->current_monitor_spec->product = g_strndup (text, text_len);
          return;
        }

      case STATE_MONITOR_SPEC_SERIAL:
        {
          parser->current_monitor_spec->serial = g_strndup (text, text_len);
          return;
        }

      case STATE_LOGICAL_MONITOR_X:
        {
          read_int (text, text_len,
                    &parser->current_logical_monitor_config->layout.x, error);
          return;
        }

      case STATE_LOGICAL_MONITOR_Y:
        {
          read_int (text, text_len,
                    &parser->current_logical_monitor_config->layout.y, error);
          return;
        }

      case STATE_LOGICAL_MONITOR_SCALE:
        {
          if (!read_float (text, text_len,
                           &parser->current_logical_monitor_config->scale, error))
            return;

          if (parser->current_logical_monitor_config->scale <= 0.0f)
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Logical monitor scale '%g' invalid",
                           (gdouble) parser->current_logical_monitor_config->scale);
              return;
            }

          return;
        }

      case STATE_LOGICAL_MONITOR_PRIMARY:
        {
          read_bool (text, text_len,
                     &parser->current_logical_monitor_config->is_primary,
                     error);
          return;
        }

      case STATE_LOGICAL_MONITOR_PRESENTATION:
        {
          read_bool (text, text_len,
                     &parser->current_logical_monitor_config->is_presentation,
                     error);
          return;
        }

      case STATE_TRANSFORM_ROTATION:
        {
          if (strncmp (text, "normal", text_len) == 0)
            parser->current_transform = GF_MONITOR_TRANSFORM_NORMAL;
          else if (strncmp (text, "left", text_len) == 0)
            parser->current_transform = GF_MONITOR_TRANSFORM_90;
          else if (strncmp (text, "upside_down", text_len) == 0)
            parser->current_transform = GF_MONITOR_TRANSFORM_180;
          else if (strncmp (text, "right", text_len) == 0)
            parser->current_transform = GF_MONITOR_TRANSFORM_270;
          else
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Invalid rotation type %.*s", (int)text_len, text);

          return;
        }

      case STATE_TRANSFORM_FLIPPED:
        {
          read_bool (text, text_len,
                     &parser->current_transform_flipped,
                     error);
          return;
        }

      case STATE_MONITOR_MODE_WIDTH:
        {
          read_int (text, text_len,
                    &parser->current_monitor_mode_spec->width,
                    error);
          return;
        }

      case STATE_MONITOR_MODE_HEIGHT:
        {
          read_int (text, text_len,
                    &parser->current_monitor_mode_spec->height,
                    error);
          return;
        }

      case STATE_MONITOR_MODE_RATE:
        {
          read_float (text, text_len,
                      &parser->current_monitor_mode_spec->refresh_rate,
                      error);
          return;
        }

      case STATE_MONITOR_MODE_FLAG:
        {
          if (strncmp (text, "interlace", text_len) == 0)
            {
              parser->current_monitor_mode_spec->flags |= GF_CRTC_MODE_FLAG_INTERLACE;
            }
          else
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid mode flag %.*s", (int) text_len, text);
            }

          return;
        }

      case STATE_MONITOR_UNDERSCANNING:
        {
          read_bool (text, text_len,
                     &parser->current_monitor_config->enable_underscanning,
                     error);
          return;
        }

      default:
        break;
    }
}

static const GMarkupParser config_parser =
  {
    .start_element = handle_start_element,
    .end_element = handle_end_element,
    .text = handle_text
  };

static gboolean
read_config_file (GfMonitorConfigStore  *config_store,
                  GFile                 *file,
                  GfMonitorsConfigFlag   extra_config_flags,
                  GError               **error)
{
  gchar *buffer;
  gsize size;
  ConfigParser parser;
  GMarkupParseContext *parse_context;

  if (!g_file_load_contents (file, NULL, &buffer, &size, NULL, error))
    return FALSE;

  parser = (ConfigParser) {
    .state = STATE_INITIAL,
    .config_store = config_store,
    .extra_config_flags = extra_config_flags
  };

  parse_context = g_markup_parse_context_new (&config_parser,
                                              G_MARKUP_TREAT_CDATA_AS_TEXT |
                                              G_MARKUP_PREFIX_ERROR_POSITION,
                                              &parser, NULL);

  if (!g_markup_parse_context_parse (parse_context, buffer, size, error))
    {
      g_list_free_full (parser.current_logical_monitor_configs,
                        (GDestroyNotify) gf_logical_monitor_config_free);

      g_clear_pointer (&parser.current_monitor_spec, gf_monitor_spec_free);
      g_free (parser.current_monitor_mode_spec);
      g_clear_pointer (&parser.current_monitor_config, gf_monitor_config_free);

      g_clear_pointer (&parser.current_logical_monitor_config,
                       gf_logical_monitor_config_free);

      g_markup_parse_context_free (parse_context);
      g_free (buffer);

      return FALSE;
    }

  g_markup_parse_context_free (parse_context);
  g_free (buffer);

  return TRUE;
}

static const gchar *
bool_to_string (gboolean value)
{
  return value ? "yes" : "no";
}

static void
append_transform (GString            *buffer,
                  GfMonitorTransform  transform)
{
  const gchar *rotation;
  gboolean flipped;

  rotation = NULL;
  flipped = FALSE;

  switch (transform)
    {
      case GF_MONITOR_TRANSFORM_90:
        rotation = "left";
        break;

      case GF_MONITOR_TRANSFORM_180:
        rotation = "upside_down";
        break;

      case GF_MONITOR_TRANSFORM_270:
        rotation = "right";
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED:
        rotation = "normal";
        flipped = TRUE;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED_90:
        rotation = "left";
        flipped = TRUE;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED_180:
        rotation = "upside_down";
        flipped = TRUE;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED_270:
        rotation = "right";
        flipped = TRUE;
        break;

      case GF_MONITOR_TRANSFORM_NORMAL:
      default:
        return;
    }

  g_string_append (buffer, "      <transform>\n");
  g_string_append_printf (buffer, "        <rotation>%s</rotation>\n", rotation);
  g_string_append_printf (buffer, "        <flipped>%s</flipped>\n", bool_to_string (flipped));
  g_string_append (buffer, "      </transform>\n");
}

static void
append_monitor_spec (GString       *buffer,
                     GfMonitorSpec *monitor_spec,
                     const gchar   *indentation)
{
  g_string_append_printf (buffer, "%s<monitorspec>\n", indentation);
  g_string_append_printf (buffer, "%s  <connector>%s</connector>\n",
                          indentation,
                          monitor_spec->connector);
  g_string_append_printf (buffer, "%s  <vendor>%s</vendor>\n",
                          indentation,
                          monitor_spec->vendor);
  g_string_append_printf (buffer, "%s  <product>%s</product>\n",
                          indentation,
                          monitor_spec->product);
  g_string_append_printf (buffer, "%s  <serial>%s</serial>\n",
                          indentation,
                          monitor_spec->serial);
  g_string_append_printf (buffer, "%s</monitorspec>\n", indentation);
}

static void
append_monitors (GString *buffer,
                 GList   *monitor_configs)
{
  GList *l;

  for (l = monitor_configs; l; l = l->next)
    {
      GfMonitorConfig *monitor_config;
      GfMonitorModeSpec *mode_spec;
      gchar rate_str[G_ASCII_DTOSTR_BUF_SIZE];

      monitor_config = l->data;
      mode_spec = monitor_config->mode_spec;

      g_ascii_dtostr (rate_str, sizeof (rate_str), mode_spec->refresh_rate);

      g_string_append (buffer, "      <monitor>\n");
      append_monitor_spec (buffer, monitor_config->monitor_spec, "        ");
      g_string_append (buffer, "        <mode>\n");
      g_string_append_printf (buffer, "          <width>%d</width>\n", mode_spec->width);
      g_string_append_printf (buffer, "          <height>%d</height>\n", mode_spec->height);
      g_string_append_printf (buffer, "          <rate>%s</rate>\n", rate_str);
      if (monitor_config->mode_spec->flags & GF_CRTC_MODE_FLAG_INTERLACE)
        g_string_append_printf (buffer, "          <flag>interlace</flag>\n");
      g_string_append (buffer, "        </mode>\n");
      if (monitor_config->enable_underscanning)
        g_string_append (buffer, "        <underscanning>yes</underscanning>\n");
      g_string_append (buffer, "      </monitor>\n");
    }
}

static void
append_logical_monitor_xml (GString                *buffer,
                            GfMonitorsConfig       *config,
                            GfLogicalMonitorConfig *logical_monitor_config)
{
  gchar scale_str[G_ASCII_DTOSTR_BUF_SIZE];

  g_string_append (buffer, "    <logicalmonitor>\n");
  g_string_append_printf (buffer, "      <x>%d</x>\n", logical_monitor_config->layout.x);
  g_string_append_printf (buffer, "      <y>%d</y>\n", logical_monitor_config->layout.y);

  g_ascii_dtostr (scale_str, G_ASCII_DTOSTR_BUF_SIZE, logical_monitor_config->scale);
  if ((config->flags & GF_MONITORS_CONFIG_FLAG_MIGRATED) == 0)
    g_string_append_printf (buffer, "      <scale>%s</scale>\n", scale_str);

  if (logical_monitor_config->is_primary)
    g_string_append (buffer, "      <primary>yes</primary>\n");

  if (logical_monitor_config->is_presentation)
    g_string_append (buffer, "      <presentation>yes</presentation>\n");

  append_transform (buffer, logical_monitor_config->transform);
  append_monitors (buffer, logical_monitor_config->monitor_configs);
  g_string_append (buffer, "    </logicalmonitor>\n");
}

static GString *
generate_config_xml (GfMonitorConfigStore *config_store)
{
  GString *buffer;
  GHashTableIter iter;
  GfMonitorsConfig *config;

  buffer = g_string_new ("");
  g_string_append_printf (buffer, "<monitors version=\"%d\">\n",
                          MONITORS_CONFIG_XML_FORMAT_VERSION);

  g_hash_table_iter_init (&iter, config_store->configs);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &config))
    {
      GList *l;

      if (config->flags & GF_MONITORS_CONFIG_FLAG_SYSTEM_CONFIG)
        continue;

      g_string_append (buffer, "  <configuration>\n");

      if (config->flags & GF_MONITORS_CONFIG_FLAG_MIGRATED)
        g_string_append (buffer, "    <migrated/>\n");

      for (l = config->logical_monitor_configs; l; l = l->next)
        {
          GfLogicalMonitorConfig *logical_monitor_config = l->data;

          append_logical_monitor_xml (buffer, config, logical_monitor_config);
        }

      if (config->disabled_monitor_specs)
        {
          g_string_append (buffer, "    <disabled>\n");
          for (l = config->disabled_monitor_specs; l; l = l->next)
            {
              GfMonitorSpec *monitor_spec = l->data;

              append_monitor_spec (buffer, monitor_spec, "      ");
            }
          g_string_append (buffer, "    </disabled>\n");
        }

      g_string_append (buffer, "  </configuration>\n");
    }

  g_string_append (buffer, "</monitors>\n");

  return buffer;
}

static void
saved_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
  SaveData *data;
  GError *error;

  data = user_data;

  error = NULL;
  if (!g_file_replace_contents_finish (G_FILE (object), result, NULL, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Saving monitor configuration failed: %s\n", error->message);
          g_clear_object (&data->config_store->save_cancellable);
        }

      g_error_free (error);
    }
  else
    {
      g_clear_object (&data->config_store->save_cancellable);
    }

  g_clear_object (&data->config_store);
  g_string_free (data->buffer, TRUE);
  g_free (data);
}

static void
gf_monitor_config_store_save_sync (GfMonitorConfigStore *config_store)
{
  GFile *file;
  GString *buffer;
  GError *error;

  if (config_store->custom_write_file)
    file = config_store->custom_write_file;
  else
    file = config_store->user_file;

  buffer = generate_config_xml (config_store);

  error = NULL;
  if (!g_file_replace_contents (file,
                                buffer->str, buffer->len,
                                NULL,
                                FALSE,
                                G_FILE_CREATE_REPLACE_DESTINATION,
                                NULL,
                                NULL,
                                &error))
    {
      g_warning ("Saving monitor configuration failed: %s\n", error->message);
      g_error_free (error);
    }

  g_string_free (buffer, TRUE);
}

static void
gf_monitor_config_store_save (GfMonitorConfigStore *config_store)
{
  GString *buffer;
  SaveData *data;

  if (config_store->save_cancellable)
    {
      g_cancellable_cancel (config_store->save_cancellable);
      g_clear_object (&config_store->save_cancellable);
    }

  /*
   * Custom write file is only ever used by the test suite, and the test suite
   * will want to have be able to read back the content immediately, so for
   * custom write files, do the content replacement synchronously.
   */
  if (config_store->custom_write_file)
    {
      gf_monitor_config_store_save_sync (config_store);
      return;
    }

  config_store->save_cancellable = g_cancellable_new ();

  buffer = generate_config_xml (config_store);

  data = g_new0 (SaveData, 1);

  data->config_store = g_object_ref (config_store);
  data->buffer = buffer;

  g_file_replace_contents_async (config_store->user_file,
                                 buffer->str, buffer->len,
                                 NULL,
                                 TRUE,
                                 G_FILE_CREATE_REPLACE_DESTINATION,
                                 config_store->save_cancellable,
                                 saved_cb, data);
}

static void
maybe_save_configs (GfMonitorConfigStore *config_store)
{
  /*
   * If a custom file is used, it means we are run by the test suite. When this
   * is done, avoid replacing the user configuration file with test data,
   * except if a custom write file is set as well.
   */
  if (!config_store->custom_read_file || config_store->custom_write_file)
    gf_monitor_config_store_save (config_store);
}

static void
gf_monitor_config_store_constructed (GObject *object)
{
  GfMonitorConfigStore *config_store;
  const char * const *system_dirs;
  char *user_file_path;
  GError *error;

  G_OBJECT_CLASS (gf_monitor_config_store_parent_class)->constructed (object);

  config_store = GF_MONITOR_CONFIG_STORE (object);
  error = NULL;

  for (system_dirs = g_get_system_config_dirs ();
       system_dirs && *system_dirs;
       system_dirs++)
    {
      char *system_file_path;

      system_file_path = g_build_filename (*system_dirs, "monitors.xml", NULL);

      if (g_file_test (system_file_path, G_FILE_TEST_EXISTS))
        {
          GFile *system_file;

          system_file = g_file_new_for_path (system_file_path);

          if (!read_config_file (config_store,
                                 system_file,
                                 GF_MONITORS_CONFIG_FLAG_SYSTEM_CONFIG,
                                 &error))
            {
              if (g_error_matches (error,
                                   GF_MONITOR_CONFIG_STORE_ERROR,
                                   GF_MONITOR_CONFIG_STORE_ERROR_NEEDS_MIGRATION))
                g_warning ("System monitor configuration file (%s) is "
                           "incompatible; ask your administrator to migrate "
                           "the system monitor configuration.",
                           system_file_path);
              else
                g_warning ("Failed to read monitors config file '%s': %s",
                           system_file_path, error->message);

              g_clear_error (&error);
            }

          g_object_unref (system_file);
        }

      g_free (system_file_path);
    }

  user_file_path = g_build_filename (g_get_user_config_dir (), "monitors.xml", NULL);
  config_store->user_file = g_file_new_for_path (user_file_path);

  if (g_file_test (user_file_path, G_FILE_TEST_EXISTS))
    {
      if (!read_config_file (config_store,
                             config_store->user_file,
                             GF_MONITORS_CONFIG_FLAG_NONE,
                             &error))
        {
          if (error->domain == GF_MONITOR_CONFIG_STORE_ERROR &&
              error->code == GF_MONITOR_CONFIG_STORE_ERROR_NEEDS_MIGRATION)
            {
              g_clear_error (&error);
              if (!gf_migrate_old_user_monitors_config (config_store, &error))
                {
                  g_warning ("Failed to migrate old monitors config file: %s",
                             error->message);
                  g_error_free (error);
                }
            }
          else
            {
              g_warning ("Failed to read monitors config file '%s': %s",
                         user_file_path, error->message);
              g_error_free (error);
            }
        }
    }

  g_free (user_file_path);
}

static void
gf_monitor_config_store_dispose (GObject *object)
{
  GfMonitorConfigStore *config_store;

  config_store = GF_MONITOR_CONFIG_STORE (object);

  if (config_store->save_cancellable)
    {
      g_cancellable_cancel (config_store->save_cancellable);
      g_clear_object (&config_store->save_cancellable);

      gf_monitor_config_store_save_sync (config_store);
    }

  config_store->monitor_manager = NULL;

  g_clear_pointer (&config_store->configs, g_hash_table_destroy);

  g_clear_object (&config_store->user_file);
  g_clear_object (&config_store->custom_read_file);
  g_clear_object (&config_store->custom_write_file);

  G_OBJECT_CLASS (gf_monitor_config_store_parent_class)->dispose (object);
}

static void
gf_monitor_config_store_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GfMonitorConfigStore *config_store;

  config_store = GF_MONITOR_CONFIG_STORE (object);

  switch (property_id)
    {
      case PROP_MONITOR_MANAGER:
        g_value_set_object (value, &config_store->monitor_manager);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_monitor_config_store_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GfMonitorConfigStore *config_store;

  config_store = GF_MONITOR_CONFIG_STORE (object);

  switch (property_id)
    {
      case PROP_MONITOR_MANAGER:
        config_store->monitor_manager = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_monitor_config_store_class_init (GfMonitorConfigStoreClass *config_store_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (config_store_class);

  object_class->constructed = gf_monitor_config_store_constructed;
  object_class->dispose = gf_monitor_config_store_dispose;
  object_class->get_property = gf_monitor_config_store_get_property;
  object_class->set_property = gf_monitor_config_store_set_property;

  config_store_properties[PROP_MONITOR_MANAGER] =
    g_param_spec_object ("monitor-manager",
                         "GfMonitorManager",
                         "GfMonitorManager",
                         GF_TYPE_MONITOR_MANAGER,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     config_store_properties);
}

static void
gf_monitor_config_store_init (GfMonitorConfigStore *config_store)
{
  config_store->configs = g_hash_table_new_full (gf_monitors_config_key_hash,
                                                 gf_monitors_config_key_equal,
                                                 NULL, g_object_unref);
}

GfMonitorConfigStore *
gf_monitor_config_store_new (GfMonitorManager *monitor_manager)
{
  return g_object_new (GF_TYPE_MONITOR_CONFIG_STORE,
                       "monitor-manager", monitor_manager,
                       NULL);
}

GfMonitorsConfig *
gf_monitor_config_store_lookup (GfMonitorConfigStore *config_store,
                                GfMonitorsConfigKey  *key)
{
  return GF_MONITORS_CONFIG (g_hash_table_lookup (config_store->configs, key));
}

void
gf_monitor_config_store_add (GfMonitorConfigStore *config_store,
                             GfMonitorsConfig     *config)
{
  g_hash_table_replace (config_store->configs, config->key, g_object_ref (config));

  if (!is_system_config (config))
    maybe_save_configs (config_store);
}

void
gf_monitor_config_store_remove (GfMonitorConfigStore *config_store,
                                GfMonitorsConfig     *config)
{
  g_hash_table_remove (config_store->configs, config->key);

  if (!is_system_config (config))
    maybe_save_configs (config_store);
}

gboolean
gf_monitor_config_store_set_custom (GfMonitorConfigStore  *config_store,
                                    const gchar           *read_path,
                                    const gchar           *write_path,
                                    GError               **error)
{
  g_clear_object (&config_store->custom_read_file);
  g_clear_object (&config_store->custom_write_file);
  g_hash_table_remove_all (config_store->configs);

  config_store->custom_read_file = g_file_new_for_path (read_path);
  if (write_path)
    config_store->custom_write_file = g_file_new_for_path (write_path);

  return read_config_file (config_store,
                           config_store->custom_read_file,
                           GF_MONITORS_CONFIG_FLAG_NONE,
                           error);
}

gint
gf_monitor_config_store_get_config_count (GfMonitorConfigStore *config_store)
{
  return (gint) g_hash_table_size (config_store->configs);
}

GfMonitorManager *
gf_monitor_config_store_get_monitor_manager (GfMonitorConfigStore *config_store)
{
  return config_store->monitor_manager;
}
