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

#include <gtk/gtk.h>

#include "dbus-end-session-dialog.h"
#include "gf-end-session-dialog.h"
#include "gf-inhibit-dialog.h"

struct _GfEndSessionDialog
{
  GObject                 parent;

  gint                    bus_name;
  GDBusInterfaceSkeleton *iface;

  GtkWidget              *dialog;
};

G_DEFINE_TYPE (GfEndSessionDialog, gf_end_session_dialog, G_TYPE_OBJECT)

static void
inhibit_dialog_response (GfInhibitDialog *dialog,
                         guint            response_id,
                         gpointer         user_data)
{
  DBusEndSessionDialog *object;
  gint action;

  object = DBUS_END_SESSION_DIALOG (user_data);
  g_object_get (dialog, "action", &action, NULL);

  switch (response_id)
    {
      case GF_RESPONSE_CANCEL:
        break;

      case GF_RESPONSE_ACCEPT:
        if (action == GF_LOGOUT_ACTION_LOGOUT)
          dbus_end_session_dialog_emit_confirmed_logout (object);
        else if (action == GF_LOGOUT_ACTION_SHUTDOWN)
          dbus_end_session_dialog_emit_confirmed_shutdown (object);
        else if (action == GF_LOGOUT_ACTION_REBOOT)
          dbus_end_session_dialog_emit_confirmed_reboot (object);
        else if (action == GF_LOGOUT_ACTION_HIBERNATE)
          dbus_end_session_dialog_emit_confirmed_hibernate (object);
        else if (action == GF_LOGOUT_ACTION_SUSPEND)
          dbus_end_session_dialog_emit_confirmed_suspend (object);
        else if (action == GF_LOGOUT_ACTION_HYBRID_SLEEP)
          dbus_end_session_dialog_emit_confirmed_hybrid_sleep (object);
        else
          g_assert_not_reached ();
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  gf_inhibit_dialog_close (dialog);
}

static void
inhibit_dialog_close (GfInhibitDialog *dialog,
                      gpointer         user_data)
{
  DBusEndSessionDialog *object;

  object = DBUS_END_SESSION_DIALOG (user_data);

  dbus_end_session_dialog_emit_canceled (object);
  dbus_end_session_dialog_emit_closed (object);
}

static void
closed (DBusEndSessionDialog *object,
        gpointer              user_data)
{
  GfEndSessionDialog *dialog;

  dialog = GF_END_SESSION_DIALOG (user_data);

  dialog->dialog = NULL;
}

static gboolean
handle_open (DBusEndSessionDialog  *object,
             GDBusMethodInvocation *invocation,
             guint                  type,
             guint                  timestamp,
             guint                  seconds_to_stay_open,
             const gchar *const    *inhibitor_object_paths,
             gpointer               user_data)
{
  GfEndSessionDialog *dialog;
  GfInhibitDialog *inhibit_dialog;

  dialog = GF_END_SESSION_DIALOG (user_data);

  if (dialog->dialog != NULL)
    {
      inhibit_dialog = GF_INHIBIT_DIALOG (dialog->dialog);

      g_object_set (dialog->dialog,
                    "inhibitor-paths", inhibitor_object_paths,
                    NULL);

      gf_inhibit_dialog_present (inhibit_dialog, timestamp);
      dbus_end_session_dialog_complete_open (object, invocation);

      return TRUE;
    }

  dialog->dialog = gf_inhibit_dialog_new (type, seconds_to_stay_open,
                                          inhibitor_object_paths);

  inhibit_dialog = GF_INHIBIT_DIALOG (dialog->dialog);

  g_signal_connect (dialog->dialog, "response",
                    G_CALLBACK (inhibit_dialog_response), object);
  g_signal_connect (dialog->dialog, "destroy",
                    G_CALLBACK (inhibit_dialog_close), object);
  g_signal_connect (dialog->dialog, "close",
                    G_CALLBACK (inhibit_dialog_close), object);

  g_signal_connect (object, "closed",
                    G_CALLBACK (closed), dialog);

  gf_inhibit_dialog_present (inhibit_dialog, timestamp);
  dbus_end_session_dialog_complete_open (object, invocation);

  return TRUE;
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  GfEndSessionDialog *dialog;
  DBusEndSessionDialog *skeleton;
  GError *error;

  dialog = GF_END_SESSION_DIALOG (user_data);
  skeleton = dbus_end_session_dialog_skeleton_new ();

  dialog->iface = G_DBUS_INTERFACE_SKELETON (skeleton);

  g_signal_connect (skeleton, "handle-open",
                    G_CALLBACK (handle_open), dialog);

  error = NULL;
  if (!g_dbus_interface_skeleton_export (dialog->iface, connection,
                                         "/org/gnome/SessionManager/EndSessionDialog",
                                         &error))
    {
      g_warning ("Failed to export interface: %s", error->message);
      g_error_free (error);

      return;
    }
}

static void
gf_end_session_dialog_finalize (GObject *object)
{
  GfEndSessionDialog *dialog;

  dialog = GF_END_SESSION_DIALOG (object);

  if (dialog->dialog != NULL)
    {
      gtk_widget_destroy (dialog->dialog);
      dialog->dialog = NULL;
    }

  if (dialog->iface != NULL)
    {
      g_dbus_interface_skeleton_unexport (dialog->iface);

      g_object_unref (dialog->iface);
      dialog->iface = NULL;
    }

  if (dialog->bus_name > 0)
    {
      g_bus_unwatch_name (dialog->bus_name);
      dialog->bus_name = 0;
    }

  G_OBJECT_CLASS (gf_end_session_dialog_parent_class)->finalize (object);
}

static void
gf_end_session_dialog_class_init (GfEndSessionDialogClass *dialog_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (dialog_class);

  object_class->finalize = gf_end_session_dialog_finalize;
}

static void
gf_end_session_dialog_init (GfEndSessionDialog *dialog)
{
  dialog->bus_name = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                       "org.gnome.Shell",
                                       G_BUS_NAME_WATCHER_FLAGS_NONE,
                                       name_appeared_handler,
                                       NULL,
                                       dialog,
                                       NULL);
}

GfEndSessionDialog *
gf_end_session_dialog_new (void)
{
  return g_object_new (GF_TYPE_END_SESSION_DIALOG, NULL);
}
