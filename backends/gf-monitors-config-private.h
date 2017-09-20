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
 * - src/backends/meta-monitor-config-manager.h
 */

#ifndef GF_MONITORS_CONFIG_PRIVATE_H
#define GF_MONITORS_CONFIG_PRIVATE_H

#include "gf-monitor-manager-private.h"

G_BEGIN_DECLS

typedef struct
{
  GList *monitor_specs;
} GfMonitorsConfigKey;

typedef enum
{
  GF_MONITORS_CONFIG_FLAG_NONE = 0,
  GF_MONITORS_CONFIG_FLAG_MIGRATED = (1 << 0),
} GfMonitorsConfigFlag;

struct _GfMonitorsConfig
{
  GObject                     parent;

  GfMonitorsConfigKey        *key;
  GList                      *logical_monitor_configs;

  GfMonitorsConfigFlag        flags;

  GfLogicalMonitorLayoutMode  layout_mode;
};

#define GF_TYPE_MONITORS_CONFIG (gf_monitors_config_get_type ())
G_DECLARE_FINAL_TYPE (GfMonitorsConfig, gf_monitors_config,
                      GF, MONITORS_CONFIG, GObject)

GfMonitorsConfig *gf_monitors_config_new       (GList                       *logical_monitor_configs,
                                                GfLogicalMonitorLayoutMode   layout_mode,
                                                GfMonitorsConfigFlag         flags);

guint             gf_monitors_config_key_hash  (gconstpointer                data);

gboolean          gf_monitors_config_key_equal (gconstpointer                data_a,
                                                gconstpointer                data_b);

void              gf_monitors_config_key_free  (GfMonitorsConfigKey         *config_key);

gboolean          gf_verify_monitors_config    (GfMonitorsConfig            *config,
                                                GfMonitorManager            *monitor_manager,
                                                GError                     **error);

G_END_DECLS

#endif
