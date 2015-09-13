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

#include <gtk/gtk.h>
#include "config.h"
#include "dbus-end-session-dialog.h"
#include "gf-end-session-dialog.h"
#include "flashback-inhibit-dialog.h"

struct _FlashbackEndSessionDialogPrivate {
	gint                    bus_name;
	GDBusInterfaceSkeleton *iface;
	GtkWidget              *dialog;
};

G_DEFINE_TYPE (FlashbackEndSessionDialog, flashback_end_session_dialog, G_TYPE_OBJECT);

static void
inhibit_dialog_response (FlashbackInhibitDialog *dialog,
                         guint                   response_id,
                         DBusEndSessionDialog   *object)
{
	int action;

	g_object_get (dialog, "action", &action, NULL);

	switch (response_id) {
	case FLASHBACK_RESPONSE_CANCEL:
		break;
	case FLASHBACK_RESPONSE_ACCEPT:
		if (action == FLASHBACK_LOGOUT_ACTION_LOGOUT) {
			dbus_end_session_dialog_emit_confirmed_logout (object);
		} else if (action == FLASHBACK_LOGOUT_ACTION_SHUTDOWN) {
			dbus_end_session_dialog_emit_confirmed_shutdown (object);
		} else if (action == FLASHBACK_LOGOUT_ACTION_REBOOT) {
			dbus_end_session_dialog_emit_confirmed_reboot (object);
		} else if (action == FLASHBACK_LOGOUT_ACTION_HIBERNATE) {
			dbus_end_session_dialog_emit_confirmed_hibernate (object);
		} else if (action == FLASHBACK_LOGOUT_ACTION_SUSPEND) {
			dbus_end_session_dialog_emit_confirmed_suspend (object);
		} else if (action == FLASHBACK_LOGOUT_ACTION_HYBRID_SLEEP) {
			dbus_end_session_dialog_emit_confirmed_hybrid_sleep (object);
		} else {
			g_assert_not_reached ();
		}
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	flashback_inhibit_dialog_close (dialog);
}

static void
inhibit_dialog_close (FlashbackInhibitDialog *dialog,
                      DBusEndSessionDialog   *object)
{
	dbus_end_session_dialog_emit_canceled (object);
	dbus_end_session_dialog_emit_closed (object);
}

static void
closed (DBusEndSessionDialog      *object,
        FlashbackEndSessionDialog *dialog)
{
	dialog->priv->dialog = NULL;
}

static gboolean
handle_open (DBusEndSessionDialog *object,
             GDBusMethodInvocation          *invocation,
             guint                           arg_type,
             guint                           arg_timestamp,
             guint                           arg_seconds_to_stay_open,
             const gchar *const             *arg_inhibitor_object_paths,
             gpointer                        user_data)
{
	FlashbackEndSessionDialog *dialog = user_data;

	if (dialog->priv->dialog != NULL) {
		g_object_set (dialog->priv->dialog, "inhibitor-paths", arg_inhibitor_object_paths, NULL);

		if (arg_timestamp != 0) {
			gtk_window_present_with_time (GTK_WINDOW (dialog->priv->dialog), arg_timestamp);
		} else {
			gtk_window_present (GTK_WINDOW (dialog->priv->dialog));
		}

		dbus_end_session_dialog_complete_open (object, invocation);
		return TRUE;
	}

	dialog->priv->dialog = flashback_inhibit_dialog_new (arg_type,
	                                                     arg_seconds_to_stay_open,
	                                                     arg_inhibitor_object_paths);

	g_signal_connect (dialog->priv->dialog, "response", G_CALLBACK (inhibit_dialog_response), object);
	g_signal_connect (dialog->priv->dialog, "destroy", G_CALLBACK (inhibit_dialog_close), object);
	g_signal_connect (dialog->priv->dialog, "close", G_CALLBACK (inhibit_dialog_close), object);
	g_signal_connect (object, "closed", G_CALLBACK (closed), dialog);

	if (arg_timestamp != 0) {
		gtk_window_present_with_time (GTK_WINDOW (dialog->priv->dialog), arg_timestamp);
	} else {
		gtk_window_present (GTK_WINDOW (dialog->priv->dialog));
	}

	dbus_end_session_dialog_complete_open (object, invocation);
	return TRUE;
}

static void
/*
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
*/
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
	FlashbackEndSessionDialog *dialog;
	GError *error = NULL;

	dialog = FLASHBACK_END_SESSION_DIALOG (user_data);
	dialog->priv->iface = G_DBUS_INTERFACE_SKELETON (dbus_end_session_dialog_skeleton_new ());
	g_signal_connect (dialog->priv->iface, "handle-open", G_CALLBACK (handle_open), dialog);

	if (!g_dbus_interface_skeleton_export (dialog->priv->iface,
	                                       connection,
	                                       "/org/gnome/SessionManager/EndSessionDialog",
	                                       &error)) {
		g_warning ("Failed to export interface: %s", error->message);
		g_error_free (error);
		return;
	}
}

/*
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
*/

static void
flashback_end_session_dialog_finalize (GObject *object)
{
	FlashbackEndSessionDialog *dialog = FLASHBACK_END_SESSION_DIALOG (object);

	if (dialog->priv->dialog) {
		gtk_widget_destroy (dialog->priv->dialog);
		dialog->priv->dialog = NULL;
	}

	if (dialog->priv->iface) {
		g_dbus_interface_skeleton_unexport (dialog->priv->iface);

		g_object_unref (dialog->priv->iface);
		dialog->priv->iface = NULL;
	}

	if (dialog->priv->bus_name) {
		/*
		g_bus_unown_name (dialog->priv->bus_name);
		*/
		g_bus_unwatch_name (dialog->priv->bus_name);
		dialog->priv->bus_name = 0;
	}

	G_OBJECT_CLASS (flashback_end_session_dialog_parent_class)->finalize (object);
}

static void
flashback_end_session_dialog_init (FlashbackEndSessionDialog *dialog)
{
	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (dialog,
	                                            FLASHBACK_TYPE_END_SESSION_DIALOG,
	                                            FlashbackEndSessionDialogPrivate);

	dialog->priv->dialog = NULL;
	dialog->priv->iface = NULL;
	dialog->priv->bus_name = g_bus_watch_name (G_BUS_TYPE_SESSION,
	                                           "org.gnome.Shell",
	                                           G_BUS_NAME_WATCHER_FLAGS_NONE,
	                                           name_appeared_handler,
	                                           NULL,
	                                           dialog,
	                                           NULL);
	/*
	dialog->priv->bus_name = g_bus_own_name (G_BUS_TYPE_SESSION,
	                                         "org.gnome.Shell",
	                                         G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
	                                         G_BUS_NAME_OWNER_FLAGS_REPLACE,
	                                         on_bus_acquired,
	                                         on_name_acquired,
	                                         on_name_lost,
	                                         dialog,
	                                         NULL);
	*/
}

static void
flashback_end_session_dialog_class_init (FlashbackEndSessionDialogClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = flashback_end_session_dialog_finalize;

	g_type_class_add_private (class, sizeof (FlashbackEndSessionDialogPrivate));
}

FlashbackEndSessionDialog *
flashback_end_session_dialog_new (void)
{
	return g_object_new (FLASHBACK_TYPE_END_SESSION_DIALOG,
	                     NULL);
}
