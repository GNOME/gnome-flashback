/*
 * Copyright (C) 2014 - 2015 Alberts Muktupāvels
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

#include "config.h"

#include <gio/gio.h>

#include "gf-session.h"

#define GF_DBUS_NAME "org.gnome.Flashback"

#define GSM_DBUS_NAME  "org.gnome.SessionManager"
#define GSM_DBUS_PATH  "/org/gnome/SessionManager"
#define GSM_DBUS_IFACE "org.gnome.SessionManager"

#define GSM_CLIENT_PRIVATE_DBUS_IFACE "org.gnome.SessionManager.ClientPrivate"

struct _GfSession
{
  GObject                 parent;

  GfSessionReadyCallback  ready_cb;
  GfSessionEndCallback    end_cb;
  gpointer                user_data;

  gulong                  name_id;

  GDBusProxy             *manager_proxy;

  gchar                  *object_path;
  GDBusProxy             *client_proxy;
};

G_DEFINE_TYPE (GfSession, gf_session, G_TYPE_OBJECT)

static void
respond_to_end_session (GDBusProxy *proxy)
{
  GVariant *parameters;

  parameters = g_variant_new ("(bs)", TRUE, "");

  g_dbus_proxy_call (proxy,
                     "EndSessionResponse", parameters,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);
}

static void
g_signal_cb (GDBusProxy *proxy,
             gchar      *sender_name,
             gchar      *signal_name,
             GVariant   *parameters,
             gpointer    user_data)
{
  GfSession *session;

  session = GF_SESSION (user_data);

  if (g_strcmp0 (signal_name, "QueryEndSession") == 0)
    {
      respond_to_end_session (proxy);
    }
  else if (g_strcmp0 (signal_name, "EndSession") == 0)
    {
      respond_to_end_session (proxy);
    }
  else if (g_strcmp0 (signal_name, "Stop") == 0)
    {
      if (session->end_cb != NULL)
        session->end_cb (session, session->user_data);
    }
}

static void
client_proxy_ready_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GfSession *session;
  GError *error;

  session = GF_SESSION (user_data);

  error = NULL;
  session->client_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      g_warning ("Failed to get a client proxy: %s", error->message);
      g_error_free (error);

      return;
    }

  g_signal_connect (session->client_proxy, "g-signal",
                    G_CALLBACK (g_signal_cb), session);
}

static void
manager_proxy_ready_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  GfSession *session;
  GError *error;

  session = GF_SESSION (user_data);

  error = NULL;
  session->manager_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      g_warning ("Failed to get session manager proxy: %s", error->message);
      g_error_free (error);

      if (session->end_cb != NULL)
        session->end_cb (session, session->user_data);

      return;
    }

  if (session->ready_cb != NULL)
    session->ready_cb (session, session->user_data);
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  GDBusProxyFlags flags;

  flags = G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
          G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS;

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION, flags, NULL,
                            GSM_DBUS_NAME, GSM_DBUS_PATH, GSM_DBUS_IFACE,
                            NULL, manager_proxy_ready_cb, user_data);
}

static void
name_lost_cb (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  GfSession *session;

  session = GF_SESSION (user_data);

  if (session->end_cb != NULL)
    session->end_cb (session, session->user_data);
}

static void
gf_session_dispose (GObject *object)
{
  GfSession *session;

  session = GF_SESSION (object);

  if (session->name_id > 0)
    {
      g_bus_unown_name (session->name_id);
      session->name_id = 0;
    }

  g_clear_object (&session->manager_proxy);
  g_clear_object (&session->client_proxy);

  G_OBJECT_CLASS (gf_session_parent_class)->dispose (object);
}

static void
gf_session_finalize (GObject *object)
{
  GfSession *session;

  session = GF_SESSION (object);

  g_free (session->object_path);

  G_OBJECT_CLASS (gf_session_parent_class)->finalize (object);
}

static void
gf_session_class_init (GfSessionClass *session_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (session_class);

  object_class->dispose = gf_session_dispose;
  object_class->finalize = gf_session_finalize;
}

static void
gf_session_init (GfSession *session)
{
}

/**
 * gf_session_new:
 * @replace: %TRUE to replace current session
 * @ready_cb:
 * @end_cb:
 * @user_data: user data
 *
 * Creates a new #GfSession.
 *
 * Returns: (transfer full): a newly created #GfSession.
 */
GfSession *
gf_session_new (gboolean                replace,
                GfSessionReadyCallback  ready_cb,
                GfSessionEndCallback    end_cb,
                gpointer                user_data)
{
  GfSession *session;
  GBusNameOwnerFlags flags;

  session = g_object_new (GF_TYPE_SESSION, NULL);

  session->ready_cb = ready_cb;
  session->end_cb = end_cb;
  session->user_data = user_data;

  flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
  if (replace)
    flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

  session->name_id = g_bus_own_name (G_BUS_TYPE_SESSION, GF_DBUS_NAME, flags,
                                     NULL, name_acquired_cb, name_lost_cb,
                                     session, NULL);

  return session;
}

/**
 * gf_session_set_environment:
 * @session: a #GfSession
 * @name: the variable name
 * @value: the value
 *
 * Set environment variable to specified value. May only be used during the
 * Session Manager initialization phase.
 *
 * Returns: %TRUE if environment was set, %FALSE otherwise.
 */
gboolean
gf_session_set_environment (GfSession   *session,
                            const gchar *name,
                            const gchar *value)
{
  GVariant *parameters;
  GError *error;
  GVariant *variant;

  parameters = g_variant_new ("(ss)", name, value);

  error = NULL;
  variant = g_dbus_proxy_call_sync (session->manager_proxy,
                                    "Setenv", parameters,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Failed to set the environment: %s", error->message);
      g_error_free (error);

      return FALSE;
    }

  g_variant_unref (variant);

  return TRUE;
}

/**
 * gf_session_register:
 * @session: a #GfSession
 *
 * Register as a Session Management client.
 *
 * Returns: %TRUE if we have registered as client, %FALSE otherwise.
 */
gboolean
gf_session_register (GfSession *session)
{
  const gchar *app_id;
  const gchar *autostart_id;
  gchar *client_startup_id;
  GVariant *parameters;
  GError *error;
  GVariant *variant;
  GDBusProxyFlags flags;

  app_id = "gnome-flashback";
  autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");

  if (autostart_id != NULL)
    {
      client_startup_id = g_strdup (autostart_id);
      g_unsetenv ("DESKTOP_AUTOSTART_ID");
    }
  else
    {
      client_startup_id = g_strdup ("");
    }

  parameters = g_variant_new ("(ss)", app_id, client_startup_id);
  g_free (client_startup_id);

  error = NULL;
  variant = g_dbus_proxy_call_sync (session->manager_proxy,
                                    "RegisterClient", parameters,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Failed to register client: %s", error->message);
      g_error_free (error);

      return FALSE;
    }

  g_variant_get (variant, "(o)", &session->object_path);
  g_variant_unref (variant);

  flags = G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES;

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION, flags, NULL,
                            GSM_DBUS_NAME, session->object_path,
                            GSM_CLIENT_PRIVATE_DBUS_IFACE,
                            NULL, client_proxy_ready_cb, session);

  return TRUE;
}
