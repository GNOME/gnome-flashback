/*
 * Copyright (C) 2016 Red Hat
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

#ifndef GF_MONITOR_CONFIG_UTILS_H
#define GF_MONITOR_CONFIG_UTILS_H

#include <glib.h>

G_BEGIN_DECLS

GList *gf_clone_logical_monitor_config_list (GList *logical_monitor_configs_in);

G_END_DECLS

#endif
