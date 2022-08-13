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
 * - src/backends/meta-monitor-config-migration.h
 */

#ifndef GF_MONITOR_CONFIG_MIGRATION_PRIVATE_H
#define GF_MONITOR_CONFIG_MIGRATION_PRIVATE_H

#include <gio/gio.h>

#include "gf-monitors-config-private.h"

G_BEGIN_DECLS

gboolean gf_migrate_old_monitors_config      (GfMonitorConfigStore  *config_store,
                                              GFile                 *in_file,
                                              GError               **error);

gboolean gf_migrate_old_user_monitors_config (GfMonitorConfigStore  *config_store,
                                              GError               **error);

gboolean gf_finish_monitors_config_migration (GfMonitorManager      *monitor_manager,
                                              GfMonitorsConfig      *config,
                                              GError               **error);

G_END_DECLS

#endif
