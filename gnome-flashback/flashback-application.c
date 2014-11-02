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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "config.h"
#include "flashback-application.h"
#include "libautomount-manager/gsd-automount-manager.h"
#include "libdesktop-background/desktop-background.h"
#include "libdisplay-config/flashback-display-config.h"
#include "libend-session-dialog/flashback-end-session-dialog.h"
#include "libidle-monitor/meta-idle-monitor-dbus.h"
#include "libkey-grabber/flashback-key-grabber.h"
#include "libsound-applet/gvc-applet.h"

#define FLASHBACK_SCHEMA       "org.gnome.gnome-flashback"
#define KEY_AUTOMOUNT_MANAGER  "automount-manager"
#define KEY_DESKTOP_BACKGROUND "desktop-background"
#define KEY_DISPLAY_CONFIG     "display-config"
#define KEY_END_SESSION_DIALOG "end-session-dialog"
#define KEY_KEY_GRABBER        "key-grabber"
#define KEY_IDLE_MONITOR       "idle-monitor"
#define KEY_SOUND_APPLET       "sound-applet"

struct _FlashbackApplicationPrivate {
	GSettings                  *settings;
	GsdAutomountManager        *automount;
	DesktopBackground          *background;
	FlashbackDisplayConfig     *config;
	FlashbackEndSessionDialog  *dialog;
	FlashbackKeyGrabber        *grabber;
	MetaIdleMonitorDBus        *idle_monitor;
	GvcApplet                  *applet;
};

G_DEFINE_TYPE_WITH_PRIVATE (FlashbackApplication, flashback_application, G_TYPE_OBJECT);

static void
flashback_application_settings_changed (GSettings   *settings,
                                        const gchar *key,
                                        gpointer     user_data)
{
	FlashbackApplication *app = FLASHBACK_APPLICATION (user_data);

	if (key == NULL || g_strcmp0 (key, KEY_AUTOMOUNT_MANAGER) == 0) {
		if (g_settings_get_boolean (settings, KEY_AUTOMOUNT_MANAGER)) {
			if (app->priv->automount == NULL) {
				app->priv->automount = gsd_automount_manager_new ();
			}
		} else {
			if (app->priv->automount) {
				g_object_unref (app->priv->automount);
				app->priv->automount = NULL;
			}
		}
	}

	if (key == NULL || g_strcmp0 (key, KEY_DESKTOP_BACKGROUND) == 0) {
		if (g_settings_get_boolean (settings, KEY_DESKTOP_BACKGROUND)) {
			if (app->priv->background == NULL) {
				app->priv->background = desktop_background_new ();
			}
		} else {
			g_clear_object (&app->priv->background);
		}
	}

	if (key == NULL || g_strcmp0 (key, KEY_DISPLAY_CONFIG) == 0) {
		if (g_settings_get_boolean (settings, KEY_DISPLAY_CONFIG)) {
			if (app->priv->config == NULL) {
				app->priv->config = flashback_display_config_new ();
			}
		} else {
			g_clear_object (&app->priv->config);
		}
	}

	if (key == NULL || g_strcmp0 (key, KEY_END_SESSION_DIALOG) == 0) {
		if (g_settings_get_boolean (settings, KEY_END_SESSION_DIALOG)) {
			if (app->priv->dialog == NULL) {
				app->priv->dialog = flashback_end_session_dialog_new ();
			}
		} else {
			g_clear_object (&app->priv->dialog);
		}
	}

	if (key == NULL || g_strcmp0 (key, KEY_IDLE_MONITOR) == 0) {
		if (g_settings_get_boolean (settings, KEY_IDLE_MONITOR)) {
			if (app->priv->idle_monitor == NULL) {
				app->priv->idle_monitor = meta_idle_monitor_dbus_new ();
			}
		} else {
			g_clear_object (&app->priv->idle_monitor);
		}
	}

	if (key == NULL || g_strcmp0 (key, KEY_KEY_GRABBER) == 0) {
		if (g_settings_get_boolean (settings, KEY_KEY_GRABBER)) {
			if (app->priv->grabber == NULL) {
				app->priv->grabber = flashback_key_grabber_new ();
			}
		} else {
			g_clear_object (&app->priv->grabber);
		}
	}

	if (key == NULL || g_strcmp0 (key, KEY_SOUND_APPLET) == 0) {
		if (g_settings_get_boolean (settings, KEY_SOUND_APPLET)) {
			if (app->priv->applet == NULL) {
				app->priv->applet = gvc_applet_new ();
			}
		} else {
			g_clear_object (&app->priv->applet);
		}
	}
}

static void
flashback_application_finalize (GObject *object)
{
	FlashbackApplication *app = FLASHBACK_APPLICATION (object);

	g_clear_object (&app->priv->background);
	g_clear_object (&app->priv->config);
	g_clear_object (&app->priv->dialog);
	g_clear_object (&app->priv->idle_monitor);
	g_clear_object (&app->priv->grabber);
	g_clear_object (&app->priv->applet);
	g_clear_object (&app->priv->settings);

	G_OBJECT_CLASS (flashback_application_parent_class)->finalize (object);
}

static void
flashback_application_init (FlashbackApplication *application)
{
	FlashbackApplicationPrivate *priv;

	application->priv = flashback_application_get_instance_private (application);
	priv = application->priv;

	priv->settings = g_settings_new (FLASHBACK_SCHEMA);

	g_signal_connect (priv->settings, "changed",
	                  G_CALLBACK (flashback_application_settings_changed), application);
	flashback_application_settings_changed (priv->settings, NULL, application);
}

static void
flashback_application_class_init (FlashbackApplicationClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = flashback_application_finalize;
}

FlashbackApplication *
flashback_application_new (void)
{
	return g_object_new (FLASHBACK_TYPE_APPLICATION, NULL);
}
