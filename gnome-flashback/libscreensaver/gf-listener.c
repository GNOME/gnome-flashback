/*
 * Copyright (C) 2004-2006 William Jon McCann
 * Copyright (C) 2016-2019 Alberts Muktupāvels
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
#include "gf-listener.h"

#include <gio/gunixfdlist.h>
#include <time.h>

#include "dbus/gf-login-manager-gen.h"
#include "dbus/gf-login-session-gen.h"
#include "dbus/gf-screensaver-gen.h"
#include "gf-screensaver-utils.h"

#define SCREENSAVER_DBUS_NAME "org.gnome.ScreenSaver"
#define SCREENSAVER_DBUS_PATH "/org/gnome/ScreenSaver"

#define LOGIN_DBUS_NAME "org.freedesktop.login1"
#define LOGIN_DBUS_PATH "/org/freedesktop/login1"

struct _GfListener
{
  GObject            parent;

  GfScreensaverGen  *screensaver;
  guint              screensaver_id;

  guint              login_id;
  GfLoginSessionGen *login_session;
  GfLoginManagerGen *login_manager;

  gboolean           active;
  time_t             active_start;

  gboolean           session_idle;
  time_t             session_idle_start;

  int                inhibit_lock_fd;
};

enum
{
  LOCK,
  SHOW_MESSAGE,
  SIMULATE_USER_ACTIVITY,

  ACTIVE_CHANGED,

  PREPARE_FOR_SLEEP,

  LAST_SIGNAL
};

static guint listener_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfListener, gf_listener, G_TYPE_OBJECT)

static void
release_inhibit_lock (GfListener *self)
{
  if (self->inhibit_lock_fd < 0)
    return;

  g_debug ("Releasing systemd inhibit lock");

  close (self->inhibit_lock_fd);
  self->inhibit_lock_fd = -1;
}

static void
inhibit_cb (GObject      *object,
            GAsyncResult *res,
            gpointer      user_data)
{
  GVariant *pipe_fd;
  GUnixFDList *fd_list;
  GError *error;
  GfListener *self;
  int index;

  error = NULL;
  pipe_fd = NULL;

  gf_login_manager_gen_call_inhibit_finish (GF_LOGIN_MANAGER_GEN (object),
                                            &pipe_fd, &fd_list, res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_LISTENER (user_data);

  index = g_variant_get_handle (pipe_fd);
  g_variant_unref (pipe_fd);

  self->inhibit_lock_fd = g_unix_fd_list_get (fd_list, index, &error);
  g_object_unref (fd_list);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  g_debug ("Inhibit lock fd: %d", self->inhibit_lock_fd);
}

static void
take_inhibit_lock (GfListener *self)
{
  if (self->inhibit_lock_fd >= 0)
    return;

  g_debug ("Taking systemd inhibit lock");
  gf_login_manager_gen_call_inhibit (self->login_manager,
                                     "sleep",
                                     "GNOME Flashback",
                                     "GNOME Flashback needs to lock the screen",
                                     "delay",
                                     NULL,
                                     NULL,
                                     inhibit_cb,
                                     self);
}

static void
set_session_idle_internal (GfListener *listener,
                           gboolean    session_idle)
{
  listener->session_idle = session_idle;

  if (session_idle)
    listener->session_idle_start = time (NULL);
  else
    listener->session_idle_start = 0;
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
  g_signal_emit (listener, listener_signals[LOCK], 0);
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
    g_signal_emit (listener, listener_signals[SHOW_MESSAGE], 0,
                   summary, body, icon);

  gf_screensaver_gen_complete_show_message (object, invocation);

  return TRUE;
}

static gboolean
handle_simulate_user_activity_cb (GfScreensaverGen      *object,
                                  GDBusMethodInvocation *invocation,
                                  GfListener            *listener)
{
  g_signal_emit (listener, listener_signals[SIMULATE_USER_ACTIVITY], 0);
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
         GfListener        *self)
{
  g_debug ("systemd requested session lock");
  g_signal_emit (self, listener_signals[LOCK], 0);
}

static void
unlock_cb (GfLoginSessionGen *login_session,
           GfListener        *self)
{
  g_debug ("systemd requested session unlock");
  gf_listener_set_active (self, FALSE);
}

static void
notify_active_cb (GfLoginSessionGen *login_session,
                  GParamSpec        *pspec,
                  GfListener        *self)
{
  if (gf_login_session_gen_get_active (login_session))
    g_signal_emit (self, listener_signals[SIMULATE_USER_ACTIVITY], 0);
}

static void
login_session_ready_cb (GObject      *object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  GError *error;
  GfLoginSessionGen *login_session;
  GfListener *self;

  error = NULL;
  login_session = gf_login_session_gen_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_LISTENER (user_data);
  self->login_session = login_session;

  g_signal_connect (self->login_session, "lock",
                    G_CALLBACK (lock_cb), self);
  g_signal_connect (self->login_session, "unlock",
                    G_CALLBACK (unlock_cb), self);

  g_signal_connect (self->login_session, "notify::active",
                    G_CALLBACK (notify_active_cb), self);

  if (gf_login_session_gen_get_locked_hint (self->login_session))
    {
      g_debug ("systemd LockedHint=True");
      g_signal_emit (self, listener_signals[LOCK], 0);
    }
}

static void
get_session_cb (GObject      *object,
                GAsyncResult *res,
                gpointer      user_data)
{
  char *object_path;
  GError *error;

  object_path = NULL;
  error = NULL;

  gf_login_manager_gen_call_get_session_finish (GF_LOGIN_MANAGER_GEN (object),
                                                &object_path,
                                                res,
                                                &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  gf_login_session_gen_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          LOGIN_DBUS_NAME,
                                          object_path,
                                          NULL,
                                          login_session_ready_cb,
                                          user_data);

  g_free (object_path);
}

static void
prepare_for_sleep_cb (GfLoginManagerGen *login_manager,
                      gboolean           start,
                      GfListener        *self)
{
  if (start)
    {
      g_debug ("A system suspend has been requested");
      g_signal_emit (self, listener_signals[PREPARE_FOR_SLEEP], 0);
      release_inhibit_lock (self);
    }
  else
    {
      take_inhibit_lock (self);
    }
}

static void
login_manager_ready_cb (GObject      *object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  GError *error;
  GfLoginManagerGen *login_manager;
  GfListener *self;
  char *session_id;

  error = NULL;
  login_manager = gf_login_manager_gen_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_LISTENER (user_data);
  self->login_manager = login_manager;

  take_inhibit_lock (self);

  g_signal_connect (self->login_manager, "prepare-for-sleep",
                    G_CALLBACK (prepare_for_sleep_cb), self);

  session_id = NULL;
  if (gf_find_systemd_session (&session_id))
    {
      g_debug ("Session id: %s", session_id);

      gf_login_manager_gen_call_get_session (self->login_manager,
                                             session_id,
                                             NULL,
                                             get_session_cb,
                                             self);

      g_free (session_id);
    }
  else
    {
      g_debug ("Couldn't determine our own session id");
    }
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  gf_login_manager_gen_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          LOGIN_DBUS_NAME, LOGIN_DBUS_PATH,
                                          NULL, login_manager_ready_cb,
                                          user_data);
}

static void
name_vanished_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  GfListener *self;

  self = GF_LISTENER (user_data);

  release_inhibit_lock (self);

  g_clear_object (&self->login_session);
  g_clear_object (&self->login_manager);
}

static void
gf_listener_dispose (GObject *object)
{
  GfListener *self;
  GDBusInterfaceSkeleton *skeleton;

  self = GF_LISTENER (object);

  if (self->screensaver_id)
    {
      g_bus_unown_name (self->screensaver_id);
      self->screensaver_id = 0;
    }

  if (self->login_id)
    {
      g_bus_unwatch_name (self->login_id);
      self->login_id = 0;
    }

  if (self->screensaver)
    {
      skeleton = G_DBUS_INTERFACE_SKELETON (self->screensaver);

      g_dbus_interface_skeleton_unexport (skeleton);
      g_clear_object (&self->screensaver);
    }

  release_inhibit_lock (self);

  g_clear_object (&self->login_session);
  g_clear_object (&self->login_manager);

  G_OBJECT_CLASS (gf_listener_parent_class)->dispose (object);
}

static void
install_signals (GObjectClass *object_class)
{
  listener_signals[LOCK] =
    g_signal_new ("lock", GF_TYPE_LISTENER, G_SIGNAL_RUN_LAST, 0, NULL,
                  NULL, NULL, G_TYPE_NONE, 0);

  listener_signals[SHOW_MESSAGE] =
    g_signal_new ("show-message", GF_TYPE_LISTENER, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL, G_TYPE_NONE, 3, G_TYPE_STRING,
                  G_TYPE_STRING, G_TYPE_STRING);

  listener_signals[SIMULATE_USER_ACTIVITY] =
    g_signal_new ("simulate-user-activity", GF_TYPE_LISTENER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  listener_signals[ACTIVE_CHANGED] =
    g_signal_new ("active-changed", GF_TYPE_LISTENER, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_BOOLEAN, 1, G_TYPE_BOOLEAN);

  listener_signals[PREPARE_FOR_SLEEP] =
    g_signal_new ("prepare-for-sleep", GF_TYPE_LISTENER, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
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
gf_listener_init (GfListener *self)
{
  self->screensaver = gf_screensaver_gen_skeleton_new ();
  self->screensaver_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                         SCREENSAVER_DBUS_NAME,
                                         G_BUS_NAME_OWNER_FLAGS_NONE,
                                         bus_acquired_handler, NULL,
                                         name_lost_handler,
                                         self, NULL);

  self->inhibit_lock_fd = -1;

  /* check if logind is running */
  if (access("/run/systemd/seats/", F_OK) >= 0)
    {
      self->login_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                         LOGIN_DBUS_NAME,
                                         G_BUS_NAME_WATCHER_FLAGS_NONE,
                                         name_appeared_handler,
                                         name_vanished_handler,
                                         self, NULL);
    }
}

GfListener *
gf_listener_new (void)
{
  return g_object_new (GF_TYPE_LISTENER, NULL);
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
  g_signal_emit (listener, listener_signals[ACTIVE_CHANGED], 0,
                 active, &handled);

  if (!handled)
    {
      /* if the signal is not handled then we haven't changed state */
      g_debug ("active-changed signal not handled");

      /* clear the idle state */
      if (active)
        set_session_idle_internal (listener, FALSE);

      return FALSE;
    }

  listener->active = active;

  /* if idle not in sync with active, change it */
  if (listener->session_idle != active)
    set_session_idle_internal (listener, active);

  if (active)
    listener->active_start = time (NULL);
  else
    listener->active_start = 0;

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
  return listener->active;
}

gboolean
gf_listener_set_session_idle (GfListener *listener,
                              gboolean    session_idle)
{
  gboolean activated;

  if (listener->session_idle == session_idle)
    {
      g_debug ("Trying to set session idle state when already: %s",
               session_idle ? "idle" : "not idle");

      return FALSE;
    }

  g_debug ("Setting session idle state: %s", session_idle ? "idle" : "not idle");

  listener->session_idle = session_idle;
  activated = TRUE;

  if (listener->session_idle)
    {
      g_debug ("Trying to activate");
      activated = gf_listener_set_active (listener, TRUE);
    }

  /* if activation fails then don't set idle */
  if (activated)
    {
      set_session_idle_internal (listener, session_idle);
    }
  else
    {
      g_debug ("Idle activation failed");
      listener->session_idle = !session_idle;
    }

  return activated;
}
