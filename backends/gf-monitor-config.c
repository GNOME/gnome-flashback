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

#include <gio/gio.h>

#include "gf-monitor-config-private.h"
#include "gf-monitor-spec-private.h"

GfMonitorConfig *
gf_monitor_config_new (GfMonitor     *monitor,
                       GfMonitorMode *mode)
{
  GfMonitorSpec *spec;
  GfMonitorModeSpec *mode_spec;
  GfMonitorConfig *config;

  spec = gf_monitor_get_spec (monitor);
  mode_spec = gf_monitor_mode_get_spec (mode);

  config = g_new0 (GfMonitorConfig, 1);
  config->monitor_spec = gf_monitor_spec_clone (spec);
  config->mode_spec = g_memdup2 (mode_spec, sizeof (GfMonitorModeSpec));
  config->enable_underscanning = gf_monitor_is_underscanning (monitor);

  config->has_max_bpc = gf_monitor_get_max_bpc (monitor, &config->max_bpc);

  return config;
}

void
gf_monitor_config_free (GfMonitorConfig *config)
{
  if (config->monitor_spec != NULL)
    gf_monitor_spec_free (config->monitor_spec);

  g_free (config->mode_spec);
  g_free (config);
}

gboolean
gf_verify_monitor_config (GfMonitorConfig  *config,
                          GError          **error)
{
  if (config->monitor_spec && config->mode_spec)
    {
      return TRUE;
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Monitor config incomplete");

      return FALSE;
    }
}
