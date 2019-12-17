/*
 * Copyright (C) 2004-2006 William Jon McCann
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2019 Alberts Muktupāvels
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
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     William Jon McCann <mccann@jhu.edu>
 */

#include "config.h"

#include <gdk/gdkx.h>

#include "dbus/gf-sm-presence-gen.h"
#include "gf-watcher.h"

#define SESSION_MANAGER_DBUS_NAME "org.gnome.SessionManager"
#define SESSION_MANAGER_PRESENCE_DBUS_PATH "/org/gnome/SessionManager/Presence"

struct _GfWatcher
{
  GObject          parent;

  guint            presence_id;
  GfSmPresenceGen *presence;

  gboolean         active;

  gboolean         idle;
  gboolean         idle_notice;
  guint            idle_id;

  guint            watchdog_id;
};

enum
{
  IDLE_CHANGED,
  IDLE_NOTICE_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfWatcher, gf_watcher, G_TYPE_OBJECT)

/* Figuring out what the appropriate XSetScreenSaver() parameters are
 * (one wouldn't expect this to be rocket science.)
 */
static void
disable_builtin_screensaver (GfWatcher *watcher,
                             gboolean   unblank_screen)
{
  GdkDisplay *display;
  Display *xdisplay;
  gint current_timeout;
  gint current_interval;
  gint prefer_blanking;
  gint current_allow_exposures;
  gint desired_timeout;
  gint desired_interval;
  gint desired_allow_exposures;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  XGetScreenSaver (xdisplay, &current_timeout, &current_interval,
                   &prefer_blanking, &current_allow_exposures);

  /* When we're not using an extension, set the server-side timeout to 0,
   * so that the server never gets involved with screen blanking, and we
   * do it all ourselves.  (However, when we *are* using an extension, we
   * tell the server when to notify us, and rather than blanking the screen,
   * the server will send us an X event telling us to blank.)
   */
  desired_timeout = 0;

  desired_interval = 0;

  /* I suspect (but am not sure) that DontAllowExposures might have
   * something to do with powering off the monitor as well, at least
   * on some systems that don't support XDPMS? Who know...
   */
  desired_allow_exposures = AllowExposures;

  if (desired_timeout != current_timeout ||
      desired_interval != current_interval ||
      desired_allow_exposures != current_allow_exposures)
    {
      g_debug ("Disabling server builtin screensaver:"
               " (xset s %d %d; xset s %s; xset s %s)",
               desired_timeout, desired_interval,
               (prefer_blanking ? "blank" : "noblank"),
               (desired_allow_exposures ? "expose" : "noexpose"));

      XSetScreenSaver (xdisplay, desired_timeout, desired_interval,
                       prefer_blanking, desired_allow_exposures);

      XSync (xdisplay, FALSE);
    }

  if (unblank_screen)
    {
      /* Turn off the server builtin saver if it is now running. */
      XForceScreenSaver (xdisplay, ScreenSaverReset);
    }
}

/* Calls disable_builtin_screensaver() so that if xset has been used, or
 * some other program (like xlock) has messed with the XSetScreenSaver()
 * settings, they will be set back to sensible values (if a server
 * extension is in use, messing with xlock can cause the screensaver to
 * never get a wakeup event, and could cause monitor power-saving to occur,
 * and all manner of heinousness.)
 */
static gboolean
watchdog_cb (gpointer user_data)
{
  GfWatcher *watcher;

  watcher = GF_WATCHER (user_data);

  disable_builtin_screensaver (watcher, FALSE);

  return G_SOURCE_CONTINUE;
}

static gboolean
set_session_idle (GfWatcher *watcher,
                  gboolean   idle)
{
  gboolean handled;

  if (watcher->idle == idle)
    return FALSE;

  handled = FALSE;
  g_signal_emit (watcher, signals[IDLE_CHANGED], 0, idle, &handled);

  if (handled)
    {
      g_debug ("Changing idle state: %d", idle);
      watcher->idle = idle;
    }
  else
    {
      g_debug ("Signal idle-changed was not handled: %d", idle);
    }

  return handled;
}

static void
set_session_idle_notice (GfWatcher *watcher,
                         gboolean   idle_notice)
{
  gboolean handled;

  if (watcher->idle_notice == idle_notice)
    return;

  handled = FALSE;
  g_signal_emit (watcher, signals[IDLE_NOTICE_CHANGED], 0,
                 idle_notice, &handled);

  if (handled)
    {
      g_debug ("Changing idle notice state: %d", idle_notice);
      watcher->idle_notice = idle_notice;
    }
  else
    {
      g_debug ("Signal idle-notice-changed was not handled: %d", idle_notice);
    }
}

static gboolean
idle_cb (gpointer user_data)
{
  GfWatcher *watcher;
  gboolean res;

  watcher = GF_WATCHER (user_data);
  res = set_session_idle (watcher, TRUE);

  set_session_idle_notice (watcher, FALSE);

  if (res)
    {
      watcher->idle_id = 0;
      return G_SOURCE_REMOVE;
    }

  /* try again if we failed i guess */
  return G_SOURCE_CONTINUE;
}

static void
status_changed_cb (GfSmPresenceGen *presence,
                   guint            status,
                   GfWatcher       *watcher)
{
  gboolean idle;

  if (!watcher->active)
    {
      g_debug ("Watcher not active, ignoring status changes");
      return;
    }

  idle = (status == 3);

  if (!idle && !watcher->idle_notice)
    {
      /* no change in idleness */
      return;
    }

  if (watcher->idle_id > 0)
    {
      g_source_remove (watcher->idle_id);
      watcher->idle_id = 0;
    }

  if (idle)
    {
      set_session_idle_notice (watcher, idle);

      /* time before idle signal to send notice signal */
      watcher->idle_id = g_timeout_add_seconds (10, idle_cb, watcher);
      g_source_set_name_by_id (watcher->idle_id, "[gnome-flashback] idle_cb");
    }
  else
    {
      set_session_idle (watcher, FALSE);
      set_session_idle_notice (watcher, FALSE);
    }
}

static void
presence_ready_cb (GObject      *object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  GfWatcher *watcher;
  GError *error;
  guint status;

  watcher = GF_WATCHER (user_data);

  error = NULL;
  watcher->presence = gf_sm_presence_gen_proxy_new_for_bus_finish (res, &error);

  if (!watcher->presence)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }

  g_signal_connect (watcher->presence, "status-changed",
                    G_CALLBACK (status_changed_cb), watcher);

  status = gf_sm_presence_gen_get_status (watcher->presence);
  status_changed_cb (watcher->presence, status, watcher);
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  gf_sm_presence_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        SESSION_MANAGER_DBUS_NAME,
                                        SESSION_MANAGER_PRESENCE_DBUS_PATH,
                                        NULL, presence_ready_cb,
                                        user_data);

}

static void
name_vanished_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  GfWatcher *watcher;

  watcher = GF_WATCHER (user_data);

  g_clear_object (&watcher->presence);
}

static void
set_active_internal (GfWatcher *watcher,
                     gboolean   active)
{
  if (watcher->active == active)
    return;

  watcher->idle = FALSE;
  watcher->idle_notice = FALSE;
  watcher->active = active;
}

static void
gf_watcher_dispose (GObject *object)
{
  GfWatcher *watcher;

  watcher = GF_WATCHER (object);

  if (watcher->presence_id)
    {
      g_bus_unwatch_name (watcher->presence_id);
      watcher->presence_id = 0;
    }

  if (watcher->idle_id > 0)
    {
      g_source_remove (watcher->idle_id);
      watcher->idle_id = 0;
    }

  if (watcher->watchdog_id > 0)
    {
      g_source_remove (watcher->watchdog_id);
      watcher->watchdog_id = 0;
    }

  g_clear_object (&watcher->presence);

  G_OBJECT_CLASS (gf_watcher_parent_class)->dispose (object);
}

static void
install_signals (GObjectClass *object_class)
{
  signals[IDLE_CHANGED] =
    g_signal_new ("idle-changed", GF_TYPE_WATCHER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_BOOLEAN, 1, G_TYPE_BOOLEAN);

  signals[IDLE_NOTICE_CHANGED] =
    g_signal_new ("idle-notice-changed", GF_TYPE_WATCHER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_BOOLEAN, 1, G_TYPE_BOOLEAN);
}

static void
gf_watcher_class_init (GfWatcherClass *watcher_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (watcher_class);

  object_class->dispose = gf_watcher_dispose;

  install_signals (object_class);
}

static void
gf_watcher_init (GfWatcher *watcher)
{
  watcher->presence_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                           SESSION_MANAGER_DBUS_NAME,
                                           G_BUS_NAME_WATCHER_FLAGS_NONE,
                                           name_appeared_handler,
                                           name_vanished_handler,
                                           watcher, NULL);

  /* This timer goes off every few minutes, whether the user is idle or
   * not, to try and clean up anything that has gone wrong.
   */
  watcher->watchdog_id = g_timeout_add_seconds (600, watchdog_cb, watcher);
  g_source_set_name_by_id (watcher->watchdog_id, "[gnome-flashback] watchdog_cb");
}

GfWatcher *
gf_watcher_new (void)
{
  return g_object_new (GF_TYPE_WATCHER, NULL);
}

gboolean
gf_watcher_set_active (GfWatcher *watcher,
                       gboolean   active)
{
  g_debug ("Turning watcher: %s", active ? "ON" : "OFF");

  if (watcher->active == active)
    {
      g_debug ("Idle detection is already %s",
               active ? "active" : "inactive");

      return FALSE;
    }

  set_active_internal (watcher, active);

  return TRUE;
}

gboolean
gf_watcher_get_active (GfWatcher *watcher)
{
  return watcher->active;
}
