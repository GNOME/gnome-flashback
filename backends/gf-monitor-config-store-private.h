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
 * - src/backends/meta-monitor-config-store.h
 */

#ifndef GF_MONITOR_CONFIG_STORE_PRIVATE_H
#define GF_MONITOR_CONFIG_STORE_PRIVATE_H

#include "gf-monitors-config-private.h"

G_BEGIN_DECLS

typedef struct
{
  gboolean enable_dbus;
} GfMonitorConfigPolicy;

#define GF_TYPE_MONITOR_CONFIG_STORE (gf_monitor_config_store_get_type ())
G_DECLARE_FINAL_TYPE (GfMonitorConfigStore, gf_monitor_config_store,
                      GF, MONITOR_CONFIG_STORE, GObject)

GfMonitorConfigStore        *gf_monitor_config_store_new                 (GfMonitorManager      *monitor_manager);

GfMonitorsConfig            *gf_monitor_config_store_lookup              (GfMonitorConfigStore  *config_store,
                                                                          GfMonitorsConfigKey   *key);

void                         gf_monitor_config_store_add                 (GfMonitorConfigStore  *config_store,
                                                                          GfMonitorsConfig      *config);

void                         gf_monitor_config_store_remove              (GfMonitorConfigStore  *config_store,
                                                                          GfMonitorsConfig      *config);

gboolean                     gf_monitor_config_store_set_custom          (GfMonitorConfigStore  *config_store,
                                                                          const gchar           *read_path,
                                                                          const gchar           *write_path,
                                                                          GfMonitorsConfigFlag   flags,
                                                                          GError               **error);

gint                         gf_monitor_config_store_get_config_count    (GfMonitorConfigStore  *config_store);

GfMonitorManager            *gf_monitor_config_store_get_monitor_manager (GfMonitorConfigStore  *config_store);

void                         gf_monitor_config_store_reset               (GfMonitorConfigStore  *config_store);

const GfMonitorConfigPolicy *gf_monitor_config_store_get_policy          (GfMonitorConfigStore  *config_store);

G_END_DECLS

#endif
