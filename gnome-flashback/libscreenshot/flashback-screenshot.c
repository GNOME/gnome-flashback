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
#include "flashback-dbus-screenshot.h"
#include "flashback-screenshot.h"

#define SHELL_DBUS_NAME "org.gnome.Shell"
#define SCREENSHOT_DBUS_PATH "/org/gnome/Shell/Screenshot"

struct _FlashbackScreenshot
{
  GObject                  parent;

  gint                     bus_name;
  GDBusInterfaceSkeleton  *iface;
};

G_DEFINE_TYPE (FlashbackScreenshot, flashback_screenshot, G_TYPE_OBJECT)

static gboolean
handle_screenshot (FlashbackDBusScreenshot *dbus_screenshot,
                   GDBusMethodInvocation   *invocation,
                   gboolean                 include_cursor,
                   gboolean                 flash,
                   const gchar             *filename,
                   gpointer                 user_data)
{
  g_warning ("screenshot: screenshot");
  flashback_dbus_screenshot_complete_screenshot (dbus_screenshot, invocation,
                                                 FALSE, "");

  return TRUE;
}

static gboolean
handle_screenshot_window (FlashbackDBusScreenshot *dbus_screenshot,
                          GDBusMethodInvocation   *invocation,
                          gboolean                 include_frame,
                          gboolean                 include_cursor,
                          gboolean                 flash,
                          const gchar             *filename,
                          gpointer                 user_data)
{
  g_warning ("screenshot: screenshot-window");
  flashback_dbus_screenshot_complete_screenshot_window (dbus_screenshot, invocation,
                                                        FALSE, "");

  return TRUE;
}

static gboolean
handle_screenshot_area (FlashbackDBusScreenshot *dbus_screenshot,
                        GDBusMethodInvocation   *invocation,
                        gint                     x,
                        gint                     y,
                        gint                     width,
                        gint                     height,
                        const gchar             *file_template,
                        GVariant                *options,
                        gpointer                 user_data)
{
  g_warning ("screenshot: screenshot-area");
  flashback_dbus_screenshot_complete_screenshot_area (dbus_screenshot, invocation,
                                                      FALSE, "");

  return TRUE;
}

static gboolean
handle_flash_area (FlashbackDBusScreenshot *dbus_screenshot,
                   GDBusMethodInvocation   *invocation,
                   gint                     x,
                   gint                     y,
                   gint                     width,
                   gint                     height,
                   gpointer                 user_data)
{
  g_warning ("screenshot: flash-area");
  flashback_dbus_screenshot_complete_flash_area (dbus_screenshot, invocation);

  return TRUE;
}

static gboolean
handle_select_area (FlashbackDBusScreenshot *dbus_screenshot,
                    GDBusMethodInvocation   *invocation,
                    gpointer                 user_data)
{
  g_warning ("screenshot: select-area");
  flashback_dbus_screenshot_complete_select_area (dbus_screenshot, invocation,
                                                  0, 0, 0, 0);

  return TRUE;
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  FlashbackScreenshot *screenshot;
  FlashbackDBusScreenshot *skeleton;
  GError *error;

  screenshot = FLASHBACK_SCREENSHOT (user_data);
  skeleton = flashback_dbus_screenshot_skeleton_new ();

  g_signal_connect (skeleton, "handle-screenshot",
                    G_CALLBACK (handle_screenshot), screenshot);
  g_signal_connect (skeleton, "handle-screenshot-window",
                    G_CALLBACK (handle_screenshot_window), screenshot);
  g_signal_connect (skeleton, "handle-screenshot-area",
                    G_CALLBACK (handle_screenshot_area), screenshot);
  g_signal_connect (skeleton, "handle-flash-area",
                    G_CALLBACK (handle_flash_area), screenshot);
  g_signal_connect (skeleton, "handle-select-area",
                    G_CALLBACK (handle_select_area), screenshot);

  error = NULL;
  screenshot->iface = G_DBUS_INTERFACE_SKELETON (skeleton);

	if (!g_dbus_interface_skeleton_export (screenshot->iface, connection,
	                                       SCREENSHOT_DBUS_PATH,
	                                       &error))
  {
    g_warning ("Failed to export interface: %s", error->message);
    g_error_free (error);
    return;
  }
}

static void
flashback_screenshot_finalize (GObject *object)
{
  FlashbackScreenshot *screenshot;

  screenshot = FLASHBACK_SCREENSHOT (object);

  if (screenshot->bus_name)
    {
      g_bus_unwatch_name (screenshot->bus_name);
      screenshot->bus_name = 0;
    }

  G_OBJECT_CLASS (flashback_screenshot_parent_class)->finalize (object);
}

static void
flashback_screenshot_class_init (FlashbackScreenshotClass *screenshot_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (screenshot_class);

  object_class->finalize = flashback_screenshot_finalize;
}

static void
flashback_screenshot_init (FlashbackScreenshot *screenshot)
{
  screenshot->bus_name = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                           SHELL_DBUS_NAME,
                                           G_BUS_NAME_WATCHER_FLAGS_NONE,
                                           name_appeared_handler,
                                           NULL,
                                           screenshot,
                                           NULL);
}

FlashbackScreenshot *
flashback_screenshot_new (void)
{
	return g_object_new (FLASHBACK_TYPE_SCREENSHOT, NULL);
}
