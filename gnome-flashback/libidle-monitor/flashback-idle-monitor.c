/* 
 * Copyright (C) 2014 Alberts Muktupāvels
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

#include <gtk/gtk.h>
#include "config.h"
#include "dbus-idle-monitor.h"
#include "flashback-idle-monitor.h"

struct _FlashbackIdleMonitorPrivate {
	gint bus_name;
};

G_DEFINE_TYPE (FlashbackIdleMonitor, flashback_idle_monitor, G_TYPE_OBJECT);

/*static void
handle_add_idle_watch (DBusIdleMonitor       *object,
                       GDBusMethodInvocation *invocation,
                       guint64                interval,
                       gpointer               user_data)
{
	g_warning ("AddIdleWatch is not implemented!");
}

static void
handle_add_user_active_watch (DBusIdleMonitor       *object,
                              GDBusMethodInvocation *invocation,
                              gpointer               user_data)
{
	g_warning ("AddUserActiveWatch is not implemented!");
}

static void
handle_remove_watch (DBusIdleMonitor       *object,
                     GDBusMethodInvocation *invocation,
                     guint                  id,
                     gpointer               user_data)
{
	g_warning ("RemoveWatch is not implemented!");
}

static void
handle_get_idletime (DBusIdleMonitor       *object,
                     GDBusMethodInvocation *invocation,
                     guint                  id,
                     gpointer               user_data)
{
	g_warning ("GetIdletime is not implemented!");
}*/

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
	FlashbackIdleMonitor *monitor = FLASHBACK_IDLE_MONITOR (user_data);
	GDBusObjectManagerServer *manager = g_dbus_object_manager_server_new ("/org/gnome/Mutter/IdleMonitor");

	g_dbus_object_manager_server_set_connection (manager, connection);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const char      *name,
              gpointer         user_data)
{
}

static void
flashback_idle_monitor_finalize (GObject *object)
{
	FlashbackIdleMonitor *monitor = FLASHBACK_IDLE_MONITOR (object);

	if (monitor->priv->bus_name) {
		g_bus_unown_name (monitor->priv->bus_name);
		monitor->priv->bus_name = 0;
	}

	G_OBJECT_CLASS (flashback_idle_monitor_parent_class)->finalize (object);
}

static void
flashback_idle_monitor_init (FlashbackIdleMonitor *monitor)
{
	monitor->priv = G_TYPE_INSTANCE_GET_PRIVATE (monitor,
	                                             FLASHBACK_TYPE_IDLE_MONITOR,
	                                             FlashbackIdleMonitorPrivate);

	monitor->priv->bus_name = g_bus_own_name (G_BUS_TYPE_SESSION,
	                                          "org.gnome.Mutter.IdleMonitor",
	                                          G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
	                                          G_BUS_NAME_OWNER_FLAGS_REPLACE,
	                                          on_bus_acquired,
	                                          on_name_acquired,
	                                          on_name_lost,
	                                          monitor,
	                                          NULL);
}

static void
flashback_idle_monitor_class_init (FlashbackIdleMonitorClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = flashback_idle_monitor_finalize;

	g_type_class_add_private (class, sizeof (FlashbackIdleMonitorPrivate));
}

FlashbackIdleMonitor *
flashback_idle_monitor_new (void)
{
	return g_object_new (FLASHBACK_TYPE_IDLE_MONITOR,
	                     NULL);
}
