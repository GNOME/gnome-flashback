/*
 * Copyright (C) 2014-2017 Alberts Muktupāvels
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gf-application.h"
#include "gf-confirm-display-change-dialog.h"
#include "backends/gf-backend.h"
#include "liba11y-keyboard/gf-a11y-keyboard.h"
#include "libaudio-device-selection/gf-audio-device-selection.h"
#include "libautomount-manager/gsd-automount-manager.h"
#include "libbluetooth-applet/gf-bluetooth-applet.h"
#include "libclipboard/gf-clipboard.h"
#include "libdesktop/gf-desktop.h"
#include "libend-session-dialog/gf-end-session-dialog.h"
#include "libidle-monitor/flashback-idle-monitor.h"
#include "libinput-settings/gf-input-settings.h"
#include "libinput-sources/gf-input-sources.h"
#include "libnotifications/gf-notifications.h"
#include "libpolkit/flashback-polkit.h"
#include "libpower-applet/gf-power-applet.h"
#include "libscreencast/gf-screencast.h"
#include "libscreensaver/gf-screensaver.h"
#include "libscreenshot/gf-screenshot.h"
#include "libshell/flashback-shell.h"
#include "libsound-applet/gf-sound-applet.h"
#include "libstatus-notifier-watcher/gf-status-notifier-watcher.h"

struct _GfApplication
{
  GObject                  parent;

  GfBackend               *backend;

  gint                     bus_name;

  GSettings               *settings;

  GtkStyleProvider        *provider;

  GsdAutomountManager     *automount;
  FlashbackIdleMonitor    *idle_monitor;
  FlashbackPolkit         *polkit;
  FlashbackShell          *shell;
  GfA11yKeyboard          *a11y_keyboard;
  GfAudioDeviceSelection  *audio_device_selection;
  GfBluetoothApplet       *bluetooth;
  GfClipboard             *clipboard;
  GfDesktop               *desktop;
  GfEndSessionDialog      *dialog;
  GfInputSettings         *input_settings;
  GfInputSources          *input_sources;
  GfNotifications         *notifications;
  GfPowerApplet           *power;
  GfScreencast            *screencast;
  GfScreensaver           *screensaver;
  GfScreenshot            *screenshot;
  GfSoundApplet           *sound;
  GfStatusNotifierWatcher *status_notifier_watcher;

  GtkWidget               *display_change_dialog;
};

G_DEFINE_TYPE (GfApplication, gf_application, G_TYPE_OBJECT)

static void
theme_changed (GtkSettings *settings,
               GParamSpec  *pspec,
               gpointer     user_data)
{
  GfApplication *application;
  GdkScreen *screen;
  gchar *theme_name;
  gboolean dark_theme;
  guint priority;
  gchar *resource;
  GtkCssProvider *css;

  application = GF_APPLICATION (user_data);
  screen = gdk_screen_get_default ();

  if (application->provider != NULL)
    {
      gtk_style_context_remove_provider_for_screen (screen, application->provider);
      g_clear_object (&application->provider);
    }

  g_object_get (settings, "gtk-theme-name", &theme_name, NULL);

  if (g_strcmp0 (theme_name, "Adwaita") != 0 &&
      g_strcmp0 (theme_name, "HighContrast") != 0)
    {
      g_free (theme_name);
      return;
    }

  g_object_get (settings,
                "gtk-application-prefer-dark-theme", &dark_theme,
                NULL);

  priority = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION;
  resource = g_strdup_printf ("/org/gnome/gnome-flashback/theme/%s/gnome-flashback%s.css",
                              theme_name, dark_theme ? "-dark" : "");

  css = gtk_css_provider_new ();
  application->provider =  GTK_STYLE_PROVIDER (css);

  gtk_css_provider_load_from_resource (css, resource);
  gtk_style_context_add_provider_for_screen (screen, application->provider,
                                             priority);

  g_free (theme_name);
  g_free (resource);
}

static void
settings_changed (GSettings   *settings,
                  const gchar *key,
                  gpointer     user_data)
{
  GfApplication *application;
  GfMonitorManager *monitor_manager;

  application = GF_APPLICATION (user_data);
  monitor_manager = gf_backend_get_monitor_manager (application->backend);

#define SETTING_CHANGED(variable_name, setting_name, function_name) \
  if (key == NULL || g_strcmp0 (key, setting_name) == 0)            \
    {                                                               \
      if (g_settings_get_boolean (settings, setting_name))          \
        {                                                           \
          if (application->variable_name == NULL)                   \
            application->variable_name = function_name ();          \
        }                                                           \
      else                                                          \
        {                                                           \
          g_clear_object (&application->variable_name);             \
        }                                                           \
    }

  SETTING_CHANGED (automount, "automount-manager", gsd_automount_manager_new)
  SETTING_CHANGED (idle_monitor, "idle-monitor", flashback_idle_monitor_new)
  SETTING_CHANGED (polkit, "polkit", flashback_polkit_new)
  SETTING_CHANGED (shell, "shell", flashback_shell_new)
  SETTING_CHANGED (a11y_keyboard, "a11y-keyboard", gf_a11y_keyboard_new)
  SETTING_CHANGED (audio_device_selection, "audio-device-selection", gf_audio_device_selection_new)
  SETTING_CHANGED (bluetooth, "bluetooth-applet", gf_bluetooth_applet_new)
  SETTING_CHANGED (desktop, "desktop", gf_desktop_new)
  SETTING_CHANGED (clipboard, "clipboard", gf_clipboard_new)
  SETTING_CHANGED (dialog, "end-session-dialog", gf_end_session_dialog_new)
  SETTING_CHANGED (input_settings, "input-settings", gf_input_settings_new)
  SETTING_CHANGED (input_sources, "input-sources", gf_input_sources_new)
  SETTING_CHANGED (notifications, "notifications", gf_notifications_new)
  SETTING_CHANGED (power, "power-applet", gf_power_applet_new)
  SETTING_CHANGED (screencast, "screencast", gf_screencast_new)
  SETTING_CHANGED (screensaver, "screensaver", gf_screensaver_new)
  SETTING_CHANGED (screenshot, "screenshot", gf_screenshot_new)
  SETTING_CHANGED (sound, "sound-applet", gf_sound_applet_new)
  SETTING_CHANGED (status_notifier_watcher, "status-notifier-watcher", gf_status_notifier_watcher_new)

#undef SETTING_CHANGED

  if (application->input_settings)
    gf_input_settings_set_monitor_manager (application->input_settings,
                                           monitor_manager);

  if (application->shell)
    flashback_shell_set_monitor_manager (application->shell, monitor_manager);
}

static void
gf_application_dispose (GObject *object)
{
  GfApplication *application;

  application = GF_APPLICATION (object);

  if (application->bus_name)
    {
      g_bus_unown_name (application->bus_name);
      application->bus_name = 0;
    }

  g_clear_object (&application->settings);

  g_clear_object (&application->provider);

  g_clear_object (&application->automount);
  g_clear_object (&application->idle_monitor);
  g_clear_object (&application->polkit);
  g_clear_object (&application->shell);
  g_clear_object (&application->a11y_keyboard);
  g_clear_object (&application->audio_device_selection);
  g_clear_object (&application->bluetooth);
  g_clear_object (&application->desktop);
  g_clear_object (&application->clipboard);
  g_clear_object (&application->dialog);
  g_clear_object (&application->input_settings);
  g_clear_object (&application->input_sources);
  g_clear_object (&application->notifications);
  g_clear_object (&application->power);
  g_clear_object (&application->screencast);
  g_clear_object (&application->screenshot);
  g_clear_object (&application->screensaver);
  g_clear_object (&application->sound);
  g_clear_object (&application->status_notifier_watcher);

  g_clear_pointer (&application->display_change_dialog, gtk_widget_destroy);

  g_clear_object (&application->backend);

  G_OBJECT_CLASS (gf_application_parent_class)->dispose (object);
}

static void
keep_changes_cb (GfConfirmDisplayChangeDialog *dialog,
                 gboolean                      keep_changes,
                 GfApplication                *application)
{
  GfMonitorManager *monitor_manager;

  monitor_manager = gf_backend_get_monitor_manager (application->backend);

  gf_monitor_manager_confirm_configuration (monitor_manager, keep_changes);
  g_clear_pointer (&application->display_change_dialog, gtk_widget_destroy);
}

static void
confirm_display_change_cb (GfMonitorManager *monitor_manager,
                           GfApplication    *application)
{
  gint timeout;

  timeout = gf_monitor_manager_get_display_configuration_timeout ();

  g_clear_pointer (&application->display_change_dialog, gtk_widget_destroy);
  application->display_change_dialog = gf_confirm_display_change_dialog_new (timeout);

  g_signal_connect (application->display_change_dialog, "keep-changes",
                    G_CALLBACK (keep_changes_cb), application);

  gtk_window_present (GTK_WINDOW (application->display_change_dialog));
}

static void
gf_application_init (GfApplication *application)
{
  GtkSettings *settings;
  GfMonitorManager *monitor_manager;

  application->backend = gf_backend_new (GF_BACKEND_TYPE_X11_CM);

  monitor_manager = gf_backend_get_monitor_manager (application->backend);
  g_signal_connect (monitor_manager, "confirm-display-change",
                    G_CALLBACK (confirm_display_change_cb), application);

  application->settings = g_settings_new ("org.gnome.gnome-flashback");
  settings = gtk_settings_get_default ();

  g_signal_connect (application->settings, "changed",
                    G_CALLBACK (settings_changed), application);
  g_signal_connect (settings, "notify::gtk-theme-name",
                    G_CALLBACK (theme_changed), application);

  settings_changed (application->settings, NULL, application);
  theme_changed (settings, NULL, application);

  application->bus_name = g_bus_own_name (G_BUS_TYPE_SESSION,
                                          "org.gnome.Shell",
                                          G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                          G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                          NULL, NULL, NULL, NULL, NULL);
}

static void
gf_application_class_init (GfApplicationClass *application_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (application_class);

  object_class->dispose = gf_application_dispose;
}

GfApplication *
gf_application_new (void)
{
  return g_object_new (GF_TYPE_APPLICATION, NULL);
}
