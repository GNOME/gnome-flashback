/*
 * Copyright (C) 2016 Red Hat
 * Copyright (C) 2017-2019 Alberts Muktupāvels
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
 * - src/backends/meta-monitor-config-manager.c
 */

#include "config.h"

#include <gio/gio.h>

#include "gf-logical-monitor-config-private.h"
#include "gf-monitor-config-private.h"
#include "gf-monitor-config-utils.h"
#include "gf-monitor-spec-private.h"
#include "gf-monitors-config-private.h"
#include "gf-rectangle-private.h"

G_DEFINE_TYPE (GfMonitorsConfig, gf_monitors_config, G_TYPE_OBJECT)

static gboolean
gf_monitors_config_is_monitor_enabled (GfMonitorsConfig *config,
                                       GfMonitorSpec    *monitor_spec)
{
  return gf_logical_monitor_configs_have_monitor (config->logical_monitor_configs,
                                                  monitor_spec);
}

static GfMonitorsConfigKey *
gf_monitors_config_key_new (GList                      *logical_monitor_configs,
                            GList                      *disabled_monitor_specs,
                            GfLogicalMonitorLayoutMode  layout_mode)
{
  GfMonitorsConfigKey *config_key;
  GList *monitor_specs;
  GList *l;

  monitor_specs = NULL;
  for (l = logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config = l->data;
      GList *k;

      for (k = logical_monitor_config->monitor_configs; k; k = k->next)
        {
          GfMonitorConfig *monitor_config = k->data;
          GfMonitorSpec *monitor_spec;

          monitor_spec = gf_monitor_spec_clone (monitor_config->monitor_spec);
          monitor_specs = g_list_prepend (monitor_specs, monitor_spec);
        }
    }

  for (l = disabled_monitor_specs; l; l = l->next)
    {
      GfMonitorSpec *monitor_spec = l->data;

      monitor_spec = gf_monitor_spec_clone (monitor_spec);
      monitor_specs = g_list_prepend (monitor_specs, monitor_spec);
    }

  monitor_specs = g_list_sort (monitor_specs, (GCompareFunc) gf_monitor_spec_compare);

  config_key = g_new0 (GfMonitorsConfigKey, 1);
  config_key->monitor_specs = monitor_specs;
  config_key->layout_mode = layout_mode;

  return config_key;
}

static void
gf_monitors_config_finalize (GObject *object)
{
  GfMonitorsConfig *config;

  config = GF_MONITORS_CONFIG (object);

  g_clear_object (&config->parent_config);

  gf_monitors_config_key_free (config->key);
  g_list_free_full (config->logical_monitor_configs,
                    (GDestroyNotify) gf_logical_monitor_config_free);
  g_list_free_full (config->disabled_monitor_specs,
                    (GDestroyNotify) gf_monitor_spec_free);

  G_OBJECT_CLASS (gf_monitors_config_parent_class)->finalize (object);
}

static void
gf_monitors_config_class_init (GfMonitorsConfigClass *config_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (config_class);

  object_class->finalize = gf_monitors_config_finalize;
}

static void
gf_monitors_config_init (GfMonitorsConfig *config)
{
}

GfMonitorsConfig *
gf_monitors_config_new_full (GList                      *logical_monitor_configs,
                             GList                      *disabled_monitor_specs,
                             GfLogicalMonitorLayoutMode  layout_mode,
                             GfMonitorsConfigFlag        flags)
{
  GfMonitorsConfig *config;

  config = g_object_new (GF_TYPE_MONITORS_CONFIG, NULL);
  config->logical_monitor_configs = logical_monitor_configs;
  config->disabled_monitor_specs = disabled_monitor_specs;
  config->layout_mode = layout_mode;
  config->key = gf_monitors_config_key_new (logical_monitor_configs,
                                            disabled_monitor_specs,
                                            layout_mode);
  config->flags = flags;
  config->switch_config = GF_MONITOR_SWITCH_CONFIG_UNKNOWN;

  return config;
}

GfMonitorsConfig *
gf_monitors_config_new (GfMonitorManager           *monitor_manager,
                        GList                      *logical_monitor_configs,
                        GfLogicalMonitorLayoutMode  layout_mode,
                        GfMonitorsConfigFlag        flags)
{
  GList *disabled_monitor_specs = NULL;
  GList *monitors;
  GList *l;

  monitors = gf_monitor_manager_get_monitors (monitor_manager);
  for (l = monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;
      GfMonitorSpec *monitor_spec;

      if (!gf_monitor_manager_is_monitor_visible (monitor_manager, monitor))
        continue;

      monitor_spec = gf_monitor_get_spec (monitor);
      if (gf_logical_monitor_configs_have_monitor (logical_monitor_configs,
                                                   monitor_spec))
        continue;

      disabled_monitor_specs =
        g_list_prepend (disabled_monitor_specs,
                        gf_monitor_spec_clone (monitor_spec));
    }

  return gf_monitors_config_new_full (logical_monitor_configs,
                                      disabled_monitor_specs,
                                      layout_mode, flags);
}

void
gf_monitors_config_set_parent_config (GfMonitorsConfig *config,
                                      GfMonitorsConfig *parent_config)
{
  g_assert (config != parent_config);
  g_assert (parent_config == NULL || parent_config->parent_config != config);

  g_set_object (&config->parent_config, parent_config);
}

GfMonitorSwitchConfigType
gf_monitors_config_get_switch_config (GfMonitorsConfig *config)
{
  return config->switch_config;
}

void
gf_monitors_config_set_switch_config (GfMonitorsConfig          *config,
                                      GfMonitorSwitchConfigType  switch_config)
{
  config->switch_config = switch_config;
}

guint
gf_monitors_config_key_hash (gconstpointer data)
{
  const GfMonitorsConfigKey *config_key;
  glong hash;
  GList *l;

  config_key = data;
  hash = config_key->layout_mode;

  for (l = config_key->monitor_specs; l; l = l->next)
    {
      GfMonitorSpec *monitor_spec = l->data;

      hash ^= (g_str_hash (monitor_spec->connector) ^
               g_str_hash (monitor_spec->vendor) ^
               g_str_hash (monitor_spec->product) ^
               g_str_hash (monitor_spec->serial));
    }

  return hash;
}

gboolean
gf_monitors_config_key_equal (gconstpointer data_a,
                              gconstpointer data_b)
{
  const GfMonitorsConfigKey *config_key_a;
  const GfMonitorsConfigKey *config_key_b;
  GList *l_a, *l_b;

  config_key_a = data_a;
  config_key_b = data_b;

  if (config_key_a->layout_mode != config_key_b->layout_mode)
    return FALSE;

  for (l_a = config_key_a->monitor_specs, l_b = config_key_b->monitor_specs;
       l_a && l_b;
       l_a = l_a->next, l_b = l_b->next)
    {
      GfMonitorSpec *monitor_spec_a = l_a->data;
      GfMonitorSpec *monitor_spec_b = l_b->data;

      if (!gf_monitor_spec_equals (monitor_spec_a, monitor_spec_b))
        return FALSE;
    }

  if (l_a || l_b)
    return FALSE;

  return TRUE;
}

void
gf_monitors_config_key_free (GfMonitorsConfigKey *config_key)
{
  g_list_free_full (config_key->monitor_specs, (GDestroyNotify) gf_monitor_spec_free);
  g_free (config_key);
}

gboolean
gf_verify_monitors_config (GfMonitorsConfig  *config,
                           GfMonitorManager  *monitor_manager,
                           GError           **error)
{
  GList *l;

  if (!gf_verify_logical_monitor_config_list (config->logical_monitor_configs,
                                              config->layout_mode,
                                              monitor_manager,
                                              error))
    return FALSE;

  for (l = config->disabled_monitor_specs; l; l = l->next)
    {
      GfMonitorSpec *monitor_spec = l->data;

      if (gf_monitors_config_is_monitor_enabled (config, monitor_spec))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Assigned monitor explicitly disabled");
          return FALSE;
        }
    }

  return TRUE;
}
