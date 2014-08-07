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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "config.h"
#include "flashback-application.h"
#include "flashback-desktop-background.h"
#include "flashback-end-session-dialog-generated.h"
#include "flashback-inhibit-dialog.h"
#include "flashback-gsettings.h"

struct _FlashbackApplicationPrivate {
	FlashbackDesktopBackground *background;
	GSettings                  *settings;
	gint                        bus_name;
};

G_DEFINE_TYPE (FlashbackApplication, flashback_application, GTK_TYPE_APPLICATION);

static void
inhibit_dialog_response (FlashbackInhibitDialog    *dialog,
                         guint                      response_id,
                         FlashbackEndSessionDialog *object)
{
	int action;

	g_object_get (dialog, "action", &action, NULL);
	gtk_widget_destroy (GTK_WIDGET (dialog));

	switch (response_id) {
	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_DELETE_EVENT:
		if (action == FLASHBACK_LOGOUT_ACTION_LOGOUT
			|| action == FLASHBACK_LOGOUT_ACTION_SHUTDOWN
			|| action == FLASHBACK_LOGOUT_ACTION_REBOOT) {
			g_print ("cancel action %d\n", action);
			flashback_end_session_dialog_emit_canceled (object);
			flashback_end_session_dialog_emit_closed (object);
		}
		break;
	case GTK_RESPONSE_ACCEPT:
		g_print ("confirm action %d\n", action);
		if (action == FLASHBACK_LOGOUT_ACTION_LOGOUT) {
			flashback_end_session_dialog_emit_confirmed_logout (object);
		} else if (action == FLASHBACK_LOGOUT_ACTION_SHUTDOWN) {
			flashback_end_session_dialog_emit_confirmed_shutdown (object);
		} else if (action == FLASHBACK_LOGOUT_ACTION_REBOOT) {
			flashback_end_session_dialog_emit_confirmed_reboot (object);
		}
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static gboolean
handle_open (FlashbackEndSessionDialog *object,
             GDBusMethodInvocation     *invocation,
             guint                      arg_type,
             guint                      arg_timestamp,
             guint                      arg_seconds_to_stay_open,
             const gchar *const        *arg_inhibitor_object_paths,
             gpointer                   user_data)
{
	g_print ("handle open\n");

	GtkWidget *dialog = flashback_inhibit_dialog_new (arg_type, arg_seconds_to_stay_open, arg_inhibitor_object_paths);

	g_signal_connect (dialog, "response", G_CALLBACK (inhibit_dialog_response), object);

	gtk_window_present_with_time (GTK_WINDOW (dialog), arg_timestamp);

	flashback_end_session_dialog_complete_open (object, invocation);

	return TRUE;
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
	g_print ("Acquired a message bus connection\n");

	GDBusInterfaceSkeleton *iface;
	GError *error = NULL;

	iface = G_DBUS_INTERFACE_SKELETON (flashback_end_session_dialog_skeleton_new ());
	g_signal_connect (iface, "handle-open", G_CALLBACK (handle_open), NULL);

	if (!g_dbus_interface_skeleton_export (iface,
	                                       connection,
	                                       "/org/gnome/SessionManager/EndSessionDialog",
	                                       &error)) {
		g_warning ("Failed to export interface: %s", error->message);
		g_error_free (error);
		return;
	}
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
	g_print ("Acquired the name %s\n", name);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
	g_print ("Lost the name %s\n", name);
}

static void
flashback_application_settings_changed (GSettings   *settings,
                                        const gchar *key,
                                        gpointer     user_data)
{
	FlashbackApplication *app = FLASHBACK_APPLICATION (user_data);
	gboolean draw_background = g_settings_get_boolean (settings, key);

	if (draw_background) {
		if (app->priv->background == NULL) {
			app->priv->background = flashback_desktop_background_new ();
		}
	} else {
		if (app->priv->background) {
			g_object_unref (app->priv->background);
			app->priv->background = NULL;
		}
	}
}

static void
flashback_application_activate (GApplication *application)
{
}

static void
flashback_application_startup (GApplication *application)
{
	FlashbackApplication *app = FLASHBACK_APPLICATION (application);

	G_APPLICATION_CLASS (flashback_application_parent_class)->startup (application);

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	app->priv->settings = g_settings_new (FLASHBACK_SCHEMA);

	g_signal_connect (app->priv->settings, "changed::" KEY_DRAW_BACKGROUND,
	                  G_CALLBACK (flashback_application_settings_changed), app);
	flashback_application_settings_changed (app->priv->settings, KEY_DRAW_BACKGROUND, app);

	app->priv->bus_name = g_bus_own_name (G_BUS_TYPE_SESSION,
	                                      "org.gnome.Shell",
	                                      G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
	                                      G_BUS_NAME_OWNER_FLAGS_REPLACE,
	                                      on_bus_acquired,
	                                      on_name_acquired,
	                                      on_name_lost,
	                                      NULL,
	                                      NULL);

	g_application_hold (application);
}

static void
flashback_application_shutdown (GApplication *application)
{
	FlashbackApplication *app = FLASHBACK_APPLICATION (application);

	if (app->priv->background) {
		g_object_unref (app->priv->background);
		app->priv->background = NULL;
	}

	if (app->priv->settings) {
		g_object_unref (app->priv->settings);
		app->priv->settings = NULL;
	}

	g_bus_unown_name (app->priv->bus_name);

	G_APPLICATION_CLASS (flashback_application_parent_class)->shutdown (application);
}

static void
flashback_application_init (FlashbackApplication *application)
{
	application->priv = G_TYPE_INSTANCE_GET_PRIVATE (application, FLASHBACK_TYPE_APPLICATION, FlashbackApplicationPrivate);
}

static void
flashback_application_class_init (FlashbackApplicationClass *class)
{
	GApplicationClass *application_class = G_APPLICATION_CLASS (class);

	application_class->startup  = flashback_application_startup;
	application_class->shutdown = flashback_application_shutdown;
	application_class->activate = flashback_application_activate;

	g_type_class_add_private (class, sizeof (FlashbackApplicationPrivate));
}

FlashbackApplication *
flashback_application_new (void)
{
	return g_object_new (FLASHBACK_TYPE_APPLICATION,
	                     "application-id", "org.gnome.gnome-flashback",
	                     "flags", G_APPLICATION_FLAGS_NONE,
	                     "register-session", TRUE,
	                     NULL);
}
