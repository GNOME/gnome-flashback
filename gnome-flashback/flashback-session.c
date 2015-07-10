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

#include <config.h>
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>

#include "flashback-session.h"

#define FLASHBACK_DBUS_SERVICE "org.gnome.Flashback"

#define SESSION_MANAGER_NAME      "org.gnome.SessionManager"
#define SESSION_MANAGER_PATH      "/org/gnome/SessionManager"
#define SESSION_MANAGER_INTERFACE "org.gnome.SessionManager"

#define SESSION_MANAGER_CLIENT_PRIVATE_INTERFACE "org.gnome.SessionManager.ClientPrivate"

struct _FlashbackSessionPrivate {
	GDBusConnection *connection;
	guint            name_lost_id;

	GDBusProxy      *session_manager_proxy;

	gchar           *object_path;
	GDBusProxy      *client_proxy;
};

G_DEFINE_TYPE_WITH_PRIVATE (FlashbackSession, flashback_session, G_TYPE_OBJECT)

static void
respond_to_end_session (GDBusProxy *proxy)
{
	g_dbus_proxy_call (proxy,
	                   "EndSessionResponse",
	                   g_variant_new ("(bs)", TRUE, ""),
	                   G_DBUS_CALL_FLAGS_NONE,
	                   -1,
	                   NULL,
	                   NULL,
	                   NULL);
}

static void
flashback_session_client_proxy_signal_cb (GDBusProxy *proxy,
                                          gchar      *sender_name,
                                          gchar      *signal_name,
                                          GVariant   *parameters,
                                          gpointer    user_data)
{
	if (g_str_equal (signal_name, "QueryEndSession"))
		respond_to_end_session (proxy);
	else if (g_str_equal (signal_name, "EndSession"))
		respond_to_end_session (proxy);
	else if (g_str_equal (signal_name, "Stop"))
		gtk_main_quit ();
}

static gboolean
flashback_session_get_session_manager_proxy (FlashbackSession *session)
{
	FlashbackSessionPrivate *priv;
	GError *error;
	GDBusProxyFlags flags;

	if (!session)
		return FALSE;

	priv = session->priv;
	error = NULL;
	flags = G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
	        G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS;

	priv->session_manager_proxy = g_dbus_proxy_new_sync (priv->connection,
	                                                     flags,
	                                                     NULL,
	                                                     SESSION_MANAGER_NAME,
	                                                     SESSION_MANAGER_PATH,
	                                                     SESSION_MANAGER_INTERFACE,
	                                                     NULL,
	                                                     &error);

	if (error) {
		g_warning ("Failed to get session manager proxy: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

static void
flashback_session_name_lost (GDBusConnection *connection,
                             const gchar     *sender_name,
                             const gchar     *object_path,
                             const gchar     *interface_name,
                             const gchar     *signal_name,
                             GVariant        *parameters,
                             gpointer         user_data)
{
	gtk_main_quit ();
}

static gboolean
flashback_session_request_name (FlashbackSession *session,
                                gboolean          replace)
{
	FlashbackSessionPrivate *priv;
	GBusNameOwnerFlags flags;
	GError *error;
	GVariant *result;
	guint32 reply;

	priv = session->priv;
	flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
	error = NULL;

	priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (error) {
		g_warning ("Failed to get session bus: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	if (replace)
		flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

	result = g_dbus_connection_call_sync (priv->connection,
	                                      DBUS_SERVICE_DBUS,
	                                      DBUS_PATH_DBUS,
	                                      DBUS_INTERFACE_DBUS,
	                                      "RequestName",
	                                      g_variant_new ("(su)",
	                                                     FLASHBACK_DBUS_SERVICE,
	                                                     flags),
	                                      G_VARIANT_TYPE ("(u)"),
	                                      G_DBUS_CALL_FLAGS_NONE,
	                                      -1,
	                                      NULL,
	                                      &error);

	if (error) {
		g_warning ("Failed to request name: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	g_variant_get (result, "(u)", &reply);
	g_variant_unref (result);

	switch (reply) {
		case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
		case DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER:
			priv->name_lost_id = g_dbus_connection_signal_subscribe (priv->connection,
			                                                         DBUS_SERVICE_DBUS,
			                                                         DBUS_INTERFACE_DBUS,
			                                                         "NameLost",
			                                                         DBUS_PATH_DBUS,
			                                                         FLASHBACK_DBUS_SERVICE,
			                                                         G_DBUS_SIGNAL_FLAGS_NONE,
			                                                         (GDBusSignalCallback) flashback_session_name_lost,
			                                                         session,
			                                                         NULL);
			break;
		case DBUS_REQUEST_NAME_REPLY_IN_QUEUE:
		case DBUS_REQUEST_NAME_REPLY_EXISTS:
			g_warning ("Failed to request name: the name already has an owner");
			return FALSE;
		default:
			g_warning ("Failed to request name: unhandled reply %u from RequestName", reply);
			return FALSE;
	}

	return TRUE;
}

static void
flashback_session_finalize (GObject *object)
{
	FlashbackSession *session;
	FlashbackSessionPrivate *priv;

	session = FLASHBACK_SESSION (object);
	priv = session->priv;

	if (priv->object_path != NULL) {
		g_free (priv->object_path);
		priv->object_path;
	}

	g_clear_object (&priv->client_proxy);
	g_clear_object (&priv->session_manager_proxy);

	if (priv->name_lost_id > 0) {
		g_dbus_connection_signal_unsubscribe (priv->connection, priv->name_lost_id);
		priv->name_lost_id = 0;
	}

	g_clear_object (&priv->connection);

	G_OBJECT_CLASS (flashback_session_parent_class)->finalize (object);
}

static void
flashback_session_class_init (FlashbackSessionClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = flashback_session_finalize;
}

static void
flashback_session_init (FlashbackSession *session)
{
	session->priv = flashback_session_get_instance_private (session);
}

FlashbackSession *
flashback_session_new (gboolean replace)
{
	FlashbackSession *session;

	session = g_object_new (FLASHBACK_TYPE_SESSION, NULL);

	if (!flashback_session_request_name (session, replace))
		g_clear_object (&session);

	if (!flashback_session_get_session_manager_proxy (session))
		g_clear_object (&session);

	return session;
}

gboolean
flashback_session_set_environment (FlashbackSession *session,
                                   const gchar      *name,
                                   const gchar      *value)
{
	GError *error;
	GVariant *parameters;
	GVariant *res;

	error = NULL;
	parameters = g_variant_new ("(ss)", name, value);
	res = g_dbus_proxy_call_sync (session->priv->session_manager_proxy,
	                              "Setenv",
	                              parameters,
	                              G_DBUS_CALL_FLAGS_NONE,
	                              -1,
	                              NULL,
	                              &error);

	if (error) {
		g_debug ("Failed to set the environment: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	g_variant_unref (res);

	return TRUE;
}

gboolean
flashback_session_register_client (FlashbackSession *session)
{
	FlashbackSessionPrivate *priv;
	GError *error;
	const gchar *app_id;
	const gchar *client_startup_id;
	gchar *startup_id;
	GVariant *parameters;
	GVariant *res;

	priv = session->priv;
	error = NULL;
	app_id = "gnome-flashback";
	client_startup_id = g_getenv ("DESKTOP_AUTOSTART_ID");

	if (client_startup_id != NULL) {
		startup_id = g_strdup (client_startup_id);
		g_unsetenv ("DESKTOP_AUTOSTART_ID");
	} else {
		startup_id = g_strdup ("");
	}

	parameters = g_variant_new ("(ss)", app_id, startup_id);
	g_free (startup_id);

	res = g_dbus_proxy_call_sync (priv->session_manager_proxy,
	                              "RegisterClient",
	                              parameters,
	                              G_DBUS_CALL_FLAGS_NONE,
	                              -1,
	                              NULL,
	                              &error);

	if (error) {
		g_warning ("Failed to register client: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	g_variant_get (res, "(o)", &priv->object_path);
	g_variant_unref (res);

	priv->client_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
	                                                    G_DBUS_PROXY_FLAGS_NONE,
	                                                    NULL,
	                                                    SESSION_MANAGER_NAME,
	                                                    priv->object_path,
	                                                    SESSION_MANAGER_CLIENT_PRIVATE_INTERFACE,
	                                                    NULL,
	                                                    &error);

	if (error) {
		g_warning ("Failed to get a client proxy: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	g_signal_connect (priv->client_proxy, "g-signal",
	                  G_CALLBACK (flashback_session_client_proxy_signal_cb), session);

	return TRUE;
}
