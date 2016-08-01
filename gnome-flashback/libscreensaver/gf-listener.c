/*
 * Copyright (C) 2004-2006 William Jon McCann
 * Copyright (C) 2016 Alberts Muktupāvels
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

#include <time.h>

#include "gf-listener.h"
#include "gf-login-manager-gen.h"
#include "gf-login-session-gen.h"
#include "gf-screensaver-gen.h"

#define SCREENSAVER_DBUS_NAME "org.gnome.ScreenSaver"
#define SCREENSAVER_DBUS_PATH "/org/gnome/ScreenSaver"

#define LOGIN_DBUS_NAME "org.freedesktop.login1"
#define LOGIN_DBUS_PATH "/org/freedesktop/login1"
#define LOGIN_SESSION_DBUS_PATH "/org/freedesktop/login1/session"

struct _GfListener
{
  GObject            parent;

  GfScreensaverGen  *screensaver;
  guint              screensaver_id;

  guint              login_id;
  GfLoginSessionGen *login_session;
  GfLoginManagerGen *login_manager;

  gboolean           enabled;

  gboolean           active;
  time_t             active_start;

  gboolean           idle;
  time_t             idle_start;
};

enum
{
  LOCK,
  SHOW_MESSAGE,
  SIMULATE_USER_ACTIVITY,

  ACTIVE_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfListener, gf_listener, G_TYPE_OBJECT)

static void
set_idle_internal (GfListener *listener,
                   gboolean    idle)
{
  listener->idle = idle;

  if (idle)
    {
      listener->idle_start = time (NULL);
    }
  else
    {
      listener->idle_start = 0;
    }
}

static gboolean
handle_get_active_cb (GfScreensaverGen      *object,
                      GDBusMethodInvocation *invocation,
                      GfListener            *listener)
{
  gf_screensaver_gen_complete_get_active (object, invocation,
                                          listener->active);

  return TRUE;
}

static gboolean
handle_get_active_time_cb (GfScreensaverGen      *object,
                           GDBusMethodInvocation *invocation,
                           GfListener            *listener)
{
  guint seconds;

  if (listener->active)
    {
      time_t now;

      now = time (NULL);

      if (now < listener->active_start)
        {
          g_debug ("Active start time is in the future");
          seconds = 0;
        }
      else if (listener->active_start <= 0)
        {
          g_debug ("Active start time was not set");
          seconds = 0;
        }
      else
        {
          seconds = now - listener->active_start;
        }
    }
  else
    {
      seconds = 0;
    }

  g_debug ("Screensaver active for %u seconds", seconds);
  gf_screensaver_gen_complete_get_active_time (object, invocation, seconds);

  return TRUE;
}

static gboolean
handle_lock_cb (GfScreensaverGen      *object,
                GDBusMethodInvocation *invocation,
                GfListener            *listener)
{
  g_signal_emit (listener, signals[LOCK], 0);
  gf_screensaver_gen_complete_lock (object, invocation);

  return TRUE;
}

static gboolean
handle_set_active_cb (GfScreensaverGen      *object,
                      GDBusMethodInvocation *invocation,
                      gboolean               active,
                      GfListener            *listener)
{
  gf_listener_set_active (listener, active);
  gf_screensaver_gen_complete_set_active (object, invocation);

  return TRUE;
}

static gboolean
handle_show_message_cb (GfScreensaverGen      *object,
                        GDBusMethodInvocation *invocation,
                        const gchar           *summary,
                        const gchar           *body,
                        const gchar           *icon,
                        GfListener            *listener)
{
  if (listener->active)
    {
      g_signal_emit (listener, signals[SHOW_MESSAGE], 0, summary, body, icon);
    }

  gf_screensaver_gen_complete_show_message (object, invocation);

  return TRUE;
}

static gboolean
handle_simulate_user_activity_cb (GfScreensaverGen      *object,
                                  GDBusMethodInvocation *invocation,
                                  GfListener            *listener)
{
  g_signal_emit (listener, signals[SIMULATE_USER_ACTIVITY], 0);
  gf_screensaver_gen_complete_simulate_user_activity (object, invocation);

  return TRUE;
}

static void
bus_acquired_handler (GDBusConnection *connection,
                      const gchar     *name,
                      gpointer         user_data)
{
  GfListener *listener;
  GDBusInterfaceSkeleton *skeleton;
  GError *error;
  gboolean exported;

  listener = GF_LISTENER (user_data);
  skeleton = G_DBUS_INTERFACE_SKELETON (listener->screensaver);

  g_signal_connect (listener->screensaver, "handle-get-active",
                    G_CALLBACK (handle_get_active_cb), listener);
  g_signal_connect (listener->screensaver, "handle-get-active-time",
                    G_CALLBACK (handle_get_active_time_cb), listener);
  g_signal_connect (listener->screensaver, "handle-lock",
                    G_CALLBACK (handle_lock_cb), listener);
  g_signal_connect (listener->screensaver, "handle-set-active",
                    G_CALLBACK (handle_set_active_cb), listener);
  g_signal_connect (listener->screensaver, "handle-show-message",
                    G_CALLBACK (handle_show_message_cb), listener);
  g_signal_connect (listener->screensaver, "handle-simulate-user-activity",
                    G_CALLBACK (handle_simulate_user_activity_cb), listener);

  error = NULL;
  exported = g_dbus_interface_skeleton_export (skeleton, connection,
                                               SCREENSAVER_DBUS_PATH,
                                               &error);

  if (!exported)
    {
      g_warning ("Failed to export interface: %s", error->message);
      g_error_free (error);

      return;
    }
}

static void
name_lost_handler (GDBusConnection *connection,
                   const gchar     *name,
                   gpointer         user_data)
{
  g_warning ("Screensaver already running in this session");
}

static void
lock_cb (GfLoginSessionGen *login_session,
         GfListener        *listener)
{
  g_debug ("Systemd requested session lock");
  g_signal_emit (listener, signals[LOCK], 0);
}

static void
unlock_cb (GfLoginSessionGen *login_session,
           GfListener        *listener)
{
  g_debug ("Systemd requested session unlock");
  gf_listener_set_active (listener, FALSE);
}

static void
notify_active_cb (GfLoginSessionGen *login_session,
                  GParamSpec        *pspec,
                  GfListener        *listener)
{
  if (gf_login_session_gen_get_active (login_session))
    {
      g_signal_emit (listener, signals[SIMULATE_USER_ACTIVITY], 0);
    }
}

static void
login_session_ready_cb (GObject      *object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  GfListener *listener;
  GError *error;

  listener = GF_LISTENER (user_data);

  error = NULL;
  listener->login_session = gf_login_session_gen_proxy_new_for_bus_finish (res, &error);

  if (!listener->login_session)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }

  g_signal_connect (listener->login_session, "lock",
                    G_CALLBACK (lock_cb), listener);
  g_signal_connect (listener->login_session, "unlock",
                    G_CALLBACK (unlock_cb), listener);

  g_signal_connect (listener->login_session, "notify::active",
                    G_CALLBACK (notify_active_cb), listener);
}

static void
prepare_for_sleep_cb (GfLoginManagerGen *login_manager,
                      GfListener        *listener)
{
  g_debug ("A system suspend has been requested");
  g_signal_emit (listener, signals[LOCK], 0);
}

static void
login_manager_ready_cb (GObject      *object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  GfListener *listener;
  GError *error;

  listener = GF_LISTENER (user_data);

  error = NULL;
  listener->login_manager = gf_login_manager_gen_proxy_new_for_bus_finish (res, &error);

  if (!listener->login_manager)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }

  g_signal_connect (listener->login_manager, "prepare-for-sleep",
                    G_CALLBACK (prepare_for_sleep_cb), listener);
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  const gchar *xdg_session_id;
  gchar *path;

  xdg_session_id = g_getenv ("XDG_SESSION_ID");
  if (!xdg_session_id || xdg_session_id[0] == '\0')
    {
      g_warning ("XDG_SESSION_ID environment variable is not set");
      return;
    }

  path = g_strdup_printf ("%s/%s", LOGIN_SESSION_DBUS_PATH, xdg_session_id);

  gf_login_session_gen_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          LOGIN_DBUS_NAME, path,
                                          NULL, login_session_ready_cb,
                                          user_data);

  gf_login_manager_gen_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          LOGIN_DBUS_NAME, LOGIN_DBUS_PATH,
                                          NULL, login_manager_ready_cb,
                                          user_data);

  g_free (path);
}

static void
name_vanished_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  GfListener *listener;

  listener = GF_LISTENER (user_data);

  g_clear_object (&listener->login_session);
  g_clear_object (&listener->login_manager);
}

static void
gf_listener_dispose (GObject *object)
{
  GfListener *listener;
  GDBusInterfaceSkeleton *skeleton;

  listener = GF_LISTENER (object);

  if (listener->screensaver_id)
    {
      g_bus_unown_name (listener->screensaver_id);
      listener->screensaver_id = 0;
    }

  if (listener->login_id)
    {
      g_bus_unwatch_name (listener->login_id);
      listener->login_id = 0;
    }

  if (listener->screensaver)
    {
      skeleton = G_DBUS_INTERFACE_SKELETON (listener->screensaver);

      g_dbus_interface_skeleton_unexport (skeleton);
      g_clear_object (&listener->screensaver);
    }

  g_clear_object (&listener->login_session);
  g_clear_object (&listener->login_manager);

  G_OBJECT_CLASS (gf_listener_parent_class)->dispose (object);
}

static void
install_signals (GObjectClass *object_class)
{
  signals[LOCK] =
    g_signal_new ("lock", GF_TYPE_LISTENER, G_SIGNAL_RUN_LAST, 0, NULL,
                  NULL, NULL, G_TYPE_NONE, 0);

  signals[SHOW_MESSAGE] =
    g_signal_new ("show-message", GF_TYPE_LISTENER, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL, G_TYPE_NONE, 3, G_TYPE_STRING,
                  G_TYPE_STRING, G_TYPE_STRING);

  signals[SIMULATE_USER_ACTIVITY] =
    g_signal_new ("simulate-user-activity", GF_TYPE_LISTENER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals[ACTIVE_CHANGED] =
    g_signal_new ("active-changed", GF_TYPE_LISTENER, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_BOOLEAN, 1, G_TYPE_BOOLEAN);
}

static void
gf_listener_class_init (GfListenerClass *listener_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (listener_class);

  object_class->dispose = gf_listener_dispose;

  install_signals (object_class);
}

static void
gf_listener_init (GfListener *listener)
{
  listener->screensaver = gf_screensaver_gen_skeleton_new ();
  listener->screensaver_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                             SCREENSAVER_DBUS_NAME,
                                             G_BUS_NAME_OWNER_FLAGS_NONE,
                                             bus_acquired_handler, NULL,
                                             name_lost_handler,
                                             listener, NULL);

  if (access("/run/systemd/seats/", F_OK) >= 0)
    {
      listener->login_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                             LOGIN_DBUS_NAME,
                                             G_BUS_NAME_WATCHER_FLAGS_NONE,
                                             name_appeared_handler,
                                             name_vanished_handler,
                                             listener, NULL);
    }
}

GfListener *
gf_listener_new (void)
{
  return g_object_new (GF_TYPE_LISTENER, NULL);
}

void
gf_listener_set_enabled (GfListener *listener,
                         gboolean    enabled)
{
  listener->enabled = enabled;
}

gboolean
gf_listener_get_enabled (GfListener *listener)
{
  return listener->enabled;
}

gboolean
gf_listener_set_active (GfListener *listener,
                        gboolean    active)
{
  gboolean handled;

  if (listener->active == active)
    {
      g_debug ("Trying to set active state when already: %s",
               active ? "active" : "inactive");

      return FALSE;
    }

  g_debug ("Setting active state: %s", active ? "active" : "inactive");

  handled = FALSE;
  g_signal_emit (listener, signals[ACTIVE_CHANGED], 0, active, &handled);

  if (!handled)
    {
      /* if the signal is not handled then we haven't changed state */
      g_debug ("active-changed signal not handled");

      /* clear the idle state */
      if (active)
        {
          set_idle_internal (listener, FALSE);
        }

      return FALSE;
    }

  listener->active = active;

  /* if idle not in sync with active, change it */
  if (listener->idle != active)
    {
      set_idle_internal (listener, active);
    }

  if (active)
    {
      listener->active_start = time (NULL);
    }
  else
    {
      listener->active_start = 0;
    }

  g_debug ("Sending the ActiveChanged(%s) signal on the session bus",
           active ? "TRUE" : "FALSE");

  if (listener->login_session)
    {
      gf_login_session_gen_call_set_locked_hint (listener->login_session,
                                                 active, NULL, NULL, NULL);
    }

  gf_screensaver_gen_emit_active_changed (listener->screensaver, active);

  return TRUE;
}

gboolean
gf_listener_get_active (GfListener *listener)
{
  return listener->enabled;
}

gboolean
gf_listener_set_idle (GfListener *listener,
                      gboolean    idle)
{
  gboolean activated;

  if (listener->idle == idle)
    {
      g_debug ("Trying to set idle state when already: %s",
               idle ? "idle" : "not idle");

      return FALSE;
    }

  g_debug ("Setting idle state: %s", idle ? "idle" : "not idle");

  listener->idle = idle;
  activated = TRUE;

  if (listener->enabled && listener->idle)
    {
      g_debug ("Trying to activate");
      activated = gf_listener_set_active (listener, TRUE);
    }

  /* if activation fails then don't set idle */
  if (activated)
    {
      set_idle_internal (listener, idle);
    }
  else
    {
      g_debug ("Idle activation failed");
      listener->idle = !idle;
    }

  return activated;
}
