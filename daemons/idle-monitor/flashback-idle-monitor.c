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

#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>

#include "flashback-idle-monitor.h"
#include "meta-idle-monitor.h"
#include "meta-dbus-idle-monitor.h"

typedef struct
{
  GSource               source;
  GPollFD               event_poll_fd;

  FlashbackIdleMonitor *monitor;
} XEventSource;

struct _FlashbackIdleMonitor
{
  GObject                   parent;

  Display                  *xdisplay;

  GSource                  *source;

  XSyncCounter              counter;
  XSyncAlarm                user_active_alarm;

  MetaIdleMonitor          *monitor;

  gint                      dbus_name_id;

  gint                      xsync_event_base;
  gint                      xsync_error_base;

  GDBusObjectManagerServer *server;

  guint                     upower_watch_id;
  GDBusProxy               *upower_proxy;
  gboolean                  lid_is_closed;
  gboolean                  on_battery;
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

static gboolean
handle_reset_idletime (MetaDBusIdleMonitor   *skeleton,
                       GDBusMethodInvocation *invocation,
                       MetaIdleMonitor       *monitor)
{
  if (g_getenv ("MUTTER_DEBUG_RESET_IDLETIME") == NULL)
    {
      const char *message;

      message = "This method is for testing purposes only. "
                "MUTTER_DEBUG_RESET_IDLETIME must be set to use it!";

      g_dbus_method_invocation_return_error_literal (invocation,
                                                     G_DBUS_ERROR,
                                                     G_DBUS_ERROR_UNKNOWN_METHOD,
                                                     message);
      return TRUE;
    }

  meta_idle_monitor_reset_idletime (monitor);
  meta_dbus_idle_monitor_complete_reset_idletime (skeleton, invocation);

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

  g_free (watch);
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

  watch = g_new0 (DBusWatch, 1);
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
  GDBusObjectSkeleton *object;

  skeleton = meta_dbus_idle_monitor_skeleton_new ();
  g_signal_connect_object (skeleton, "handle-add-idle-watch",
                           G_CALLBACK (handle_add_idle_watch), monitor, 0);
  g_signal_connect_object (skeleton, "handle-add-user-active-watch",
                           G_CALLBACK (handle_add_user_active_watch), monitor, 0);
  g_signal_connect_object (skeleton, "handle-remove-watch",
                           G_CALLBACK (handle_remove_watch), monitor, 0);
  g_signal_connect_object (skeleton, "handle-get-idletime",
                           G_CALLBACK (handle_get_idletime), monitor, 0);
  g_signal_connect_object (skeleton, "handle-reset-idletime",
                           G_CALLBACK (handle_reset_idletime), monitor, 0);

  object = g_dbus_object_skeleton_new (path);
  g_dbus_object_skeleton_add_interface (object,
                                        G_DBUS_INTERFACE_SKELETON (skeleton));

  g_dbus_object_manager_server_export (server, object);

  g_object_unref (skeleton);
  g_object_unref (object);
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const char      *name,
                 gpointer         user_data)
{
  FlashbackIdleMonitor *idle_monitor;
  const gchar *server_path;
  const gchar *core_path;

  idle_monitor = FLASHBACK_IDLE_MONITOR (user_data);

  server_path = "/org/gnome/Mutter/IdleMonitor";
  idle_monitor->server = g_dbus_object_manager_server_new (server_path);

  core_path = "/org/gnome/Mutter/IdleMonitor/Core";
  create_monitor_skeleton (idle_monitor->server, idle_monitor->monitor, core_path);

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

static void
ensure_alarm_rescheduled (Display    *dpy,
                          XSyncAlarm  alarm)
{
  XSyncAlarmAttributes attr;

  /* Some versions of Xorg have an issue where alarms aren't
   * always rescheduled. Calling XSyncChangeAlarm, even
   * without any attributes, will reschedule the alarm.
   */
  XSyncChangeAlarm (dpy, alarm, 0, &attr);
}

static void
handle_alarm_notify (FlashbackIdleMonitor  *self,
                     XSyncAlarmNotifyEvent *alarm_event)
{
  if (alarm_event->state != XSyncAlarmActive ||
      alarm_event->alarm != self->user_active_alarm)
    return;

  ensure_alarm_rescheduled (self->xdisplay, self->user_active_alarm);

  meta_idle_monitor_reset_idletime (self->monitor);
}

static void
handle_xevent (FlashbackIdleMonitor *monitor,
               XEvent               *xevent)
{
  if (xevent->type == (monitor->xsync_event_base + XSyncAlarmNotify))
    handle_alarm_notify (monitor, (XSyncAlarmNotifyEvent *) xevent);
}

static gboolean
x_event_source_prepare (GSource *source,
                        int     *timeout)
{
  XEventSource *x_source;
  FlashbackIdleMonitor *monitor;

  x_source = (XEventSource *) source;
  monitor = x_source->monitor;

  *timeout = -1;

  return XPending (monitor->xdisplay);
}

static gboolean
x_event_source_check (GSource *source)
{
  XEventSource *x_source;
  FlashbackIdleMonitor *monitor;

  x_source = (XEventSource *) source;
  monitor = x_source->monitor;

  return XPending (monitor->xdisplay);
}

static gboolean
x_event_source_dispatch (GSource     *source,
                         GSourceFunc  callback,
                         gpointer     user_data)
{
  XEventSource *x_source;
  FlashbackIdleMonitor *monitor;

  x_source = (XEventSource *) source;
  monitor = x_source->monitor;

  while (XPending (monitor->xdisplay))
    {
      XEvent event;

      XNextEvent (monitor->xdisplay, &event);
      handle_xevent (monitor, &event);
    }

  return TRUE;
}

static GSourceFuncs x_event_funcs =
  {
    x_event_source_prepare,
    x_event_source_check,
    x_event_source_dispatch,
  };

static GSource *
x_event_source_new (FlashbackIdleMonitor *monitor)
{
  GSource *source;
  XEventSource *x_source;

  source = g_source_new (&x_event_funcs, sizeof (XEventSource));

  x_source = (XEventSource *) source;
  x_source->event_poll_fd.fd = ConnectionNumber (monitor->xdisplay);
  x_source->event_poll_fd.events = G_IO_IN;
  x_source->monitor = monitor;

  g_source_add_poll (source, &x_source->event_poll_fd);
  g_source_attach (source, NULL);

  return source;
}

static void
uint64_to_xsync_value (uint64_t    value,
                       XSyncValue *xsync_value)
{
  XSyncIntsToValue (xsync_value, value & 0xffffffff, value >> 32);
}

static XSyncAlarm
_xsync_alarm_set (FlashbackIdleMonitor *self,
                  XSyncTestType         test_type,
                  guint64               interval,
                  gboolean              want_events)
{
  XSyncAlarmAttributes attr;
  XSyncValue delta;
  guint flags;

  flags = XSyncCACounter | XSyncCAValueType | XSyncCATestType |
          XSyncCAValue | XSyncCADelta | XSyncCAEvents;

  XSyncIntToValue (&delta, 0);
  attr.trigger.counter = self->counter;
  attr.trigger.value_type = XSyncAbsolute;
  attr.trigger.test_type = test_type;
  attr.delta = delta;
  attr.events = want_events;

  uint64_to_xsync_value (interval, &attr.trigger.wait_value);

  return XSyncCreateAlarm (self->xdisplay, flags, &attr);
}

static XSyncCounter
find_idletime_counter (FlashbackIdleMonitor *self)
{
  int i;
  int ncounters;
  XSyncSystemCounter *counters;
  XSyncCounter counter = None;

  counters = XSyncListSystemCounters (self->xdisplay, &ncounters);
  for (i = 0; i < ncounters; i++)
    {
      if (g_strcmp0 (counters[i].name, "IDLETIME") == 0)
        {
          counter = counters[i].counter;
          break;
        }
    }

  XSyncFreeSystemCounterList (counters);

  return counter;
}

static void
upower_properties_changed_cb (GDBusProxy           *proxy,
                              GVariant             *changed_properties,
                              GStrv                 invalidated_properties,
                              FlashbackIdleMonitor *self)
{
  gboolean reset_idle_time;
  GVariant *v;

  reset_idle_time = FALSE;

  v = g_variant_lookup_value (changed_properties,
                              "LidIsClosed",
                              G_VARIANT_TYPE_BOOLEAN);

  if (v != NULL)
    {
      gboolean lid_is_closed;

      lid_is_closed = g_variant_get_boolean (v);
      g_variant_unref (v);

      if (lid_is_closed != self->lid_is_closed)
        {
          self->lid_is_closed = lid_is_closed;

          if (!lid_is_closed)
            reset_idle_time = TRUE;
        }
    }

  v = g_variant_lookup_value (changed_properties,
                              "OnBattery",
                              G_VARIANT_TYPE_BOOLEAN);

  if (v != NULL)
    {
      gboolean on_battery;

      on_battery = g_variant_get_boolean (v);
      g_variant_unref (v);

      if (on_battery != self->on_battery)
        {
          self->on_battery = on_battery;
          reset_idle_time = TRUE;
        }
    }

  if (reset_idle_time)
    meta_idle_monitor_reset_idletime (self->monitor);
}

static void
upower_ready_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GError *error;
  GDBusProxy *proxy;
  FlashbackIdleMonitor *self;
  GVariant *v;

  error = NULL;
  proxy = g_dbus_proxy_new_finish (res, &error);

  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to create UPower proxy: %s", error->message);

      g_error_free (error);
      return;
    }

  self = FLASHBACK_IDLE_MONITOR (user_data);
  self->upower_proxy = proxy;

  g_signal_connect (proxy,
                    "g-properties-changed",
                    G_CALLBACK (upower_properties_changed_cb),
                    self);

  v = g_dbus_proxy_get_cached_property (proxy, "LidIsClosed");

  if (v != NULL)
    {
      self->lid_is_closed = g_variant_get_boolean (v);
      g_variant_unref (v);
    }

  v = g_dbus_proxy_get_cached_property (proxy, "OnBattery");

  if (v)
    {
      self->on_battery = g_variant_get_boolean (v);
      g_variant_unref (v);
    }
}

static void
upower_appeared_cb (GDBusConnection *connection,
                    const char      *name,
                    const char      *name_owner,
                    gpointer         user_data)
{
  g_dbus_proxy_new (connection,
                    G_DBUS_PROXY_FLAGS_NONE,
                    NULL,
                    "org.freedesktop.UPower",
                    "/org/freedesktop/UPower",
                    "org.freedesktop.UPower",
                    NULL,
                    upower_ready_cb,
                    user_data);
}

static void
upower_vanished_cb (GDBusConnection *connection,
                    const char      *name,
                    gpointer         user_data)
{
  FlashbackIdleMonitor *self;

  self = FLASHBACK_IDLE_MONITOR (user_data);

  g_clear_object (&self->upower_proxy);
}

static void
flashback_idle_monitor_dispose (GObject *object)
{
  FlashbackIdleMonitor *monitor;
  const gchar *core_path;

  monitor = FLASHBACK_IDLE_MONITOR (object);

  if (monitor->dbus_name_id > 0)
    {
      g_bus_unown_name (monitor->dbus_name_id);
      monitor->dbus_name_id = 0;
    }

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

  if (monitor->user_active_alarm != None)
    {
      XSyncDestroyAlarm (monitor->xdisplay, monitor->user_active_alarm);
      monitor->user_active_alarm = None;
    }

  g_object_unref (monitor->monitor);

  if (monitor->source != NULL)
    {
      g_source_unref (monitor->source);
      monitor->source = NULL;
    }

  if (monitor->xdisplay != NULL)
    {
      XCloseDisplay (monitor->xdisplay);
      monitor->xdisplay = NULL;
    }

  if (monitor->upower_watch_id != 0)
    {
      g_bus_unwatch_name (monitor->upower_watch_id);
      monitor->upower_watch_id = 0;
    }

  g_clear_object (&monitor->upower_proxy);

  G_OBJECT_CLASS (flashback_idle_monitor_parent_class)->finalize (object);
}

static void
flashback_idle_monitor_init (FlashbackIdleMonitor *monitor)
{
  const char *display;
  gint event_base;
  gint error_base;
  gint major;
  gint minor;

  display = g_getenv ("DISPLAY");

  if (display == NULL)
    {
      g_critical ("Unable to open display, DISPLAY not set");
      return;
    }

  monitor->xdisplay = XOpenDisplay (display);

  if (monitor->xdisplay == NULL)
    {
      g_critical ("Unable to open display '%s'", display);
      return;
    }

  monitor->source = x_event_source_new (monitor);

  monitor->monitor = g_object_new (META_TYPE_IDLE_MONITOR,
                                   NULL);

  monitor->dbus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                          "org.gnome.Mutter.IdleMonitor",
                                          G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                          G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                          (GBusAcquiredCallback) on_bus_acquired,
                                          (GBusNameAcquiredCallback) on_name_acquired,
                                          (GBusNameLostCallback) on_name_lost,
                                          monitor, NULL);

  if (!XSyncQueryExtension (monitor->xdisplay, &event_base, &error_base))
    {
      g_critical ("Could not query XSync extension");
      return;
    }

  monitor->xsync_event_base = event_base;
  monitor->xsync_error_base = error_base;

  if (!XSyncInitialize (monitor->xdisplay, &major, &minor))
    {
      g_critical ("Could not initialize XSync");
      return;
    }

  monitor->counter = find_idletime_counter (monitor);
  if (monitor->counter == None)
    {
      g_critical ("IDLETIME counter not found");
      return;
    }

  monitor->user_active_alarm = _xsync_alarm_set (monitor,
                                                 XSyncNegativeTransition,
                                                 1,
                                                 TRUE);

  monitor->upower_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                               "org.freedesktop.UPower",
                                               G_BUS_NAME_WATCHER_FLAGS_NONE,
                                               upower_appeared_cb,
                                               upower_vanished_cb,
                                               monitor,
                                               NULL);
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
