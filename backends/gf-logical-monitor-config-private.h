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

#ifndef GF_LOGICAL_MONITOR_CONFIG_PRIVATE_H
#define GF_LOGICAL_MONITOR_CONFIG_PRIVATE_H

#include "gf-monitor-manager-private.h"
#include "gf-rectangle.h"

G_BEGIN_DECLS

typedef struct
{
  GfRectangle         layout;
  GList              *monitor_configs;
  GfMonitorTransform  transform;
  gfloat              scale;
  gboolean            is_primary;
  gboolean            is_presentation;
} GfLogicalMonitorConfig;

void     gf_logical_monitor_config_free          (GfLogicalMonitorConfig      *config);

gboolean gf_logical_monitor_configs_have_monitor (GList                       *logical_monitor_configs,
                                                  GfMonitorSpec               *monitor_spec);

gboolean gf_verify_logical_monitor_config        (GfLogicalMonitorConfig      *config,
                                                  GfLogicalMonitorLayoutMode   layout_mode,
                                                  GfMonitorManager            *monitor_manager,
                                                  GError                     **error);

G_END_DECLS

#endif
