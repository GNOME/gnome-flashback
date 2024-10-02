/*
 * Copyright (C) 2016 Red Hat
 * Copyright (c) 2018 DisplayLink (UK) Ltd.
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
#include "gf-monitor-config-utils.h"

#include <gio/gio.h>

#include "gf-monitor-config-store-private.h"
#include "gf-monitor-config-manager-private.h"
#include "gf-monitor-spec-private.h"
#include "gf-rectangle-private.h"

static GList *
clone_monitor_config_list (GList *configs_in)
{
  GList *configs_out;
  GList *l;

  configs_out = NULL;

  for (l = configs_in; l != NULL; l = l->next)
    {
      GfMonitorConfig *config_in;
      GfMonitorConfig *config_out;

      config_in = l->data;

      config_out = g_new0 (GfMonitorConfig, 1);
      *config_out = (GfMonitorConfig) {
        .monitor_spec = gf_monitor_spec_clone (config_in->monitor_spec),
        .mode_spec = g_memdup2 (config_in->mode_spec, sizeof (GfMonitorModeSpec)),
        .enable_underscanning = config_in->enable_underscanning,
        .has_max_bpc = config_in->has_max_bpc,
        .max_bpc = config_in->max_bpc
      };

      configs_out = g_list_append (configs_out, config_out);
    }

  return configs_out;
}

static GList *
find_adjacent_neighbours (GList                  *logical_monitor_configs,
                          GfLogicalMonitorConfig *logical_monitor_config)
{
  GList *adjacent_neighbors;
  GList *l;

  adjacent_neighbors = NULL;

  if (!logical_monitor_configs->next)
    {
      g_assert (logical_monitor_configs->data == logical_monitor_config);
      return NULL;
    }

  for (l = logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *other_logical_monitor_config = l->data;

      if (logical_monitor_config == other_logical_monitor_config)
        continue;

      if (gf_rectangle_is_adjacent_to (&logical_monitor_config->layout,
                                       &other_logical_monitor_config->layout))
        {
          adjacent_neighbors = g_list_prepend (adjacent_neighbors,
                                               other_logical_monitor_config);
        }
    }

  return adjacent_neighbors;
}

static void
traverse_new_neighbours (GList                  *logical_monitor_configs,
                         GfLogicalMonitorConfig *logical_monitor_config,
                         GHashTable             *neighbourhood)
{
  GList *adjacent_neighbours;
  GList *l;

  g_hash_table_add (neighbourhood, logical_monitor_config);

  adjacent_neighbours = find_adjacent_neighbours (logical_monitor_configs,
                                                  logical_monitor_config);

  for (l = adjacent_neighbours; l; l = l->next)
    {
      GfLogicalMonitorConfig *neighbour;

      neighbour = l->data;

      if (g_hash_table_contains (neighbourhood, neighbour))
        continue;

      traverse_new_neighbours (logical_monitor_configs, neighbour, neighbourhood);
    }

  g_list_free (adjacent_neighbours);
}

static gboolean
is_connected_to_all (GfLogicalMonitorConfig *logical_monitor_config,
                     GList                  *logical_monitor_configs)
{
  GHashTable *neighbourhood;
  gboolean is_connected_to_all;

  neighbourhood = g_hash_table_new (NULL, NULL);

  traverse_new_neighbours (logical_monitor_configs,
                           logical_monitor_config,
                           neighbourhood);

  is_connected_to_all = g_hash_table_size (neighbourhood) == g_list_length (logical_monitor_configs);

  g_hash_table_destroy (neighbourhood);

  return is_connected_to_all;
}

GList *
gf_clone_logical_monitor_config_list (GList *logical_monitor_configs_in)
{
  GList *configs_out;
  GList *l;

  configs_out = NULL;

  for (l = logical_monitor_configs_in; l != NULL; l = l->next)
    {
      GfLogicalMonitorConfig *config_in;
      GfLogicalMonitorConfig *config_out;
      GList *config_list;

      config_in = l->data;

      config_out = g_memdup2 (config_in, sizeof (GfLogicalMonitorConfig));

      config_list = clone_monitor_config_list (config_in->monitor_configs);
      config_out->monitor_configs = config_list;

      configs_out = g_list_append (configs_out, config_out);
    }

  return configs_out;
}

gboolean
gf_verify_logical_monitor_config_list (GList                       *logical_monitor_configs,
                                       GfLogicalMonitorLayoutMode   layout_mode,
                                       GfMonitorManager            *monitor_manager,
                                       GError                     **error)
{
  gint min_x, min_y;
  gboolean has_primary;
  GList *region;
  GList *l;
  gboolean global_scale_required;

  if (!logical_monitor_configs)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Monitors config incomplete");

      return FALSE;
    }

  global_scale_required = !!(gf_monitor_manager_get_capabilities (monitor_manager) &
                             GF_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED);

  min_x = INT_MAX;
  min_y = INT_MAX;
  region = NULL;
  has_primary = FALSE;

  for (l = logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config = l->data;

      if (!gf_verify_logical_monitor_config (logical_monitor_config,
                                             layout_mode,
                                             monitor_manager,
                                             error))
        return FALSE;

      if (global_scale_required)
        {
          GfLogicalMonitorConfig *prev_logical_monitor_config;

          prev_logical_monitor_config = l->prev ? l->prev->data : NULL;

          if (prev_logical_monitor_config &&
              (prev_logical_monitor_config->scale !=
               logical_monitor_config->scale))
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Logical monitor scales must be identical");

              return FALSE;
            }
        }

      if (gf_rectangle_overlaps_with_region (region, &logical_monitor_config->layout))
        {
          g_list_free (region);
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Logical monitors overlap");

          return FALSE;
        }

      if (has_primary && logical_monitor_config->is_primary)
        {
          g_list_free (region);
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Config contains multiple primary logical monitors");

          return FALSE;
        }
      else if (logical_monitor_config->is_primary)
        {
          has_primary = TRUE;
        }

      if (!is_connected_to_all (logical_monitor_config, logical_monitor_configs))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Logical monitors not adjacent");

          return FALSE;
        }

      min_x = MIN (logical_monitor_config->layout.x, min_x);
      min_y = MIN (logical_monitor_config->layout.y, min_y);

      region = g_list_prepend (region, &logical_monitor_config->layout);
    }

  g_list_free (region);

  if (min_x != 0 || min_y != 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Logical monitors positions are offset");

      return FALSE;
    }

  if (!has_primary)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Config is missing primary logical");

      return FALSE;
    }

  return TRUE;
}
