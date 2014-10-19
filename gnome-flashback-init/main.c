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
#include <gtk/gtk.h>

static void
set_session_env (GDBusProxy  *proxy,
                 const gchar *name,
                 const gchar *value)
{
	GError *error;
	GVariant *parameters;
	GVariant *res;

	error = NULL;
	parameters = g_variant_new ("(ss)", name, value);
	res = g_dbus_proxy_call_sync (proxy,
	                              "Setenv",
	                              parameters,
	                              G_DBUS_CALL_FLAGS_NONE,
	                              -1,
	                              NULL,
	                              &error);

	if (error) {
		g_debug ("Failed to set the environment: %s", error->message);
		g_error_free (error);
		return;
	}

	g_variant_unref (res);
}

int
main (int argc, char *argv[])
{
	GError *error;
	GDBusConnection *connection;
	GDBusProxy *proxy;
	const gchar *app_id;
	const gchar *client_startup_id;
	GVariant *parameters;
	GVariant *res;

	error = NULL;
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (!connection) {
		g_warning ("Cannot connect to session bus: %s", error->message);
		g_error_free (error);
		return 1;
	}

	proxy = g_dbus_proxy_new_sync (connection,
	                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
	                               G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
	                               NULL,
	                               "org.gnome.SessionManager",
	                               "/org/gnome/SessionManager",
	                               "org.gnome.SessionManager",
	                               NULL,
	                               &error);

	if (error) {
		g_warning ("Failed to get a session proxy: %s", error->message);
		g_error_free (error);
		g_object_unref (connection);
		return 1;
	}

	set_session_env (proxy, "XDG_MENU_PREFIX", "gnome-flashback-");

	app_id = "gnome-flashback-init";
	client_startup_id = g_getenv ("DESKTOP_AUTOSTART_ID");

	parameters = g_variant_new ("(ss)",app_id, client_startup_id ? client_startup_id : "");
	res = g_dbus_proxy_call_sync (proxy,
	                              "RegisterClient",
	                              parameters,
	                              G_DBUS_CALL_FLAGS_NONE,
	                              -1,
	                              NULL,
	                              &error);

	if (error) {
		g_warning ("Failed to register client: %s", error->message);
		g_error_free (error);
		g_object_unref (proxy);
		g_object_unref (connection);
		return 1;
	}

	g_variant_unref (res);
	g_object_unref (proxy);
	g_object_unref (connection);

	return 0;
}
