/*
 * Copyright (C) 2013 Red Hat, Inc.
 * Copyright (C) 2015 Alberts Muktupāvels
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

#include "flashback-idle-monitor.h"
#include "meta-backend.h"
#include "meta-idle-monitor.h"
#include "meta-idle-monitor-xsync.h"
#include "meta-dbus-idle-monitor.h"

struct _FlashbackIdleMonitor
{
  GObject                   parent;

  gint                      dbus_name_id;

  gint                      xsync_event_base;
  gint                      xsync_error_base;

  GDBusObjectManagerServer *server;
};

G_DEFINE_TYPE (FlashbackIdleMonitor, flashback_idle_monitor, G_TYPE_OBJECT)

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
  DBusWatch *watch;
  GDBusInterfaceSkeleton *skeleton;
  GDBusConnection *connection;
  const gchar *path;

  watch = (DBusWatch *) user_data;
  skeleton = G_DBUS_INTERFACE_SKELETON (watch->dbus_monitor);
  connection = g_dbus_interface_skeleton_get_connection (skeleton);
  path = g_dbus_interface_skeleton_get_object_path (skeleton);

  if (connection == NULL)
    return;

  g_dbus_connection_emit_signal (connection, watch->dbus_name, path,
                                 "org.gnome.Mutter.IdleMonitor",
                                 "WatchFired", g_variant_new ("(u)", watch_id),
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
  GDBusConnection *connection;
  const gchar *sender;
  DBusWatch *watch;

  connection = g_dbus_method_invocation_get_connection (invocation);
  sender = g_dbus_method_invocation_get_sender (invocation);

  watch = g_slice_new (DBusWatch);
  watch->dbus_monitor = g_object_ref (skeleton);
  watch->monitor = g_object_ref (monitor);
  watch->dbus_name = g_strdup (sender);
  watch->name_watcher_id = g_bus_watch_name_on_connection (connection,
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
                                                      dbus_idle_callback,
                                                      watch,
                                                      destroy_dbus_watch);

  meta_dbus_idle_monitor_complete_add_idle_watch (skeleton, invocation,
                                                  watch->watch_id);

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
                                                             dbus_idle_callback,
                                                             watch,
                                                             destroy_dbus_watch);

  meta_dbus_idle_monitor_complete_add_user_active_watch (skeleton, invocation,
                                                         watch->watch_id);

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
create_monitor_skeleton (GDBusObjectManagerServer *server,
                         MetaIdleMonitor          *monitor,
                         const gchar              *path)
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

  g_dbus_object_manager_server_export (server, G_DBUS_OBJECT_SKELETON (object));

  g_object_unref (skeleton);
  g_object_unref (object);
}

static void
on_device_added (GdkDeviceManager *device_manager,
                 GdkDevice        *device,
                 gpointer          user_data)
{
  FlashbackIdleMonitor *idle_monitor;
  MetaIdleMonitor *monitor;
  gint device_id;
  gchar *path;

  idle_monitor = FLASHBACK_IDLE_MONITOR (user_data);

  device_id = gdk_x11_device_get_id (device);
  monitor = meta_idle_monitor_get_for_device (device_id);
  path = g_strdup_printf ("/org/gnome/Mutter/IdleMonitor/Device%d", device_id);

  create_monitor_skeleton (idle_monitor->server, monitor, path);
  g_free (path);
}

static void
on_device_removed (GdkDeviceManager *device_manager,
                   GdkDevice        *device,
                   gpointer          user_data)
{
  FlashbackIdleMonitor *monitor;
  gint device_id;
  gchar *path;

  monitor = FLASHBACK_IDLE_MONITOR (user_data);

  device_id = gdk_x11_device_get_id (device);
  path = g_strdup_printf ("/org/gnome/Mutter/IdleMonitor/Device%d", device_id);

  g_dbus_object_manager_server_unexport (monitor->server, path);
  g_free (path);
}

static void
on_device_changed (GdkDeviceManager *device_manager,
                   GdkDevice        *device,
                   gpointer          user_data)
{
  FlashbackIdleMonitor *monitor;

  monitor = FLASHBACK_IDLE_MONITOR (user_data);

  if (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_FLOATING)
    on_device_removed (device_manager, device, monitor);
  else
    on_device_added (device_manager, device, monitor);
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const char      *name,
                 gpointer         user_data)
{
  FlashbackIdleMonitor *idle_monitor;
  const gchar *server_path;
  MetaIdleMonitor *monitor;
  const gchar *core_path;
  GdkDisplay *display;
  GdkDeviceManager *device_manager;
  GList *master;
  GList *slave;
  GList *devices;
  GList *iter;

  idle_monitor = FLASHBACK_IDLE_MONITOR (user_data);

  server_path = "/org/gnome/Mutter/IdleMonitor";
  idle_monitor->server = g_dbus_object_manager_server_new (server_path);

  monitor = meta_idle_monitor_get_core ();
  core_path = "/org/gnome/Mutter/IdleMonitor/Core";
  create_monitor_skeleton (idle_monitor->server, monitor, core_path);

  display = gdk_display_get_default ();
  device_manager = gdk_display_get_device_manager (display);
  master = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);
  slave = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_SLAVE);
  devices = g_list_concat (master, slave);

  for (iter = devices; iter; iter = iter->next)
    {
      GdkDevice *device;

      device = (GdkDevice *) iter->data;

      on_device_added (device_manager, device, idle_monitor);
    }

  g_list_free (devices);

  g_signal_connect_object (device_manager, "device-added",
                           G_CALLBACK (on_device_added), idle_monitor, 0);
  g_signal_connect_object (device_manager, "device-removed",
                           G_CALLBACK (on_device_removed), idle_monitor, 0);
  g_signal_connect_object (device_manager, "device-changed",
                           G_CALLBACK (on_device_changed), idle_monitor, 0);

  g_dbus_object_manager_server_set_connection (idle_monitor->server, connection);
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
  FlashbackIdleMonitor *monitor;
  MetaBackend *backend;
  XEvent *xev;
  gint i;

  monitor = FLASHBACK_IDLE_MONITOR (user_data);
  backend = meta_get_backend ();
  xev = (XEvent *) xevent;

  if (xev->type == (monitor->xsync_event_base + XSyncAlarmNotify))
    {
      for (i = 0; i <= backend->device_id_max; i++)
        {
          if (backend->device_monitors[i])
            meta_idle_monitor_xsync_handle_xevent (backend->device_monitors[i],
                                                   (XSyncAlarmNotifyEvent*) xev);
        }
    }

  return GDK_FILTER_CONTINUE;
}

static void
flashback_idle_monitor_dispose (GObject *object)
{
  FlashbackIdleMonitor *monitor;
  GdkDisplay *display;
  GdkDeviceManager *device_manager;
  GList *master;
  GList *slave;
  GList *devices;
  GList *iter;
  const gchar *core_path;

  monitor = FLASHBACK_IDLE_MONITOR (object);

  if (monitor->dbus_name_id > 0)
    {
      g_bus_unown_name (monitor->dbus_name_id);
      monitor->dbus_name_id = 0;
    }

  display = gdk_display_get_default ();
  device_manager = gdk_display_get_device_manager (display);
  master = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);
  slave = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_SLAVE);
  devices = g_list_concat (master, slave);

  for (iter = devices; iter; iter = iter->next)
    {
      GdkDevice *device;

      device = (GdkDevice *) iter->data;

      on_device_removed (device_manager, device, monitor);
    }

  g_list_free (devices);

  core_path = "/org/gnome/Mutter/IdleMonitor/Core";
  g_dbus_object_manager_server_unexport (monitor->server, core_path);

  g_clear_object (&monitor->server);

  G_OBJECT_CLASS (flashback_idle_monitor_parent_class)->dispose (object);
}

static void
flashback_idle_monitor_finalize (GObject *object)
{
  FlashbackIdleMonitor *monitor;

  monitor = FLASHBACK_IDLE_MONITOR (object);

  gdk_window_remove_filter (NULL, (GdkFilterFunc) filter_func, monitor);

  G_OBJECT_CLASS (flashback_idle_monitor_parent_class)->finalize (object);
}

static void
flashback_idle_monitor_init (FlashbackIdleMonitor *monitor)
{
  GdkDisplay *display;
  Display *xdisplay;
  gint event_base;
  gint error_base;
  gint major;
  gint minor;

  monitor->dbus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                          "org.gnome.Mutter.IdleMonitor",
                                          G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                          G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                          (GBusAcquiredCallback) on_bus_acquired,
                                          (GBusNameAcquiredCallback) on_name_acquired,
                                          (GBusNameLostCallback) on_name_lost,
                                          monitor, NULL);

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  if (!XSyncQueryExtension (xdisplay, &event_base, &error_base))
    g_critical ("Could not query XSync extension");

  monitor->xsync_event_base = event_base;
  monitor->xsync_error_base = error_base;

  if (!XSyncInitialize (xdisplay, &major, &minor))
    g_critical ("Could not initialize XSync");

  gdk_window_add_filter (NULL, (GdkFilterFunc) filter_func, monitor);
}

static void
flashback_idle_monitor_class_init (FlashbackIdleMonitorClass *monitor_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (monitor_class);

  object_class->dispose = flashback_idle_monitor_dispose;
  object_class->finalize =flashback_idle_monitor_finalize;
}

FlashbackIdleMonitor *
flashback_idle_monitor_new (void)
{
  return g_object_new (FLASHBACK_TYPE_IDLE_MONITOR, NULL);
}
