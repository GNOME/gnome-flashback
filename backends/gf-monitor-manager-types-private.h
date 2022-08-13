/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2017-2019 Alberts Muktupāvels
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

#ifndef GF_MONITOR_MANAGER_TYPES_PRIVATE_H
#define GF_MONITOR_MANAGER_TYPES_PRIVATE_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GfDBusDisplayConfig GfDBusDisplayConfig;

typedef struct _GfMonitorConfigManager GfMonitorConfigManager;
typedef struct _GfMonitorConfigStore GfMonitorConfigStore;
typedef struct _GfMonitorsConfig GfMonitorsConfig;

typedef struct _GfMonitor GfMonitor;
typedef struct _GfMonitorSpec GfMonitorSpec;
typedef struct _GfLogicalMonitor GfLogicalMonitor;

typedef struct _GfMonitorMode GfMonitorMode;

typedef struct _GfGpu GfGpu;

typedef struct _GfCrtc GfCrtc;
typedef struct _GfOutput GfOutput;
typedef struct _GfOutputCtm GfOutputCtm;
typedef struct _GfCrtcMode GfCrtcMode;

G_END_DECLS

#endif
