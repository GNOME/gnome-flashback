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

#include "gf-monitor-config-store-private.h"
#include "gf-monitor-config-manager-private.h"
#include "gf-monitor-spec-private.h"

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
