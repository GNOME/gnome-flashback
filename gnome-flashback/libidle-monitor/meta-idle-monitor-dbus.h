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
 *
 * Adapted from gnome-session/gnome-session/gs-idle-monitor.c and
 *         from gnome-desktop/libgnome-desktop/gnome-idle-monitor.c
 */

#ifndef META_IDLE_MONITOR_DBUS_H
#define META_IDLE_MONITOR_DBUS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define META_TYPE_IDLE_MONITOR_DBUS         (meta_idle_monitor_dbus_get_type ())
#define META_IDLE_MONITOR_DBUS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), META_TYPE_IDLE_MONITOR_DBUS, MetaIdleMonitorDBus))
#define META_IDLE_MONITOR_DBUS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k),    META_TYPE_IDLE_MONITOR_DBUS, MetaIdleMonitorDBusClass))
#define META_IS_IDLE_MONITOR_DBUS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), META_TYPE_IDLE_MONITOR_DBUS))
#define META_IS_IDLE_MONITOR_DBUS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k),    META_TYPE_IDLE_MONITOR_DBUS))
#define META_IDLE_MONITOR_DBUS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  META_TYPE_IDLE_MONITOR_DBUS, MetaIdleMonitorDBusClass))

typedef struct _MetaIdleMonitorDBus        MetaIdleMonitorDBus;
typedef struct _MetaIdleMonitorDBusClass   MetaIdleMonitorDBusClass;
typedef struct _MetaIdleMonitorDBusPrivate MetaIdleMonitorDBusPrivate;

struct _MetaIdleMonitorDBus {
	GObject                     parent;
	MetaIdleMonitorDBusPrivate *priv;
};

struct _MetaIdleMonitorDBusClass {
    GObjectClass parent_class;
};

GType                meta_idle_monitor_dbus_get_type (void);
MetaIdleMonitorDBus *meta_idle_monitor_dbus_new      (void);

G_END_DECLS

#endif
