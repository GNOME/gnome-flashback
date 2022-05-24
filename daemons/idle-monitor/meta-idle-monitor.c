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

/**
 * SECTION:idle-monitor
 * @title: MetaIdleMonitor
 * @short_description: Mutter idle counter (similar to X's IDLETIME)
 */

#include "config.h"

#include <string.h>

#include "dbus/gf-session-manager-gen.h"
#include "meta-idle-monitor.h"

#define GSM_INHIBITOR_FLAG_IDLE 1 << 3

struct _MetaIdleMonitor
{
  GObject              parent;

  GHashTable          *watches;

  guint64              last_event_time;

  GfSessionManagerGen *session_manager;
  gboolean             inhibited;
};

typedef struct
{
  MetaIdleMonitor          *monitor;
  guint                     id;
  MetaIdleMonitorWatchFunc  callback;
  gpointer                  user_data;
  GDestroyNotify            notify;
  guint64                   timeout_msec;
  GSource                  *timeout_source;
} MetaIdleMonitorWatch;

G_STATIC_ASSERT(sizeof(unsigned long) == sizeof(gpointer));

G_DEFINE_TYPE (MetaIdleMonitor, meta_idle_monitor, G_TYPE_OBJECT)

static void
update_inhibited_watch (gpointer key,
                        gpointer value,
                        gpointer user_data)
{
  MetaIdleMonitor *self;
  MetaIdleMonitorWatch *watch;

  self = user_data;
  watch = value;

  if (watch->timeout_source == NULL)
    return;

  if (self->inhibited)
    {
      g_source_set_ready_time (watch->timeout_source, -1);
    }
  else
    {
      g_source_set_ready_time (watch->timeout_source,
                               self->last_event_time +
                               watch->timeout_msec * 1000);
    }
}

static void
inhibited_actions_changed_cb (GfSessionManagerGen *session_manager,
                              GParamSpec          *pspec,
                              MetaIdleMonitor     *self)
{
  guint actions;

  actions = gf_session_manager_gen_get_inhibited_actions (session_manager);

  if ((actions & GSM_INHIBITOR_FLAG_IDLE) != GSM_INHIBITOR_FLAG_IDLE)
    {
      self->last_event_time = g_get_monotonic_time ();
      self->inhibited = FALSE;
    }
  else
    {
      self->inhibited = TRUE;
    }

  g_hash_table_foreach (self->watches,
                        update_inhibited_watch,
                        self);
}

static void
session_manager_ready_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  GError *error;
  GfSessionManagerGen *session_manager;
  MetaIdleMonitor *self;

  error = NULL;
  session_manager = gf_session_manager_gen_proxy_new_for_bus_finish (res,
                                                                     &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to get session manager proxy: %s", error->message);

      g_error_free (error);
      return;
    }

  self = META_IDLE_MONITOR (user_data);
  self->session_manager = session_manager;

  g_signal_connect (self->session_manager,
                    "notify::inhibited-actions",
                    G_CALLBACK (inhibited_actions_changed_cb),
                    self);

  inhibited_actions_changed_cb (self->session_manager, NULL, self);
}

static void
meta_idle_monitor_watch_fire (MetaIdleMonitorWatch *watch)
{
  MetaIdleMonitor *monitor;
  guint id;
  gboolean is_user_active_watch;

  monitor = watch->monitor;
  g_object_ref (monitor);

  id = watch->id;
  is_user_active_watch = (watch->timeout_msec == 0);

  if (watch->callback)
    watch->callback (monitor, id, watch->user_data);

  if (is_user_active_watch)
    meta_idle_monitor_remove_watch (monitor, id);

  g_object_unref (monitor);
}

static void
free_watch (gpointer data)
{
  MetaIdleMonitorWatch *watch = data;
  MetaIdleMonitor *monitor = watch->monitor;

  g_object_ref (monitor);

  if (watch->notify != NULL)
    watch->notify (watch->user_data);

  if (watch->timeout_source != NULL)
    g_source_destroy (watch->timeout_source);

  g_object_unref (monitor);
  g_free (watch);
}

static void
meta_idle_monitor_dispose (GObject *object)
{
  MetaIdleMonitor *monitor = META_IDLE_MONITOR (object);

  g_clear_pointer (&monitor->watches, g_hash_table_destroy);
  g_clear_object (&monitor->session_manager);

  G_OBJECT_CLASS (meta_idle_monitor_parent_class)->dispose (object);
}

static void
meta_idle_monitor_class_init (MetaIdleMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = meta_idle_monitor_dispose;
}

static void
meta_idle_monitor_init (MetaIdleMonitor *monitor)
{
  monitor->watches = g_hash_table_new_full (NULL, NULL, NULL, free_watch);
  monitor->last_event_time = g_get_monotonic_time ();

  gf_session_manager_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                            G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
                                            G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                            "org.gnome.SessionManager",
                                            "/org/gnome/SessionManager",
                                            NULL,
                                            session_manager_ready_cb,
                                            monitor);
}

static guint32
get_next_watch_serial (void)
{
  static guint32 serial = 0;
  g_atomic_int_inc (&serial);
  return serial;
}

static gboolean
idle_monitor_dispatch_timeout (GSource     *source,
                               GSourceFunc  callback,
                               gpointer     user_data)
{
  MetaIdleMonitorWatch *watch = (MetaIdleMonitorWatch *) user_data;
  int64_t now;
  int64_t ready_time;

  now = g_source_get_time (source);
  ready_time = g_source_get_ready_time (source);
  if (ready_time > now)
    return G_SOURCE_CONTINUE;

  g_source_set_ready_time (watch->timeout_source, -1);
  meta_idle_monitor_watch_fire (watch);

  return G_SOURCE_CONTINUE;
}

static GSourceFuncs idle_monitor_source_funcs =
  {
    .prepare = NULL,
    .check = NULL,
    .dispatch = idle_monitor_dispatch_timeout,
    .finalize = NULL,
  };

static MetaIdleMonitorWatch *
make_watch (MetaIdleMonitor           *monitor,
            guint64                    timeout_msec,
            MetaIdleMonitorWatchFunc   callback,
            gpointer                   user_data,
            GDestroyNotify             notify)
{
  MetaIdleMonitorWatch *watch;

  watch = g_new0 (MetaIdleMonitorWatch, 1);

  watch->monitor = monitor;
  watch->id = get_next_watch_serial ();
  watch->callback = callback;
  watch->user_data = user_data;
  watch->notify = notify;
  watch->timeout_msec = timeout_msec;

  if (timeout_msec != 0)
    {
      GSource *source;

      source = g_source_new (&idle_monitor_source_funcs, sizeof (GSource));

      g_source_set_callback (source, NULL, watch, NULL);

      if (!monitor->inhibited)
        {
          g_source_set_ready_time (source,
                                   monitor->last_event_time +
                                   timeout_msec * 1000);
        }

      g_source_attach (source, NULL);
      g_source_unref (source);

      watch->timeout_source = source;
    }

  g_hash_table_insert (monitor->watches,
                       GUINT_TO_POINTER (watch->id),
                       watch);
  return watch;
}

/**
 * meta_idle_monitor_add_idle_watch:
 * @monitor: A #MetaIdleMonitor
 * @interval_msec: The idletime interval, in milliseconds
 * @callback: (nullable): The callback to call when the user has
 *     accumulated @interval_msec milliseconds of idle time.
 * @user_data: (nullable): The user data to pass to the callback
 * @notify: A #GDestroyNotify
 *
 * Returns: a watch id
 *
 * Adds a watch for a specific idle time. The callback will be called
 * when the user has accumulated @interval_msec milliseconds of idle time.
 * This function will return an ID that can either be passed to
 * meta_idle_monitor_remove_watch(), or can be used to tell idle time
 * watches apart if you have more than one.
 *
 * Also note that this function will only care about positive transitions
 * (user's idle time exceeding a certain time). If you want to know about
 * when the user has become active, use
 * meta_idle_monitor_add_user_active_watch().
 */
guint
meta_idle_monitor_add_idle_watch (MetaIdleMonitor	       *monitor,
                                  guint64	                interval_msec,
                                  MetaIdleMonitorWatchFunc      callback,
                                  gpointer			user_data,
                                  GDestroyNotify		notify)
{
  MetaIdleMonitorWatch *watch;

  g_return_val_if_fail (META_IS_IDLE_MONITOR (monitor), 0);
  g_return_val_if_fail (interval_msec > 0, 0);

  watch = make_watch (monitor,
                      interval_msec,
                      callback,
                      user_data,
                      notify);

  return watch->id;
}

/**
 * meta_idle_monitor_add_user_active_watch:
 * @monitor: A #MetaIdleMonitor
 * @callback: (nullable): The callback to call when the user is
 *     active again.
 * @user_data: (nullable): The user data to pass to the callback
 * @notify: A #GDestroyNotify
 *
 * Returns: a watch id
 *
 * Add a one-time watch to know when the user is active again.
 * Note that this watch is one-time and will de-activate after the
 * function is called, for efficiency purposes. It's most convenient
 * to call this when an idle watch, as added by
 * meta_idle_monitor_add_idle_watch(), has triggered.
 */
guint
meta_idle_monitor_add_user_active_watch (MetaIdleMonitor          *monitor,
                                         MetaIdleMonitorWatchFunc  callback,
                                         gpointer		   user_data,
                                         GDestroyNotify	           notify)
{
  MetaIdleMonitorWatch *watch;

  g_return_val_if_fail (META_IS_IDLE_MONITOR (monitor), 0);

  watch = make_watch (monitor,
                      0,
                      callback,
                      user_data,
                      notify);

  return watch->id;
}

/**
 * meta_idle_monitor_remove_watch:
 * @monitor: A #MetaIdleMonitor
 * @id: A watch ID
 *
 * Removes an idle time watcher, previously added by
 * meta_idle_monitor_add_idle_watch() or
 * meta_idle_monitor_add_user_active_watch().
 */
void
meta_idle_monitor_remove_watch (MetaIdleMonitor *monitor,
                                guint	         id)
{
  g_return_if_fail (META_IS_IDLE_MONITOR (monitor));

  g_object_ref (monitor);
  g_hash_table_remove (monitor->watches,
                       GUINT_TO_POINTER (id));
  g_object_unref (monitor);
}

/**
 * meta_idle_monitor_get_idletime:
 * @monitor: A #MetaIdleMonitor
 *
 * Returns: The current idle time, in milliseconds, or -1 for not supported
 */
gint64
meta_idle_monitor_get_idletime (MetaIdleMonitor *monitor)
{
  return (g_get_monotonic_time () - monitor->last_event_time) / 1000;
}

void
meta_idle_monitor_reset_idletime (MetaIdleMonitor *self)
{
  GList *node, *watch_ids;

  self->last_event_time = g_get_monotonic_time ();

  watch_ids = g_hash_table_get_keys (self->watches);

  for (node = watch_ids; node != NULL; node = node->next)
    {
      guint watch_id = GPOINTER_TO_UINT (node->data);
      MetaIdleMonitorWatch *watch;

      watch = g_hash_table_lookup (self->watches,
                                   GUINT_TO_POINTER (watch_id));
      if (!watch)
        continue;

      if (watch->timeout_msec == 0)
        {
          meta_idle_monitor_watch_fire (watch);
        }
      else
        {
          if (self->inhibited)
            {
              g_source_set_ready_time (watch->timeout_source, -1);
            }
          else
            {
              g_source_set_ready_time (watch->timeout_source,
                                       self->last_event_time +
                                       watch->timeout_msec * 1000);
            }
        }
    }

  g_list_free (watch_ids);
}
