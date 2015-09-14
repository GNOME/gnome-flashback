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

#include "config.h"

#include <gtk/gtk.h>

#include "gf-dbus-screencast.h"
#include "gf-screencast.h"

#define SCREENCAST_DBUS_NAME "org.gnome.Shell.Screencast"
#define SCREENCAST_DBUS_PATH "/org/gnome/Shell/Screencast"

struct _GfScreencast
{
  GObject                 parent;

  gint                    bus_name;
  GDBusInterfaceSkeleton *iface;
};

G_DEFINE_TYPE (GfScreencast, gf_screencast, G_TYPE_OBJECT)

static gboolean
handle_screencast (GfDBusScreencast      *dbus_screencast,
                   GDBusMethodInvocation *invocation,
                   const gchar           *file_template,
                   GVariant              *options,
                   gpointer               user_data)
{
  g_warning ("screencast: screencast");
  gf_dbus_screencast_complete_screencast (dbus_screencast, invocation,
                                          FALSE, "");

  return TRUE;
}

static gboolean
handle_screencast_area (GfDBusScreencast      *dbus_screencast,
                        GDBusMethodInvocation *invocation,
                        gint                   x,
                        gint                   y,
                        gint                   width,
                        gint                   height,
                        const gchar           *file_template,
                        GVariant              *options,
                        gpointer               user_data)
{
  g_warning ("screencast: screencast-area");
  gf_dbus_screencast_complete_screencast_area (dbus_screencast, invocation,
                                               FALSE, "");

  return TRUE;
}

static gboolean
handle_stop_screencast (GfDBusScreencast      *dbus_screencast,
                        GDBusMethodInvocation *invocation,
                        gpointer               user_data)
{
  g_warning ("screencast: stop-screencast");
  gf_dbus_screencast_complete_stop_screencast (dbus_screencast, invocation,
                                               TRUE);

  return TRUE;
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  GfScreencast *screencast;
  GfDBusScreencast *skeleton;
  GError *error;

  screencast = GF_SCREENCAST (user_data);
  skeleton = gf_dbus_screencast_skeleton_new ();

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
gf_screencast_finalize (GObject *object)
{
  GfScreencast *screencast;

  screencast = GF_SCREENCAST (object);

  if (screencast->bus_name)
    {
      g_bus_unwatch_name (screencast->bus_name);
      screencast->bus_name = 0;
    }

  G_OBJECT_CLASS (gf_screencast_parent_class)->finalize (object);
}

static void
gf_screencast_class_init (GfScreencastClass *screencast_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (screencast_class);

  object_class->finalize = gf_screencast_finalize;
}

static void
gf_screencast_init (GfScreencast *screencast)
{
  screencast->bus_name = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                           SCREENCAST_DBUS_NAME,
                                           G_BUS_NAME_WATCHER_FLAGS_NONE,
                                           name_appeared_handler,
                                           NULL,
                                           screencast,
                                           NULL);
}

GfScreencast *
gf_screencast_new (void)
{
  return g_object_new (GF_TYPE_SCREENCAST, NULL);
}
