/*
 * Copyright 2013 Red Hat, Inc.
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

#ifndef META_IDLE_MONITOR_H
#define META_IDLE_MONITOR_H

#include <glib-object.h>

#define META_TYPE_IDLE_MONITOR            (meta_idle_monitor_get_type ())
#define META_IDLE_MONITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_IDLE_MONITOR, MetaIdleMonitor))
#define META_IDLE_MONITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_IDLE_MONITOR, MetaIdleMonitorClass))
#define META_IS_IDLE_MONITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_IDLE_MONITOR))
#define META_IS_IDLE_MONITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_IDLE_MONITOR))
#define META_IDLE_MONITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_IDLE_MONITOR, MetaIdleMonitorClass))

typedef struct _MetaIdleMonitor        MetaIdleMonitor;
typedef struct _MetaIdleMonitorClass   MetaIdleMonitorClass;

typedef void (*MetaIdleMonitorWatchFunc) (MetaIdleMonitor *monitor,
                                          guint            watch_id,
                                          gpointer         user_data);

struct _MetaIdleMonitorClass
{
  GObjectClass parent_class;
};

GType meta_idle_monitor_get_type (void);

guint         meta_idle_monitor_add_idle_watch        (MetaIdleMonitor          *monitor,
						       guint64                   interval_msec,
						       MetaIdleMonitorWatchFunc  callback,
						       gpointer                  user_data,
						       GDestroyNotify            notify);

guint         meta_idle_monitor_add_user_active_watch (MetaIdleMonitor          *monitor,
						       MetaIdleMonitorWatchFunc  callback,
						       gpointer                  user_data,
						       GDestroyNotify            notify);

void          meta_idle_monitor_remove_watch          (MetaIdleMonitor          *monitor,
						       guint                     id);
gint64        meta_idle_monitor_get_idletime          (MetaIdleMonitor          *monitor);

void          meta_idle_monitor_reset_idletime        (MetaIdleMonitor          *monitor);

#endif
