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

#ifndef GF_MONITOR_MANAGER_PRIVATE_H
#define GF_MONITOR_MANAGER_PRIVATE_H

#include "gf-backend-private.h"
#include "gf-dbus-display-config.h"
#include "gf-monitor-manager.h"

G_BEGIN_DECLS

typedef struct _GfMonitorManagerClass GfMonitorManagerClass;

#define GF_TYPE_MONITOR_MANAGER         (gf_monitor_manager_get_type ())
#define GF_MONITOR_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GF_TYPE_MONITOR_MANAGER, GfMonitorManager))
#define GF_MONITOR_MANAGER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    GF_TYPE_MONITOR_MANAGER, GfMonitorManagerClass))
#define GF_IS_MONITOR_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GF_TYPE_MONITOR_MANAGER))
#define GF_IS_MONITOR_MANAGER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    GF_TYPE_MONITOR_MANAGER))
#define GF_MONITOR_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  GF_TYPE_MONITOR_MANAGER, GfMonitorManagerClass))

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GfMonitorManager, g_object_unref)

struct _GfMonitorManager
{
  GfDBusDisplayConfigSkeleton parent;
};

struct _GfMonitorManagerClass
{
  GfDBusDisplayConfigSkeletonClass parent_class;
};

GType gf_monitor_manager_get_type (void);

G_END_DECLS

#endif
