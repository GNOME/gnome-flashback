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

#include <gdk/gdkx.h>
#include <string.h>

#include "meta-idle-monitor.h"

struct _MetaIdleMonitor
{
  GObject       parent;

  GHashTable   *watches;

  GHashTable   *alarms;
  Display      *display;
  XSyncCounter  counter;
  XSyncAlarm    user_active_alarm;
};

G_STATIC_ASSERT(sizeof(unsigned long) == sizeof(gpointer));

G_DEFINE_TYPE (MetaIdleMonitor, meta_idle_monitor, G_TYPE_OBJECT)

void
_meta_idle_monitor_watch_fire (MetaIdleMonitorWatch *watch)
{
  MetaIdleMonitor *monitor;
  guint id;
  gboolean is_user_active_watch;

  monitor = watch->monitor;
  g_object_ref (monitor);

  if (watch->idle_source_id)
    {
      g_source_remove (watch->idle_source_id);
      watch->idle_source_id = 0;
    }

  id = watch->id;
  is_user_active_watch = (watch->timeout_msec == 0);

  if (watch->callback)
    watch->callback (monitor, id, watch->user_data);

  if (is_user_active_watch)
    meta_idle_monitor_remove_watch (monitor, id);

  g_object_unref (monitor);
}

static gint64
_xsyncvalue_to_int64 (XSyncValue value)
{
  return ((guint64) XSyncValueHigh32 (value)) << 32 |
         (guint64) XSyncValueLow32 (value);
}

static void
free_watch (gpointer data)
{
  MetaIdleMonitorWatch *watch = data;
  MetaIdleMonitor *monitor = watch->monitor;

  g_object_ref (monitor);

  if (watch->idle_source_id)
    {
      g_source_remove (watch->idle_source_id);
      watch->idle_source_id = 0;
    }

  if (watch->notify != NULL)
    watch->notify (watch->user_data);

  if (watch->xalarm != monitor->user_active_alarm &&
      watch->xalarm != None)
    {
      XSyncDestroyAlarm (monitor->display, watch->xalarm);
      g_hash_table_remove (monitor->alarms, (gpointer) watch->xalarm);
    }

  g_object_unref (monitor);
  g_free (watch);
}

#define GUINT64_TO_XSYNCVALUE(value, ret) XSyncIntsToValue (ret, (value) & 0xFFFFFFFF, ((guint64)(value)) >> 32)

static XSyncAlarm
_xsync_alarm_set (MetaIdleMonitor *self,
                  XSyncTestType    test_type,
                  guint64          interval,
                  gboolean         want_events)
{
  XSyncAlarmAttributes attr;
  XSyncValue delta;
  guint flags;

  flags = XSyncCACounter | XSyncCAValueType | XSyncCATestType |
          XSyncCAValue | XSyncCADelta | XSyncCAEvents;

  XSyncIntToValue (&delta, 0);
  attr.trigger.counter = self->counter;
  attr.trigger.value_type = XSyncAbsolute;
  attr.delta = delta;
  attr.events = want_events;

  GUINT64_TO_XSYNCVALUE (interval, &attr.trigger.wait_value);
  attr.trigger.test_type = test_type;
  return XSyncCreateAlarm (self->display, flags, &attr);
}

static XSyncCounter
find_idletime_counter (MetaIdleMonitor *self)
{
  int i;
  int ncounters;
  XSyncSystemCounter *counters;
  XSyncCounter counter = None;

  counters = XSyncListSystemCounters (self->display, &ncounters);
  for (i = 0; i < ncounters; i++)
    {
      if (counters[i].name != NULL && strcmp (counters[i].name, "IDLETIME") == 0)
        {
          counter = counters[i].counter;
          break;
        }
    }
  XSyncFreeSystemCounterList (counters);

  return counter;
}

static void
init_xsync (MetaIdleMonitor *self)
{
  self->counter = find_idletime_counter (self);

  /* IDLETIME counter not found? */
  if (self->counter == None)
    {
      g_warning ("IDLETIME counter not found\n");
      return;
    }

  self->user_active_alarm = _xsync_alarm_set (self, XSyncNegativeTransition, 1, FALSE);
}

static void
meta_idle_monitor_constructed (GObject *object)
{
  MetaIdleMonitor *self;
  GdkDisplay *display;

  self = META_IDLE_MONITOR (object);

  G_OBJECT_CLASS (meta_idle_monitor_parent_class)->constructed (object);

  display = gdk_display_get_default ();
  self->display = gdk_x11_display_get_xdisplay (display);
  init_xsync (self);
}

static void
meta_idle_monitor_dispose (GObject *object)
{
  MetaIdleMonitor *monitor = META_IDLE_MONITOR (object);

  g_clear_pointer (&monitor->watches, g_hash_table_destroy);

  if (monitor->user_active_alarm != None)
    {
      XSyncDestroyAlarm (monitor->display, monitor->user_active_alarm);
      monitor->user_active_alarm = None;
    }

  g_clear_pointer (&monitor->alarms, g_hash_table_destroy);

  G_OBJECT_CLASS (meta_idle_monitor_parent_class)->dispose (object);
}

static void
meta_idle_monitor_class_init (MetaIdleMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = meta_idle_monitor_constructed;
  object_class->dispose = meta_idle_monitor_dispose;
}

static void
meta_idle_monitor_init (MetaIdleMonitor *monitor)
{
  monitor->watches = g_hash_table_new_full (NULL, NULL, NULL, free_watch);
  monitor->alarms = g_hash_table_new (NULL, NULL);
}

static void
check_x11_watches (MetaIdleMonitor *self,
                   XSyncAlarm       alarm)
{
  GList *node, *watch_ids;

  /* we get the keys and do explicit look ups in case
   * an early iteration of the loop ends up leading
   * to watches from later iterations getting invalidated
   */
  watch_ids = g_hash_table_get_keys (self->watches);

  for (node = watch_ids; node != NULL; node = node->next)
    {
      guint watch_id = GPOINTER_TO_UINT (node->data);
      MetaIdleMonitorWatch *watch;

      watch = g_hash_table_lookup (self->watches, GUINT_TO_POINTER (watch_id));

      if (watch && watch->xalarm == alarm)
        _meta_idle_monitor_watch_fire (watch);
    }

  g_list_free (watch_ids);
}

static void
ensure_alarm_rescheduled (Display    *dpy,
                          XSyncAlarm  alarm)
{
  XSyncAlarmAttributes attr;

  /* Some versions of Xorg have an issue where alarms aren't
   * always rescheduled. Calling XSyncChangeAlarm, even
   * without any attributes, will reschedule the alarm. */
  XSyncChangeAlarm (dpy, alarm, 0, &attr);
}

static void
set_alarm_enabled (Display    *dpy,
                   XSyncAlarm  alarm,
                   gboolean    enabled)
{
  XSyncAlarmAttributes attr;
  attr.events = enabled;
  XSyncChangeAlarm (dpy, alarm, XSyncCAEvents, &attr);
}

void
meta_idle_monitor_handle_xevent (MetaIdleMonitor       *self,
                                 XSyncAlarmNotifyEvent *alarm_event)
{
  XSyncAlarm alarm;
  gboolean has_alarm;

  if (alarm_event->state != XSyncAlarmActive)
    return;

  alarm = alarm_event->alarm;

  has_alarm = FALSE;

  if (alarm == self->user_active_alarm)
    {
      set_alarm_enabled (self->display, alarm, FALSE);
      has_alarm = TRUE;
    }
  else if (g_hash_table_contains (self->alarms, (gpointer) alarm))
    {
      ensure_alarm_rescheduled (self->display, alarm);
      has_alarm = TRUE;
    }

  if (has_alarm)
    check_x11_watches (self, alarm);
}

static guint32
get_next_watch_serial (void)
{
  static guint32 serial = 0;
  g_atomic_int_inc (&serial);
  return serial;
}

static gboolean
fire_watch_idle (gpointer data)
{
  MetaIdleMonitorWatch *watch = data;

  watch->idle_source_id = 0;
  _meta_idle_monitor_watch_fire (watch);

  return FALSE;
}

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

  if (monitor->user_active_alarm != None)
    {
      if (timeout_msec != 0)
        {
          watch->xalarm = _xsync_alarm_set (monitor, XSyncPositiveTransition, timeout_msec, TRUE);

          g_hash_table_add (monitor->alarms, (gpointer) watch->xalarm);

          if (meta_idle_monitor_get_idletime (monitor) > (gint64) timeout_msec)
            {
              watch->idle_source_id = g_idle_add (fire_watch_idle, watch);
              g_source_set_name_by_id (watch->idle_source_id, "[mutter] fire_watch_idle");
            }
        }
      else
        {
          watch->xalarm = monitor->user_active_alarm;

          set_alarm_enabled (monitor->display, monitor->user_active_alarm, TRUE);
        }
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
  XSyncValue value;

  if (!XSyncQueryCounter (monitor->display, monitor->counter, &value))
    return -1;

  return _xsyncvalue_to_int64 (value);
}
