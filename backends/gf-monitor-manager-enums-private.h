/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
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
 * - src/backends/meta-monitor-manager-private.h
 */

#ifndef GF_MONITOR_MANAGER_ENUMS_PRIVATE_H
#define GF_MONITOR_MANAGER_ENUMS_PRIVATE_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  GF_MONITOR_MANAGER_CAPABILITY_NONE = 0,
  GF_MONITOR_MANAGER_CAPABILITY_LAYOUT_MODE = (1 << 0),
  GF_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED = (1 << 1)
} GfMonitorManagerCapability;

/* Equivalent to the 'method' enum in org.gnome.Mutter.DisplayConfig */
typedef enum
{
  GF_MONITORS_CONFIG_METHOD_VERIFY = 0,
  GF_MONITORS_CONFIG_METHOD_TEMPORARY = 1,
  GF_MONITORS_CONFIG_METHOD_PERSISTENT = 2
} GfMonitorsConfigMethod;

/* Equivalent to the 'layout-mode' enum in org.gnome.Mutter.DisplayConfig */
typedef enum
{
  GF_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL = 1,
  GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL = 2
} GfLogicalMonitorLayoutMode;

/* This matches the values in drm_mode.h */
typedef enum
{
  GF_CONNECTOR_TYPE_Unknown = 0,
  GF_CONNECTOR_TYPE_VGA = 1,
  GF_CONNECTOR_TYPE_DVII = 2,
  GF_CONNECTOR_TYPE_DVID = 3,
  GF_CONNECTOR_TYPE_DVIA = 4,
  GF_CONNECTOR_TYPE_Composite = 5,
  GF_CONNECTOR_TYPE_SVIDEO = 6,
  GF_CONNECTOR_TYPE_LVDS = 7,
  GF_CONNECTOR_TYPE_Component = 8,
  GF_CONNECTOR_TYPE_9PinDIN = 9,
  GF_CONNECTOR_TYPE_DisplayPort = 10,
  GF_CONNECTOR_TYPE_HDMIA = 11,
  GF_CONNECTOR_TYPE_HDMIB = 12,
  GF_CONNECTOR_TYPE_TV = 13,
  GF_CONNECTOR_TYPE_eDP = 14,
  GF_CONNECTOR_TYPE_VIRTUAL = 15,
  GF_CONNECTOR_TYPE_DSI = 16,
  GF_CONNECTOR_TYPE_DPI = 17,
  GF_CONNECTOR_TYPE_WRITEBACK = 18,
  GF_CONNECTOR_TYPE_SPI = 19,
  GF_CONNECTOR_TYPE_USB = 20
} GfConnectorType;

G_END_DECLS

#endif
