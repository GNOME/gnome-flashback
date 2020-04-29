/*
 * Copyright (C) 2020 Alberts Muktupāvels
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
#include "gf-shell-introspect.h"

#include "dbus/gf-shell-introspect-gen.h"

#define SHELL_INTROSPECT_DBUS_NAME "org.gnome.Shell.Introspect"
#define SHELL_INTROSPECT_DBUS_PATH "/org/gnome/Shell/Introspect"

#define INTROSPECT_DBUS_API_VERSION 2

struct _GfShellIntrospect
{
  GObject               parent;

  GfShellIntrospectGen *introspect;
  gint                  bus_name_id;

  GSettings            *interface_settings;
};

G_DEFINE_TYPE (GfShellIntrospect, gf_shell_introspect, G_TYPE_OBJECT)

static void
enable_animations_changed_cb (GSettings         *settings,
                              const char        *key,
                              GfShellIntrospect *self)
{
  gboolean animations_enabled;

  animations_enabled = g_settings_get_boolean (settings, key);

  gf_shell_introspect_gen_set_animations_enabled (self->introspect,
                                                  animations_enabled);
}

static gboolean
handle_get_running_applications (GfShellIntrospectGen  *object,
                                 GDBusMethodInvocation *invocation,
                                 GfShellIntrospect     *self)
{
  g_dbus_method_invocation_return_error_literal (invocation,
                                                 G_DBUS_ERROR,
                                                 G_DBUS_ERROR_NOT_SUPPORTED,
                                                 "GetRunningApplications method "
                                                 "is not implemented in GNOME "
                                                 "Flashback!");

  return TRUE;
}

static gboolean
handle_get_windows (GfShellIntrospectGen  *object,
                    GDBusMethodInvocation *invocation,
                    GfShellIntrospect     *self)
{
  g_dbus_method_invocation_return_error_literal (invocation,
                                                 G_DBUS_ERROR,
                                                 G_DBUS_ERROR_NOT_SUPPORTED,
                                                 "GetWindows method is not "
                                                 "implemented in GNOME "
                                                 "Flashback!");

  return TRUE;
}

static void
bus_acquired_handler (GDBusConnection *connection,
                      const char      *name,
                      gpointer         user_data)
{
  GfShellIntrospect *self;
  GDBusInterfaceSkeleton *skeleton;
  GError *error;

  self = GF_SHELL_INTROSPECT (user_data);

  skeleton = G_DBUS_INTERFACE_SKELETON (self->introspect);
  error = NULL;

  if (!g_dbus_interface_skeleton_export (skeleton,
                                         connection,
                                         SHELL_INTROSPECT_DBUS_PATH,
                                         &error))
    {
      g_warning ("Failed to export interface: %s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (self->introspect,
                    "handle-get-running-applications",
                    G_CALLBACK (handle_get_running_applications),
                    self);

  g_signal_connect (self->introspect,
                    "handle-get-windows",
                    G_CALLBACK (handle_get_windows),
                    self);

  gf_shell_introspect_gen_set_version (self->introspect,
                                       INTROSPECT_DBUS_API_VERSION);
}

static void
name_acquired_handler (GDBusConnection *connection,
                       const char      *name,
                       gpointer         user_data)
{
}

static void
name_lost_handler (GDBusConnection *connection,
                   const char      *name,
                   gpointer         user_data)
{
}

static void
gf_shell_introspect_dispose (GObject *object)
{
  GfShellIntrospect *self;

  self = GF_SHELL_INTROSPECT (object);

  if (self->bus_name_id != 0)
    {
      g_bus_unown_name (self->bus_name_id);
      self->bus_name_id = 0;
    }

  if (self->introspect != NULL)
    {
      GDBusInterfaceSkeleton *skeleton;

      skeleton = G_DBUS_INTERFACE_SKELETON (self->introspect);
      g_dbus_interface_skeleton_unexport (skeleton);

      g_object_unref (self->introspect);
      self->introspect = NULL;
    }

  g_clear_object (&self->interface_settings);

  G_OBJECT_CLASS (gf_shell_introspect_parent_class)->dispose (object);
}

static void
gf_shell_introspect_class_init (GfShellIntrospectClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_shell_introspect_dispose;
}

static void
gf_shell_introspect_init (GfShellIntrospect *self)
{
  self->introspect = gf_shell_introspect_gen_skeleton_new ();

  self->interface_settings = g_settings_new ("org.gnome.desktop.interface");

  g_signal_connect (self->interface_settings,
                    "changed::enable-animations",
                    G_CALLBACK (enable_animations_changed_cb),
                    self);

  enable_animations_changed_cb (self->interface_settings,
                                "enable-animations",
                                self);

  self->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                      SHELL_INTROSPECT_DBUS_NAME,
                                      G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                      G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                      bus_acquired_handler,
                                      name_acquired_handler,
                                      name_lost_handler,
                                      self,
                                      NULL);
}

GfShellIntrospect *
gf_shell_introspect_new (void)
{
  return g_object_new (GF_TYPE_SHELL_INTROSPECT, NULL);
}
