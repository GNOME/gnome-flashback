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
#include "gf-screencast.h"

#include <gtk/gtk.h>

#include "dbus/gf-screencast-gen.h"

struct _GfScreencast
{
  GObject          parent;

  gint             bus_name_id;
  GfScreencastGen *screencast_gen;
};

G_DEFINE_TYPE (GfScreencast, gf_screencast, G_TYPE_OBJECT)

static gboolean
handle_screencast (GfScreencastGen       *screencast_gen,
                   GDBusMethodInvocation *invocation,
                   const gchar           *file_template,
                   GVariant              *options,
                   GfScreencast          *self)
{
  g_dbus_method_invocation_return_error_literal (invocation,
                                                 G_DBUS_ERROR,
                                                 G_DBUS_ERROR_NOT_SUPPORTED,
                                                 "Screencast method is not "
                                                 "implemented in GNOME "
                                                 "Flashback!");

  return TRUE;
}

static gboolean
handle_screencast_area (GfScreencastGen       *screencast_gen,
                        GDBusMethodInvocation *invocation,
                        gint                   x,
                        gint                   y,
                        gint                   width,
                        gint                   height,
                        const gchar           *file_template,
                        GVariant              *options,
                        GfScreencast          *self)
{
  g_dbus_method_invocation_return_error_literal (invocation,
                                                 G_DBUS_ERROR,
                                                 G_DBUS_ERROR_NOT_SUPPORTED,
                                                 "ScreencastArea method is "
                                                 "not implemented in GNOME "
                                                 "Flashback!");

  return TRUE;
}

static gboolean
handle_stop_screencast (GfScreencastGen       *screencast_gen,
                        GDBusMethodInvocation *invocation,
                        GfScreencast          *self)
{
  g_dbus_method_invocation_return_error_literal (invocation,
                                                 G_DBUS_ERROR,
                                                 G_DBUS_ERROR_NOT_SUPPORTED,
                                                 "StopScreencast method is "
                                                 "not implemented in GNOME "
                                                 "Flashback!");

  return TRUE;
}

static void
bus_acquired_cb (GDBusConnection *connection,
                 const char      *name,
                 gpointer         user_data)
{
  GfScreencast *self;
  GDBusInterfaceSkeleton *skeleton;
  GError *error;

  self = GF_SCREENCAST (user_data);
  skeleton = G_DBUS_INTERFACE_SKELETON (self->screencast_gen);
  error = NULL;

  if (!g_dbus_interface_skeleton_export (skeleton,
                                         connection,
                                         "/org/gnome/Shell/Screencast",
                                         &error))
    {
      g_warning ("Failed to export interface: %s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (skeleton, "handle-screencast",
                    G_CALLBACK (handle_screencast),
                    self);

  g_signal_connect (skeleton, "handle-screencast-area",
                    G_CALLBACK (handle_screencast_area),
                    self);

  g_signal_connect (skeleton, "handle-stop-screencast",
                    G_CALLBACK (handle_stop_screencast),
                    self);
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
}

static void
name_lost_cb (GDBusConnection *connection,
              const char      *name,
              gpointer         user_data)
{
}

static void
gf_screencast_dispose (GObject *object)
{
  GfScreencast *self;

  self = GF_SCREENCAST (object);

  if (self->bus_name_id != 0)
    {
      g_bus_unown_name (self->bus_name_id);
      self->bus_name_id = 0;
    }

  if (self->screencast_gen)
    {
      GDBusInterfaceSkeleton *skeleton;

      skeleton = G_DBUS_INTERFACE_SKELETON (self->screencast_gen);

      g_dbus_interface_skeleton_unexport (skeleton);
      g_clear_object (&self->screencast_gen);
    }

  G_OBJECT_CLASS (gf_screencast_parent_class)->dispose (object);
}

static void
gf_screencast_class_init (GfScreencastClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_screencast_dispose;
}

static void
gf_screencast_init (GfScreencast *self)
{
  self->screencast_gen = gf_screencast_gen_skeleton_new ();

  self->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                      "org.gnome.Shell.Screencast",
                                      G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                      G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                      bus_acquired_cb,
                                      name_acquired_cb,
                                      name_lost_cb,
                                      self,
                                      NULL);
}

GfScreencast *
gf_screencast_new (void)
{
  return g_object_new (GF_TYPE_SCREENCAST, NULL);
}
