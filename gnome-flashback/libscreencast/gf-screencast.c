/*
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

#include <config.h>
#include <gtk/gtk.h>
#include "flashback-dbus-screencast.h"
#include "gf-screencast.h"

#define SHELL_DBUS_NAME "org.gnome.Shell"
#define SCREENCAST_DBUS_PATH "/org/gnome/Shell/Screencast"

struct _FlashbackScreencast
{
  GObject                  parent;

  gint                     bus_name;
  GDBusInterfaceSkeleton  *iface;
};

G_DEFINE_TYPE (FlashbackScreencast, flashback_screencast, G_TYPE_OBJECT)

static gboolean
handle_screencast (FlashbackDBusScreencast *dbus_screencast,
                   GDBusMethodInvocation   *invocation,
                   const gchar             *file_template,
                   GVariant                *options,
                   gpointer                 user_data)
{
  g_warning ("screencast: screencast");
  flashback_dbus_screencast_complete_screencast (dbus_screencast, invocation,
                                                 FALSE, "");

  return TRUE;
}

static gboolean
handle_screencast_area (FlashbackDBusScreencast *dbus_screencast,
                        GDBusMethodInvocation   *invocation,
                        gint                     x,
                        gint                     y,
                        gint                     width,
                        gint                     height,
                        const gchar             *file_template,
                        GVariant                *options,
                        gpointer                 user_data)
{
  g_warning ("screencast: screencast-area");
  flashback_dbus_screencast_complete_screencast_area (dbus_screencast, invocation,
                                                      FALSE, "");

  return TRUE;
}

static gboolean
handle_stop_screencast (FlashbackDBusScreencast *dbus_screencast,
                        GDBusMethodInvocation   *invocation,
                        gpointer                 user_data)
{
  g_warning ("screencast: stop-screencast");
  flashback_dbus_screencast_complete_stop_screencast (dbus_screencast, invocation,
                                                      TRUE);

  return TRUE;
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  FlashbackScreencast *screencast;
  FlashbackDBusScreencast *skeleton;
  GError *error;

  screencast = FLASHBACK_SCREENCAST (user_data);
  skeleton = flashback_dbus_screencast_skeleton_new ();

  g_signal_connect (skeleton, "handle-screencast",
                    G_CALLBACK (handle_screencast), screencast);
  g_signal_connect (skeleton, "handle-screencast-area",
                    G_CALLBACK (handle_screencast_area), screencast);
  g_signal_connect (skeleton, "handle-stop-screencast",
                    G_CALLBACK (handle_stop_screencast), screencast);

  error = NULL;
  screencast->iface = G_DBUS_INTERFACE_SKELETON (skeleton);

	if (!g_dbus_interface_skeleton_export (screencast->iface, connection,
	                                       SCREENCAST_DBUS_PATH,
	                                       &error))
  {
    g_warning ("Failed to export interface: %s", error->message);
    g_error_free (error);
    return;
  }
}

static void
flashback_screencast_finalize (GObject *object)
{
  FlashbackScreencast *screencast;

  screencast = FLASHBACK_SCREENCAST (object);

  if (screencast->bus_name)
    {
      g_bus_unwatch_name (screencast->bus_name);
      screencast->bus_name = 0;
    }

  G_OBJECT_CLASS (flashback_screencast_parent_class)->finalize (object);
}

static void
flashback_screencast_class_init (FlashbackScreencastClass *screencast_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (screencast_class);

  object_class->finalize = flashback_screencast_finalize;
}

static void
flashback_screencast_init (FlashbackScreencast *screencast)
{
  screencast->bus_name = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                           SHELL_DBUS_NAME,
                                           G_BUS_NAME_WATCHER_FLAGS_NONE,
                                           name_appeared_handler,
                                           NULL,
                                           screencast,
                                           NULL);
}

FlashbackScreencast *
flashback_screencast_new (void)
{
	return g_object_new (FLASHBACK_TYPE_SCREENCAST, NULL);
}
