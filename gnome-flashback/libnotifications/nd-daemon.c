/*
 * Copyright (C) 2006 Christian Hammond <chipx86@chipx86.com>
 * Copyright (C) 2005 John (J5) Palmieri <johnp@redhat.com>
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2015 Alberts Muktupāvels
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
#include "nd-daemon.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "dbus/gf-fd-notifications-gen.h"
#include "nd-notification.h"
#include "nd-queue.h"

#define NOTIFICATIONS_DBUS_NAME "org.freedesktop.Notifications"
#define NOTIFICATIONS_DBUS_PATH "/org/freedesktop/Notifications"

#define INFO_NAME "Notification Daemon"
#define INFO_VENDOR "GNOME"
#define INFO_VERSION PACKAGE_VERSION
#define INFO_SPEC_VERSION "1.2"

struct _NdDaemon
{
  GObject               parent;

  GfFdNotificationsGen *notifications;
  guint                 bus_name_id;

  NdQueue              *queue;
};

G_DEFINE_TYPE (NdDaemon, nd_daemon, G_TYPE_OBJECT)

static void
closed_cb (NdNotification *notification,
           gint            reason,
           gpointer        user_data)
{
  NdDaemon *daemon;
  gint id;

  daemon = ND_DAEMON (user_data);
  id = nd_notification_get_id (notification);

  gf_fd_notifications_gen_emit_notification_closed (daemon->notifications,
                                                    id, reason);
}

static void
action_invoked_cb (NdNotification *notification,
                   const gchar    *action,
                   guint32         time,
                   gpointer        user_data)
{
  NdDaemon *daemon;
  gint id;

  daemon = ND_DAEMON (user_data);
  id = nd_notification_get_id (notification);

  {
    GdkDisplay *display;
    GdkAppLaunchContext *context;
    GDesktopAppInfo *app_info;
    char *activation_token;

    display = gdk_display_get_default ();
    context = gdk_display_get_app_launch_context (display);

    gdk_app_launch_context_set_timestamp (context, time);

    app_info = g_desktop_app_info_new ("gnome-flashback.desktop");
    activation_token = g_app_launch_context_get_startup_notify_id (G_APP_LAUNCH_CONTEXT (context),
                                                                   G_APP_INFO (app_info),
                                                                   NULL);

    gf_fd_notifications_gen_emit_activation_token (daemon->notifications,
                                                   id, activation_token);

    g_free (activation_token);
    g_object_unref (app_info);
    g_object_unref (context);
  }

  gf_fd_notifications_gen_emit_action_invoked (daemon->notifications,
                                               id, action);

  /* Resident notifications does not close when actions are invoked. */
  if (!nd_notification_get_is_resident (notification))
    nd_notification_close (notification, ND_NOTIFICATION_CLOSED_USER);
}

static gboolean
handle_close_notification_cb (GfFdNotificationsGen  *object,
                              GDBusMethodInvocation *invocation,
                              guint                  id,
                              gpointer               user_data)
{
  NdDaemon *daemon;
  const gchar *error_name;
  const gchar *error_message;
  NdNotification *notification;

  daemon = ND_DAEMON (user_data);
  error_name = "org.freedesktop.Notifications.InvalidId";
  error_message = _("Invalid notification identifier");

  if (id == 0)
    {
      g_dbus_method_invocation_return_dbus_error (invocation, error_name,
                                                  error_message);

      return TRUE;
    }

  notification = nd_queue_lookup (daemon->queue, id);

  if (notification == NULL)
    {
      g_dbus_method_invocation_return_dbus_error (invocation, error_name,
                                                  error_message);

      return TRUE;
    }

  nd_notification_close (notification, ND_NOTIFICATION_CLOSED_API);
  gf_fd_notifications_gen_complete_close_notification (object, invocation);

  return TRUE;
}

static gboolean
handle_get_capabilities_cb (GfFdNotificationsGen  *object,
                            GDBusMethodInvocation *invocation,
                            gpointer               user_data)
{
  const gchar *const capabilities[] =
  {
    "actions", "body", "body-hyperlinks", "body-markup", "icon-static",
    "sound", "persistence", "action-icons", NULL
  };

  gf_fd_notifications_gen_complete_get_capabilities (object, invocation,
                                                     capabilities);

  return TRUE;
}

static gboolean
handle_get_server_information_cb (GfFdNotificationsGen  *object,
                                  GDBusMethodInvocation *invocation,
                                  gpointer               user_data)
{
  gf_fd_notifications_gen_complete_get_server_information (object, invocation,
                                                           INFO_NAME,
                                                           INFO_VENDOR,
                                                           INFO_VERSION,
                                                           INFO_SPEC_VERSION);

  return TRUE;
}

static gboolean
handle_notify_cb (GfFdNotificationsGen  *object,
                  GDBusMethodInvocation *invocation,
                  const gchar           *app_name,
                  guint                  replaces_id,
                  const gchar           *app_icon,
                  const gchar           *summary,
                  const gchar           *body,
                  const gchar *const    *actions,
                  GVariant              *hints,
                  gint                   expire_timeout,
                  gpointer               user_data)
{
  NdDaemon *daemon;
  NdNotification *notification;
  gint new_id;

  daemon = ND_DAEMON (user_data);

  if (replaces_id > 0)
    {
      notification = nd_queue_lookup (daemon->queue, replaces_id);

      if (notification == NULL)
        replaces_id = 0;
      else
        g_object_ref (notification);
    }

  if (replaces_id == 0)
    {
      const gchar *sender;

      sender = g_dbus_method_invocation_get_sender (invocation);
      notification = nd_notification_new (sender);

      g_signal_connect (notification, "closed",
                        G_CALLBACK (closed_cb), daemon);
      g_signal_connect (notification, "action-invoked",
                        G_CALLBACK (action_invoked_cb), daemon);
    }

  nd_notification_update (notification, app_name, app_icon, summary, body,
                          actions, hints, expire_timeout);

  if (replaces_id == 0 || !nd_notification_get_is_queued (notification))
    {
      nd_queue_add (daemon->queue, notification);
      nd_notification_set_is_queued (notification, TRUE);
    }

  new_id = nd_notification_get_id (notification);
  gf_fd_notifications_gen_complete_notify (object, invocation, new_id);

  g_object_unref (notification);

  return TRUE;
}

static void
bus_acquired_handler_cb (GDBusConnection *connection,
                         const gchar     *name,
                         gpointer         user_data)
{
  NdDaemon *daemon;
  GDBusInterfaceSkeleton *skeleton;
  GError *error;
  gboolean exported;

  daemon = ND_DAEMON (user_data);
  skeleton = G_DBUS_INTERFACE_SKELETON (daemon->notifications);

  g_signal_connect (daemon->notifications, "handle-close-notification",
                    G_CALLBACK (handle_close_notification_cb), daemon);
  g_signal_connect (daemon->notifications, "handle-get-capabilities",
                    G_CALLBACK (handle_get_capabilities_cb), daemon);
  g_signal_connect (daemon->notifications, "handle-get-server-information",
                    G_CALLBACK (handle_get_server_information_cb), daemon);
  g_signal_connect (daemon->notifications, "handle-notify",
                    G_CALLBACK (handle_notify_cb), daemon);

  error = NULL;
  exported = g_dbus_interface_skeleton_export (skeleton, connection,
                                               NOTIFICATIONS_DBUS_PATH, &error);

  if (!exported)
    {
      g_warning ("Failed to export interface: %s", error->message);
      g_error_free (error);
    }
}

static void
name_lost_handler_cb (GDBusConnection *connection,
                      const gchar     *name,
                      gpointer         user_data)
{
}

static void
nd_daemon_constructed (GObject *object)
{
  NdDaemon *daemon;
  GBusNameOwnerFlags flags;

  daemon = ND_DAEMON (object);

  G_OBJECT_CLASS (nd_daemon_parent_class)->constructed (object);

  flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
          G_BUS_NAME_OWNER_FLAGS_REPLACE;

  daemon->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                        NOTIFICATIONS_DBUS_NAME, flags,
                                        bus_acquired_handler_cb, NULL,
                                        name_lost_handler_cb, daemon, NULL);
}

static void
nd_daemon_dispose (GObject *object)
{
  NdDaemon *daemon;

  daemon = ND_DAEMON (object);

  if (daemon->notifications != NULL)
    {
      GDBusInterfaceSkeleton *skeleton;

      skeleton = G_DBUS_INTERFACE_SKELETON (daemon->notifications);
      g_dbus_interface_skeleton_unexport (skeleton);

      g_clear_object (&daemon->notifications);
    }

  if (daemon->bus_name_id > 0)
    {
      g_bus_unown_name (daemon->bus_name_id);
      daemon->bus_name_id = 0;
    }

  g_clear_object (&daemon->queue);

  G_OBJECT_CLASS (nd_daemon_parent_class)->dispose (object);
}

static void
nd_daemon_class_init (NdDaemonClass *daemon_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (daemon_class);

  object_class->constructed = nd_daemon_constructed;
  object_class->dispose = nd_daemon_dispose;
}

static void
nd_daemon_init (NdDaemon *daemon)
{
  daemon->notifications = gf_fd_notifications_gen_skeleton_new ();
  daemon->queue = nd_queue_new ();
}

NdDaemon *
nd_daemon_new (void)
{
  return g_object_new (ND_TYPE_DAEMON, NULL);
}
