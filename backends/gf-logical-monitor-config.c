/*
 * Copyright (C) 2016 Red Hat
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
 * - src/backends/meta-monitor-config-manager.c
 */

#include "config.h"

#include <math.h>

#include "gf-logical-monitor-config-private.h"
#include "gf-monitor-config-private.h"
#include "gf-monitor-spec-private.h"

void
gf_logical_monitor_config_free (GfLogicalMonitorConfig *config)
{
  GList *monitor_configs;

  monitor_configs = config->monitor_configs;

  g_list_free_full (monitor_configs, (GDestroyNotify) gf_monitor_config_free);
  g_free (config);
}

gboolean
gf_logical_monitor_configs_have_monitor (GList         *logical_monitor_configs,
                                         GfMonitorSpec *monitor_spec)
{
  GList *l;

  for (l = logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config = l->data;
      GList *k;

      for (k = logical_monitor_config->monitor_configs; k; k = k->next)
        {
          GfMonitorConfig *monitor_config = k->data;

          if (gf_monitor_spec_equals (monitor_spec, monitor_config->monitor_spec))
            return TRUE;
        }
    }

  return FALSE;
}

gboolean
gf_verify_logical_monitor_config (GfLogicalMonitorConfig      *config,
                                  GfLogicalMonitorLayoutMode   layout_mode,
                                  GfMonitorManager            *monitor_manager,
                                  GError                     **error)
{
  GList *l;
  gint expected_mode_width = 0;
  gint expected_mode_height = 0;

  if (config->layout.x < 0 || config->layout.y < 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid logical monitor position (%d, %d)",
                   config->layout.x, config->layout.y);

      return FALSE;
    }

  if (!config->monitor_configs)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Logical monitor is empty");

      return FALSE;
    }

  if (gf_monitor_transform_is_rotated (config->transform))
    {
      expected_mode_width = config->layout.height;
      expected_mode_height = config->layout.width;
    }
  else
    {
      expected_mode_width = config->layout.width;
      expected_mode_height = config->layout.height;
    }

  switch (layout_mode)
    {
      case GF_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
        expected_mode_width = roundf (expected_mode_width * config->scale);
        expected_mode_height = roundf (expected_mode_height * config->scale);
        break;

      case GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
      default:
        break;
    }

  for (l = config->monitor_configs; l; l = l->next)
    {
      GfMonitorConfig *monitor_config = l->data;

      if (monitor_config->mode_spec->width != expected_mode_width ||
          monitor_config->mode_spec->height != expected_mode_height)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Monitor modes in logical monitor conflict");

          return FALSE;
        }
    }

  return TRUE;
}
