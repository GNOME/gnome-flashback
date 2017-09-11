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
 * - src/backends/meta-monitor.h
 */

#ifndef GF_MONITOR_TILED_PRIVATE_H
#define GF_MONITOR_TILED_PRIVATE_H

#include <stdint.h>
#include "gf-monitor-private.h"

G_BEGIN_DECLS

#define GF_TYPE_MONITOR_TILED (gf_monitor_tiled_get_type ())
G_DECLARE_FINAL_TYPE (GfMonitorTiled, gf_monitor_tiled,
                      GF, MONITOR_TILED, GfMonitor)

GfMonitorTiled *gf_monitor_tiled_new               (GfMonitorManager *monitor_manager,
                                                    GfOutput         *output);

uint32_t        gf_monitor_tiled_get_tile_group_id (GfMonitorTiled   *monitor_tiled);

G_END_DECLS

#endif
