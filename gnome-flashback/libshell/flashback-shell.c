/*
 * Copyright (C) 2015-2020 Alberts Muktupāvels
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
#include "flashback-shell.h"

#include <gtk/gtk.h>
#include <libcommon/gf-keybindings.h>

#include "dbus/gf-shell-gen.h"
#include "flashback-monitor-labeler.h"
#include "flashback-osd.h"
#include "gf-shell-introspect.h"

#define SHELL_DBUS_NAME "org.gnome.Shell"
#define SHELL_DBUS_PATH "/org/gnome/Shell"

typedef struct
{
	const gchar    *sender;
	FlashbackShell *shell;
} GrabberData;

struct _FlashbackShell
{
  GObject                  parent;

  gint                     bus_name;
  GDBusInterfaceSkeleton  *iface;

  /* key-grabber */
  GfKeybindings           *keybindings;
  GHashTable              *grabbed_accelerators;
  GHashTable              *grabbers;

  /* monitor labeler */
  GfMonitorManager        *monitor_manager;
  FlashbackMonitorLabeler *labeler;

  /* osd */
  FlashbackOsd            *osd;

  GfShellIntrospect       *introspect;
};

G_DEFINE_TYPE (FlashbackShell, flashback_shell, G_TYPE_OBJECT)

static GVariant *
build_parameters (const gchar *device_node,
                  guint        device_id,
                  guint        timestamp,
                  guint        action_mode)
{
  GVariantBuilder *builder;
  GVariant *parameters;

  builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));

  if (device_node != NULL)
      g_variant_builder_add (builder, "{sv}", "device-node",
                             g_variant_new_string (device_node));

  g_variant_builder_add (builder, "{sv}", "device-id",
                         g_variant_new_uint32 (device_id));
  g_variant_builder_add (builder, "{sv}", "timestamp",
                         g_variant_new_uint32 (timestamp));
  g_variant_builder_add (builder, "{sv}", "action-mode",
                         g_variant_new_uint32 (action_mode));

  parameters = g_variant_new ("a{sv}", builder);
  g_variant_builder_unref (builder);

  return parameters;
}

static void
accelerator_activated (GfKeybindings *keybindings,
                       guint          action,
                       const gchar   *device_node,
                       guint          device_id,
                       guint          timestamp,
                       gpointer       user_data)
{
	FlashbackShell *shell;
	GfShellGen *shell_gen;
	GVariant *parameters;

	shell = FLASHBACK_SHELL (user_data);
	shell_gen = GF_SHELL_GEN (shell->iface);
	parameters = build_parameters (device_node, device_id, timestamp, 0);

	gf_shell_gen_emit_accelerator_activated (shell_gen, action, parameters);
}

static gint
real_grab (FlashbackShell *shell,
           const gchar    *accelerator,
           guint           mode_flags,
           guint           grab_flags)
{
  return gf_keybindings_grab (shell->keybindings, accelerator);
}

static gboolean
real_ungrab (FlashbackShell *shell,
             gint            action)
{
  return gf_keybindings_ungrab (shell->keybindings, action);
}

static void
remove_watch (gpointer key,
              gpointer value,
              gpointer user_data)
{
  g_bus_unwatch_name (GPOINTER_TO_UINT (value));
}

static gboolean
remove_accelerator (gpointer key,
                    gpointer value,
                    gpointer user_data)
{
  guint action;
  gchar *sender;
  GrabberData *data;

  action = GPOINTER_TO_UINT (key);
  sender = (gchar *) value;
  data = (GrabberData *) user_data;

  if (g_str_equal (sender, data->sender))
    {
      if (real_ungrab (data->shell, action))
        return TRUE;
    }

  return FALSE;
}

static void
name_vanished_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  FlashbackShell *shell;
  guint id;
  GrabberData *data;

  shell = FLASHBACK_SHELL (user_data);
  id = GPOINTER_TO_UINT (g_hash_table_lookup (shell->grabbers, name));
  data = g_new0 (GrabberData, 1);

  data->sender = name;
  data->shell = shell;

  g_hash_table_foreach_remove (shell->grabbed_accelerators,
                               remove_accelerator, data);
  g_free (data);

  g_bus_unwatch_name (id);
  g_hash_table_remove (shell->grabbers, name);
}

static guint
grab_accelerator (FlashbackShell *shell,
                  const gchar    *accelerator,
                  guint           mode_flags,
                  guint           grab_flags,
                  const gchar    *sender)
{
  guint action;

  action = real_grab (shell, accelerator, mode_flags, grab_flags);
  g_hash_table_insert (shell->grabbed_accelerators,
                       GUINT_TO_POINTER (action), g_strdup (sender));

  if (g_hash_table_lookup (shell->grabbers, sender) == NULL)
    {
      guint id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                   sender,
                                   G_BUS_NAME_WATCHER_FLAGS_NONE,
                                   NULL,
                                   (GBusNameVanishedCallback) name_vanished_handler,
                                   shell,
                                   NULL);
      g_hash_table_insert (shell->grabbers, g_strdup (sender), GUINT_TO_POINTER (id));
    }

  return action;
}

static gboolean
ungrab_accelerator (FlashbackShell *shell,
                    guint           action,
                    const gchar    *sender)
{
  const gchar *grabbed_by;
  gboolean success;

  grabbed_by = g_hash_table_lookup (shell->grabbed_accelerators,
                                    GUINT_TO_POINTER (action));

  if (g_strcmp0 (grabbed_by, sender) != 0)
    return FALSE;

  success = real_ungrab (shell, action);

  if (success)
    g_hash_table_remove (shell->grabbed_accelerators, GUINT_TO_POINTER (action));

  return success;
}

static gboolean
handle_eval (GfShellGen            *shell_gen,
             GDBusMethodInvocation *invocation,
             const gchar            action,
             FlashbackShell        *shell)
{
  gf_shell_gen_complete_eval (shell_gen, invocation, FALSE, "");

  return TRUE;
}

static gboolean
handle_focus_search (GfShellGen            *shell_gen,
                     GDBusMethodInvocation *invocation,
                     FlashbackShell        *shell)
{
  gf_shell_gen_complete_focus_search (shell_gen, invocation);

  return TRUE;
}

static gboolean
handle_show_osd (GfShellGen            *shell_gen,
                 GDBusMethodInvocation *invocation,
                 GVariant              *params,
                 FlashbackShell        *shell)
{
  flashback_osd_show (shell->osd, shell->monitor_manager, params);

  gf_shell_gen_complete_show_osd (shell_gen, invocation);

  return TRUE;
}

static gboolean
handle_show_monitor_labels (GfShellGen            *shell_gen,
                            GDBusMethodInvocation *invocation,
                            GVariant              *params,
                            FlashbackShell        *shell)
{
  const gchar *sender;

  sender = g_dbus_method_invocation_get_sender (invocation);

  g_assert (shell->monitor_manager != NULL);

  flashback_monitor_labeler_show (shell->labeler, shell->monitor_manager,
                                  sender, params);

  gf_shell_gen_complete_show_monitor_labels (shell_gen, invocation);

  return TRUE;
}

static gboolean
handle_hide_monitor_labels (GfShellGen            *shell_gen,
                            GDBusMethodInvocation *invocation,
                            FlashbackShell        *shell)
{
  const gchar *sender;

  sender = g_dbus_method_invocation_get_sender (invocation);

  flashback_monitor_labeler_hide (shell->labeler, sender);

  gf_shell_gen_complete_hide_monitor_labels (shell_gen, invocation);

  return TRUE;
}

static gboolean
handle_focus_app (GfShellGen            *shell_gen,
                  GDBusMethodInvocation *invocation,
                  const gchar            id,
                  FlashbackShell        *shell)
{
  gf_shell_gen_complete_focus_app (shell_gen, invocation);

  return TRUE;
}

static gboolean
handle_show_applications (GfShellGen            *shell_gen,
                          GDBusMethodInvocation *invocation,
                          FlashbackShell        *shell)
{
  gf_shell_gen_complete_show_applications (shell_gen, invocation);

  return TRUE;
}

static gboolean
handle_grab_accelerator (GfShellGen            *shell_gen,
                         GDBusMethodInvocation *invocation,
                         const gchar           *accelerator,
                         guint                  mode_flags,
                         guint                  grab_flags,
                         FlashbackShell        *shell)
{
  const gchar *sender;
  guint action;

  sender = g_dbus_method_invocation_get_sender (invocation);
  action = grab_accelerator (shell, accelerator, mode_flags, grab_flags, sender);

  gf_shell_gen_complete_grab_accelerator (shell_gen, invocation, action);

  return TRUE;
}

static gboolean
handle_grab_accelerators (GfShellGen            *shell_gen,
                          GDBusMethodInvocation *invocation,
                          GVariant              *accelerators,
                          FlashbackShell        *shell)
{
  GVariantBuilder builder;
  GVariantIter iter;
  GVariant *child;
  const gchar *sender;

  g_variant_builder_init (&builder, G_VARIANT_TYPE("au"));
  g_variant_iter_init (&iter, accelerators);

  sender = g_dbus_method_invocation_get_sender (invocation);

  while ((child = g_variant_iter_next_value (&iter)))
    {
      gchar *accelerator;
      guint mode_flags;
      guint grab_flags;
      guint action;

      g_variant_get (child, "(suu)", &accelerator, &mode_flags, &grab_flags);

      action = grab_accelerator (shell, accelerator, mode_flags, grab_flags, sender);
      g_variant_builder_add (&builder, "u", action);

      g_free (accelerator);
      g_variant_unref (child);
    }

  gf_shell_gen_complete_grab_accelerators (shell_gen,
                                           invocation,
                                           g_variant_builder_end (&builder));

  return TRUE;
}

static gboolean
handle_ungrab_accelerator (GfShellGen            *shell_gen,
                           GDBusMethodInvocation *invocation,
                           guint                  action,
                           FlashbackShell        *shell)
{
  const gchar *sender;
  gboolean success;

  sender = g_dbus_method_invocation_get_sender (invocation);
  success = ungrab_accelerator (shell, action, sender);

  gf_shell_gen_complete_ungrab_accelerator (shell_gen, invocation, success);

  return TRUE;
}

static gboolean
handle_ungrab_accelerators (GfShellGen            *shell_gen,
                            GDBusMethodInvocation *invocation,
                            GVariant              *actions,
                            FlashbackShell        *shell)
{
  const char *sender;
  gboolean success;
  GVariantIter iter;
  GVariant *child;

  sender = g_dbus_method_invocation_get_sender (invocation);
  success = TRUE;

  g_variant_iter_init (&iter, actions);
  while ((child = g_variant_iter_next_value (&iter)))
    {
      guint action;

      g_variant_get (child, "u", &action);
      g_variant_unref (child);

      success &= ungrab_accelerator (shell, action, sender);
    }

  gf_shell_gen_complete_ungrab_accelerators (shell_gen, invocation, success);

  return TRUE;
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  FlashbackShell *shell;
  GfShellGen *skeleton;
  GError *error;

  shell = FLASHBACK_SHELL (user_data);
  skeleton = gf_shell_gen_skeleton_new ();

  g_signal_connect (skeleton, "handle-eval",
                    G_CALLBACK (handle_eval), shell);
  g_signal_connect (skeleton, "handle-focus-search",
                    G_CALLBACK (handle_focus_search), shell);
  g_signal_connect (skeleton, "handle-show-osd",
                    G_CALLBACK (handle_show_osd), shell);
  g_signal_connect (skeleton, "handle-show-monitor-labels",
                    G_CALLBACK (handle_show_monitor_labels), shell);
  g_signal_connect (skeleton, "handle-hide-monitor-labels",
                    G_CALLBACK (handle_hide_monitor_labels), shell);
  g_signal_connect (skeleton, "handle-focus-app",
                    G_CALLBACK (handle_focus_app), shell);
  g_signal_connect (skeleton, "handle-show-applications",
                    G_CALLBACK (handle_show_applications), shell);
  g_signal_connect (skeleton, "handle-grab-accelerator",
                    G_CALLBACK (handle_grab_accelerator), shell);
  g_signal_connect (skeleton, "handle-grab-accelerators",
                    G_CALLBACK (handle_grab_accelerators), shell);
  g_signal_connect (skeleton, "handle-ungrab-accelerator",
                    G_CALLBACK (handle_ungrab_accelerator), shell);
  g_signal_connect (skeleton, "handle-ungrab-accelerators",
                    G_CALLBACK (handle_ungrab_accelerators), shell);

  gf_shell_gen_set_mode (skeleton, "");
  gf_shell_gen_set_overview_active (skeleton, FALSE);
  gf_shell_gen_set_shell_version (skeleton, "");

  error = NULL;
  shell->iface = G_DBUS_INTERFACE_SKELETON (skeleton);

	if (!g_dbus_interface_skeleton_export (shell->iface, connection,
	                                       SHELL_DBUS_PATH,
	                                       &error))
  {
    g_warning ("Failed to export interface: %s", error->message);
    g_error_free (error);
    return;
  }
}

static void
flashback_shell_finalize (GObject *object)
{
  FlashbackShell *shell;

  shell = FLASHBACK_SHELL (object);

  if (shell->bus_name)
    {
      g_bus_unwatch_name (shell->bus_name);
      shell->bus_name = 0;
    }

  if (shell->iface != NULL)
    {
      g_dbus_interface_skeleton_unexport (shell->iface);

      g_object_unref (shell->iface);
      shell->iface = NULL;
    }

  if (shell->grabbed_accelerators)
    {
      g_hash_table_destroy (shell->grabbed_accelerators);
      shell->grabbed_accelerators = NULL;
    }

  if (shell->grabbers)
    {
      g_hash_table_foreach (shell->grabbers, remove_watch, NULL);
      g_hash_table_destroy (shell->grabbers);
      shell->grabbers = NULL;
    }

  g_clear_object (&shell->keybindings);
  g_clear_object (&shell->labeler);
  g_clear_object (&shell->osd);
  g_clear_object (&shell->introspect);

  G_OBJECT_CLASS (flashback_shell_parent_class)->finalize (object);
}

static void
flashback_shell_class_init (FlashbackShellClass *shell_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (shell_class);

  object_class->finalize = flashback_shell_finalize;
}

static void
flashback_shell_init (FlashbackShell *shell)
{
  shell->grabbed_accelerators = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  shell->grabbers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  shell->keybindings = gf_keybindings_new ();

  g_signal_connect (shell->keybindings, "accelerator-activated",
                    G_CALLBACK (accelerator_activated), shell);

  shell->labeler = flashback_monitor_labeler_new ();
  shell->osd = flashback_osd_new ();

  shell->introspect = gf_shell_introspect_new ();

  shell->bus_name = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                      SHELL_DBUS_NAME,
                                      G_BUS_NAME_WATCHER_FLAGS_NONE,
                                      name_appeared_handler,
                                      NULL,
                                      shell,
                                      NULL);
}

FlashbackShell *
flashback_shell_new (void)
{
	return g_object_new (FLASHBACK_TYPE_SHELL, NULL);
}

void
flashback_shell_set_monitor_manager (FlashbackShell   *shell,
                                     GfMonitorManager *monitor_manager)
{
  shell->monitor_manager = monitor_manager;
}
