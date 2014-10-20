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

#include "config.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/sync.h>

#include "meta-backend.h"
#include "meta-idle-monitor.h"
#include "meta-idle-monitor-dbus.h"
#include "meta-dbus-idle-monitor.h"

struct _MetaIdleMonitorDBusPrivate {
	gint dbus_name_id;

	int  xsync_event_base;
	int  xsync_error_base;
};

G_DEFINE_TYPE_WITH_PRIVATE (MetaIdleMonitorDBus, meta_idle_monitor_dbus, G_TYPE_OBJECT);

static gboolean
handle_get_idletime (MetaDBusIdleMonitor   *skeleton,
                     GDBusMethodInvocation *invocation,
                     MetaIdleMonitor       *monitor)
{
  guint64 idletime;

  idletime = meta_idle_monitor_get_idletime (monitor);
  meta_dbus_idle_monitor_complete_get_idletime (skeleton, invocation, idletime);

  return TRUE;
}

typedef struct {
  MetaDBusIdleMonitor *dbus_monitor;
  MetaIdleMonitor *monitor;
  char *dbus_name;
  guint watch_id;
  guint name_watcher_id;
} DBusWatch;

static void
destroy_dbus_watch (gpointer data)
{
  DBusWatch *watch = data;

  g_object_unref (watch->dbus_monitor);
  g_object_unref (watch->monitor);
  g_free (watch->dbus_name);
  g_bus_unwatch_name (watch->name_watcher_id);

  g_slice_free (DBusWatch, watch);
}

static void
dbus_idle_callback (MetaIdleMonitor *monitor,
                    guint            watch_id,
                    gpointer         user_data)
{
  DBusWatch *watch = user_data;
  GDBusInterfaceSkeleton *skeleton = G_DBUS_INTERFACE_SKELETON (watch->dbus_monitor);

  g_dbus_connection_emit_signal (g_dbus_interface_skeleton_get_connection (skeleton),
                                 watch->dbus_name,
                                 g_dbus_interface_skeleton_get_object_path (skeleton),
                                 "org.gnome.Mutter.IdleMonitor",
                                 "WatchFired",
                                 g_variant_new ("(u)", watch_id),
                                 NULL);
}

static void
name_vanished_callback (GDBusConnection *connection,
                        const char      *name,
                        gpointer         user_data)
{
  DBusWatch *watch = user_data;

  meta_idle_monitor_remove_watch (watch->monitor, watch->watch_id);
}

static DBusWatch *
make_dbus_watch (MetaDBusIdleMonitor   *skeleton,
                 GDBusMethodInvocation *invocation,
                 MetaIdleMonitor       *monitor)
{
  DBusWatch *watch;

  watch = g_slice_new (DBusWatch);
  watch->dbus_monitor = g_object_ref (skeleton);
  watch->monitor = g_object_ref (monitor);
  watch->dbus_name = g_strdup (g_dbus_method_invocation_get_sender (invocation));
  watch->name_watcher_id = g_bus_watch_name_on_connection (g_dbus_method_invocation_get_connection (invocation),
                                                           watch->dbus_name,
                                                           G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                           NULL, /* appeared */
                                                           name_vanished_callback,
                                                           watch, NULL);

  return watch;
}

static gboolean
handle_add_idle_watch (MetaDBusIdleMonitor   *skeleton,
                       GDBusMethodInvocation *invocation,
                       guint64                interval,
                       MetaIdleMonitor       *monitor)
{
  DBusWatch *watch;

  watch = make_dbus_watch (skeleton, invocation, monitor);
  watch->watch_id = meta_idle_monitor_add_idle_watch (monitor, interval,
                                                      dbus_idle_callback, watch, destroy_dbus_watch);

  meta_dbus_idle_monitor_complete_add_idle_watch (skeleton, invocation, watch->watch_id);

  return TRUE;
}

static gboolean
handle_add_user_active_watch (MetaDBusIdleMonitor   *skeleton,
                              GDBusMethodInvocation *invocation,
                              MetaIdleMonitor       *monitor)
{

  DBusWatch *watch;

  watch = make_dbus_watch (skeleton, invocation, monitor);
  watch->watch_id = meta_idle_monitor_add_user_active_watch (monitor,
                                                             dbus_idle_callback, watch,
                                                             destroy_dbus_watch);

  meta_dbus_idle_monitor_complete_add_user_active_watch (skeleton, invocation, watch->watch_id);

  return TRUE;
}

static gboolean
handle_remove_watch (MetaDBusIdleMonitor   *skeleton,
                     GDBusMethodInvocation *invocation,
                     guint                  id,
                     MetaIdleMonitor       *monitor)
{
  meta_idle_monitor_remove_watch (monitor, id);
  meta_dbus_idle_monitor_complete_remove_watch (skeleton, invocation);

  return TRUE;
}

static void
create_monitor_skeleton (GDBusObjectManagerServer *manager,
                         MetaIdleMonitor          *monitor,
                         const char               *path)
{
  MetaDBusIdleMonitor *skeleton;
  MetaDBusObjectSkeleton *object;

  skeleton = meta_dbus_idle_monitor_skeleton_new ();
  g_signal_connect_object (skeleton, "handle-add-idle-watch",
                           G_CALLBACK (handle_add_idle_watch), monitor, 0);
  g_signal_connect_object (skeleton, "handle-add-user-active-watch",
                           G_CALLBACK (handle_add_user_active_watch), monitor, 0);
  g_signal_connect_object (skeleton, "handle-remove-watch",
                           G_CALLBACK (handle_remove_watch), monitor, 0);
  g_signal_connect_object (skeleton, "handle-get-idletime",
                           G_CALLBACK (handle_get_idletime), monitor, 0);

  object = meta_dbus_object_skeleton_new (path);
  meta_dbus_object_skeleton_set_idle_monitor (object, skeleton);

  g_dbus_object_manager_server_export (manager, G_DBUS_OBJECT_SKELETON (object));

  g_object_unref (skeleton);
  g_object_unref (object);
}

static void
on_device_added (GdkDeviceManager         *device_manager,
                 GdkDevice                *device,
                 GDBusObjectManagerServer *manager)
{

  MetaIdleMonitor *monitor;
  gint device_id;
  gchar *path;

  device_id = gdk_x11_device_get_id (device);
  monitor = meta_idle_monitor_get_for_device (device_id);
  path = g_strdup_printf ("/org/gnome/Mutter/IdleMonitor/Device%d", device_id);

  create_monitor_skeleton (manager, monitor, path);
  g_free (path);
}

static void
on_device_removed (GdkDeviceManager         *device_manager,
                   GdkDevice                *device,
                   GDBusObjectManagerServer *manager)
{
  gint device_id;
  gchar *path;

  device_id = gdk_x11_device_get_id (device);
  path = g_strdup_printf ("/org/gnome/Mutter/IdleMonitor/Device%d", device_id);
  g_dbus_object_manager_server_unexport (manager, path);
  g_free (path);
}

static void
on_device_changed (GdkDeviceManager         *device_manager,
                   GdkDevice                *device,
                   GDBusObjectManagerServer *manager)
{
	if (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_FLOATING)
		on_device_removed (device_manager, device, manager);
	else
		on_device_added (device_manager, device, manager);
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const char      *name,
                 gpointer         user_data)
{
  GDBusObjectManagerServer *manager;
  GdkDeviceManager *device_manager;
  MetaIdleMonitor *monitor;
  GList *devices, *iter;
  char *path;

  manager = g_dbus_object_manager_server_new ("/org/gnome/Mutter/IdleMonitor");

  /* We never clear the core monitor, as that's supposed to cumulate idle times from
     all devices */
  monitor = meta_idle_monitor_get_core ();
  path = g_strdup ("/org/gnome/Mutter/IdleMonitor/Core");
  create_monitor_skeleton (manager, monitor, path);
  g_free (path);

  device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);
  devices = g_list_concat (devices, gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_SLAVE));

  for (iter = devices; iter; iter = iter->next)
    on_device_added (device_manager, iter->data, manager);

  g_list_free (devices);

  g_signal_connect_object (device_manager, "device-added",
                           G_CALLBACK (on_device_added), manager, 0);
  g_signal_connect_object (device_manager, "device-removed",
                           G_CALLBACK (on_device_removed), manager, 0);
  g_signal_connect_object (device_manager, "device-changed",
                           G_CALLBACK (on_device_changed), manager, 0);

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

static GdkFilterReturn
filter_func (GdkXEvent *xevent,
             GdkEvent  *event,
             gpointer   user_data)
{
	MetaIdleMonitorDBus *monitor = META_IDLE_MONITOR_DBUS (user_data);
	MetaBackend *backend = meta_get_backend ();
	XEvent *xev = (XEvent *) xevent;
	int i;

	if (xev->type == (monitor->priv->xsync_event_base + XSyncAlarmNotify)) {
		for (i = 0; i <= backend->device_id_max; i++) {
			if (backend->device_monitors[i]) {
				meta_idle_monitor_xsync_handle_xevent (backend->device_monitors[i], (XSyncAlarmNotifyEvent*) xev);
			}
		}
	}

	return GDK_FILTER_CONTINUE;
}

static void
meta_idle_monitor_dbus_dispose (GObject *object)
{
	MetaIdleMonitorDBus *monitor = META_IDLE_MONITOR_DBUS (object);

	if (monitor->priv->dbus_name_id) {
		g_bus_unown_name (monitor->priv->dbus_name_id);
		monitor->priv->dbus_name_id = 0;
	}

	G_OBJECT_CLASS (meta_idle_monitor_dbus_parent_class)->dispose (object);
}

static void
meta_idle_monitor_dbus_finalize (GObject *object)
{
	MetaIdleMonitorDBus *monitor = META_IDLE_MONITOR_DBUS (object);

	gdk_window_remove_filter (NULL, filter_func, monitor);

	G_OBJECT_CLASS (meta_idle_monitor_dbus_parent_class)->finalize (object);
}

static void
meta_idle_monitor_dbus_init (MetaIdleMonitorDBus *monitor)
{
	GdkDisplay *display;
	Display *xdisplay;
	int major, minor;

    monitor->priv = meta_idle_monitor_dbus_get_instance_private (monitor);
	monitor->priv->dbus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
	                                              "org.gnome.Mutter.IdleMonitor",
	                                              G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
	                                              G_BUS_NAME_OWNER_FLAGS_REPLACE,
	                                              on_bus_acquired,
	                                              on_name_acquired,
	                                              on_name_lost,
	                                              NULL, NULL);

	display = gdk_display_get_default ();
	xdisplay = gdk_x11_display_get_xdisplay (display);

	if (!XSyncQueryExtension (xdisplay, &monitor->priv->xsync_event_base, &monitor->priv->xsync_error_base))
    	g_critical ("Could not query XSync extension");

	if (!XSyncInitialize (xdisplay, &major, &minor))
    	g_critical ("Could not initialize XSync");

	gdk_window_add_filter (NULL, filter_func, monitor);
}

static void
meta_idle_monitor_dbus_class_init (MetaIdleMonitorDBusClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = meta_idle_monitor_dbus_dispose;
	object_class->finalize = meta_idle_monitor_dbus_finalize;
}

MetaIdleMonitorDBus *
meta_idle_monitor_dbus_new (void)
{
	return g_object_new (META_TYPE_IDLE_MONITOR_DBUS, NULL);
}
