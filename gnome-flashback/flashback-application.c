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

#include <config.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "flashback-application.h"
#include "libautomount-manager/gsd-automount-manager.h"
#include "libdesktop-background/desktop-background.h"
#include "libdisplay-config/flashback-display-config.h"
#include "libend-session-dialog/flashback-end-session-dialog.h"
#include "libscreencast/flashback-screencast.h"
#include "libscreenshot/flashback-screenshot.h"
#include "libshell/flashback-shell.h"
#include "libsound-applet/gvc-applet.h"

struct _FlashbackApplication
{
  GObject                    parent;

  gint                       bus_name;

  GSettings                 *settings;

  GsdAutomountManager       *automount;
  DesktopBackground         *background;
  FlashbackDisplayConfig    *config;
  FlashbackEndSessionDialog *dialog;
  FlashbackScreencast       *screencast;
  FlashbackScreenshot       *screenshot;
  FlashbackShell            *shell;
  GvcApplet                 *applet;
};

G_DEFINE_TYPE (FlashbackApplication, flashback_application, G_TYPE_OBJECT)

static void
flashback_application_settings_changed (GSettings   *settings,
                                        const gchar *key,
                                        gpointer     user_data)
{
  FlashbackApplication *application;

  application = FLASHBACK_APPLICATION (user_data);

#define SETTING_CHANGED(variable_name, setting_name, function_name) \
  if (key == NULL || g_strcmp0 (key, setting_name))                 \
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
  SETTING_CHANGED (background, "desktop-background", desktop_background_new)
  SETTING_CHANGED (config, "display-config", flashback_display_config_new)
  SETTING_CHANGED (dialog, "end-session-dialog", flashback_end_session_dialog_new)
  SETTING_CHANGED (screencast, "screencast", flashback_screencast_new)
  SETTING_CHANGED (screenshot, "screenshot", flashback_screenshot_new)
  SETTING_CHANGED (shell, "shell", flashback_shell_new)
  SETTING_CHANGED (applet, "sound-applet", gvc_applet_new)

#undef SETTING_CHANGED
}

static void
flashback_application_finalize (GObject *object)
{
  FlashbackApplication *application;

  application = FLASHBACK_APPLICATION (object);

  if (application->bus_name)
    {
      g_bus_unown_name (application->bus_name);
      application->bus_name = 0;
    }

  g_clear_object (&application->settings);

  g_clear_object (&application->automount);
  g_clear_object (&application->background);
  g_clear_object (&application->config);
  g_clear_object (&application->dialog);
  g_clear_object (&application->screencast);
  g_clear_object (&application->screenshot);
  g_clear_object (&application->shell);
  g_clear_object (&application->applet);

  G_OBJECT_CLASS (flashback_application_parent_class)->finalize (object);
}

static void
flashback_application_init (FlashbackApplication *application)
{
  application->settings = g_settings_new ("org.gnome.gnome-flashback");

  g_signal_connect (application->settings, "changed",
                    G_CALLBACK (flashback_application_settings_changed),
                    application);

  flashback_application_settings_changed (application->settings,
                                          NULL, application);

  application->bus_name = g_bus_own_name (G_BUS_TYPE_SESSION,
                                          "org.gnome.Shell",
                                          G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                          G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                          NULL, NULL, NULL, NULL, NULL);
}

static void
flashback_application_class_init (FlashbackApplicationClass *application_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (application_class);

  object_class->finalize = flashback_application_finalize;
}

FlashbackApplication *
flashback_application_new (void)
{
  return g_object_new (FLASHBACK_TYPE_APPLICATION, NULL);
}
