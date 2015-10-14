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
#include <libcommon/gf-keybindings.h>
#include <libdisplay-config/flashback-display-config.h>
#include "flashback-dbus-shell.h"
#include "flashback-monitor-labeler.h"
#include "flashback-osd.h"
#include "flashback-shell.h"

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
  FlashbackMonitorLabeler *labeler;
  FlashbackMonitorManager *manager;

  /* osd */
  FlashbackOsd            *osd;
};

G_DEFINE_TYPE (FlashbackShell, flashback_shell, G_TYPE_OBJECT)

static GVariant *
build_parameters (guint device_id,
                  guint timestamp,
                  guint action_mode)
{
  GVariantBuilder *builder;
  GVariant *parameters;

  builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));

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
                       gpointer       user_data)
{
	FlashbackShell *shell;
	FlashbackDBusShell *dbus_shell;
	GVariant *parameters;

	shell = FLASHBACK_SHELL (user_data);
	dbus_shell = FLASHBACK_DBUS_SHELL (shell->iface);
	parameters = build_parameters (0, 0, 0);

	flashback_dbus_shell_emit_accelerator_activated (dbus_shell, action, parameters);
}

static gint
real_grab (FlashbackShell *shell,
           const gchar    *accelerator)
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
ungrab_accelerator (gpointer key,
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
        g_hash_table_remove (data->shell->grabbed_accelerators, key);
    }
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

  g_hash_table_foreach (shell->grabbed_accelerators, (GHFunc) ungrab_accelerator, data);
  g_free (data);

  g_bus_unwatch_name (id);
  g_hash_table_remove (shell->grabbers, name);
}

static guint
grab_accelerator (FlashbackShell *shell,
                  const gchar    *accelerator,
                  guint           flags,
                  const gchar    *sender)
{
  guint action;

  action = real_grab (shell, accelerator);
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
handle_eval (FlashbackDBusShell    *dbus_shell,
             GDBusMethodInvocation *invocation,
             const gchar            action,
             gpointer               user_data)
{
  flashback_dbus_shell_complete_eval (dbus_shell, invocation,
                                      FALSE, "");

  return TRUE;
}

static gboolean
handle_focus_search (FlashbackDBusShell    *dbus_shell,
                     GDBusMethodInvocation *invocation,
                     gpointer               user_data)
{
  flashback_dbus_shell_complete_focus_search (dbus_shell, invocation);

  return TRUE;
}

static gboolean
handle_show_osd (FlashbackDBusShell    *dbus_shell,
                 GDBusMethodInvocation *invocation,
                 GVariant              *params,
                 gpointer               user_data)
{
  FlashbackShell *shell;

  shell = FLASHBACK_SHELL (user_data);

  flashback_osd_show (shell->osd, params);

  flashback_dbus_shell_complete_show_osd (dbus_shell, invocation);

  return TRUE;
}

static gboolean
handle_show_monitor_labels (FlashbackDBusShell    *dbus_shell,
                            GDBusMethodInvocation *invocation,
                            GVariant              *params,
                            gpointer               user_data)
{
  FlashbackShell *shell;
  const gchar *sender;

  shell = FLASHBACK_SHELL (user_data);
  sender = g_dbus_method_invocation_get_sender (invocation);

  g_assert (shell->manager != NULL);

  flashback_monitor_labeler_show (shell->labeler, shell->manager,
                                  sender, params);

  flashback_dbus_shell_complete_show_monitor_labels (dbus_shell, invocation);

  return TRUE;
}

static gboolean
handle_hide_monitor_labels (FlashbackDBusShell    *dbus_shell,
                            GDBusMethodInvocation *invocation,
                            gpointer               user_data)
{
  FlashbackShell *shell;
  const gchar *sender;

  shell = FLASHBACK_SHELL (user_data);
  sender = g_dbus_method_invocation_get_sender (invocation);

  flashback_monitor_labeler_hide (shell->labeler, sender);

  flashback_dbus_shell_complete_hide_monitor_labels (dbus_shell, invocation);

  return TRUE;
}

static gboolean
handle_focus_app (FlashbackDBusShell    *dbus_shell,
                  GDBusMethodInvocation *invocation,
                  const gchar            id,
                  gpointer               user_data)
{
  flashback_dbus_shell_complete_focus_app (dbus_shell, invocation);

  return TRUE;
}

static gboolean
handle_show_applications (FlashbackDBusShell    *dbus_shell,
                          GDBusMethodInvocation *invocation,
                          gpointer               user_data)
{
  flashback_dbus_shell_complete_show_applications (dbus_shell, invocation);

  return TRUE;
}

static gboolean
handle_grab_accelerator (FlashbackDBusShell    *dbus_shell,
                         GDBusMethodInvocation *invocation,
                         const gchar           *accelerator,
                         guint                  flags,
                         gpointer               user_data)
{
  FlashbackShell *shell;
  const gchar *sender;
  guint action;

  shell = FLASHBACK_SHELL (user_data);
  sender = g_dbus_method_invocation_get_sender (invocation);
  action = grab_accelerator (shell, accelerator, flags, sender);

  flashback_dbus_shell_complete_grab_accelerator (dbus_shell, invocation, action);

  return TRUE;
}

static gboolean
handle_grab_accelerators (FlashbackDBusShell    *dbus_shell,
                          GDBusMethodInvocation *invocation,
                          GVariant              *accelerators,
                          gpointer               user_data)
{
  FlashbackShell *shell;
  GVariantBuilder builder;
  GVariantIter iter;
  GVariant *child;
  const gchar *sender;

  shell = FLASHBACK_SHELL (user_data);

  g_variant_builder_init (&builder, G_VARIANT_TYPE("au"));
  g_variant_iter_init (&iter, accelerators);

  sender = g_dbus_method_invocation_get_sender (invocation);

  while ((child = g_variant_iter_next_value (&iter)))
    {
      gchar *accelerator;
      guint flags;
      guint action;

      g_variant_get (child, "(su)", &accelerator, &flags);

      action = grab_accelerator (shell, accelerator, flags, sender);
      g_variant_builder_add (&builder, "u", action);

      g_free (accelerator);
      g_variant_unref (child);
    }

  flashback_dbus_shell_complete_grab_accelerators (dbus_shell, invocation,
                                                   g_variant_builder_end (&builder));

  return TRUE;
}

static gboolean
handle_ungrab_accelerator (FlashbackDBusShell    *dbus_shell,
                           GDBusMethodInvocation *invocation,
                           guint                  action,
                           gpointer               user_data)
{
  FlashbackShell *shell;
  gchar *sender;
	gboolean success;

  shell = FLASHBACK_SHELL (user_data);
  success = FALSE;
  sender = (gchar *) g_hash_table_lookup (shell->grabbed_accelerators,
                                          GUINT_TO_POINTER (action));

  if (g_str_equal (sender, g_dbus_method_invocation_get_sender (invocation)))
    {
      success = real_ungrab (shell, action);

      if (success)
        g_hash_table_remove (shell->grabbed_accelerators, GUINT_TO_POINTER (action));
    }

  flashback_dbus_shell_complete_ungrab_accelerator (dbus_shell, invocation, success);

  return TRUE;
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  FlashbackShell *shell;
  FlashbackDBusShell *skeleton;
  GError *error;

  shell = FLASHBACK_SHELL (user_data);
  skeleton = flashback_dbus_shell_skeleton_new ();

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

  flashback_dbus_shell_set_mode (skeleton, "");
  flashback_dbus_shell_set_overview_active (skeleton, FALSE);
  flashback_dbus_shell_set_shell_version (skeleton, "");

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

  if (shell->grabbed_accelerators)
    {
      g_hash_table_destroy (shell->grabbed_accelerators);
      shell->grabbed_accelerators = NULL;
    }

  if (shell->grabbers)
    {
      g_hash_table_destroy (shell->grabbers);
      shell->grabbers = NULL;
    }

  g_clear_object (&shell->keybindings);
  g_clear_object (&shell->labeler);
  g_clear_object (&shell->osd);

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

  shell->keybindings = gf_keybindings_new (FALSE);
  g_signal_connect (shell->keybindings, "accelerator-activated",
                    G_CALLBACK (accelerator_activated), shell);

  shell->labeler = flashback_monitor_labeler_new ();
  shell->osd = flashback_osd_new ();

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
flashback_shell_set_display_config (FlashbackShell         *shell,
                                    FlashbackDisplayConfig *config)
{
  shell->manager = flashback_display_config_get_monitor_manager (config);
}
