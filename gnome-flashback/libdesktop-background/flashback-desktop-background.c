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

#include <gtk/gtk.h>
#include <libgnome-desktop/gnome-bg.h>
#include "flashback-desktop-background.h"

/*#define FLASHBACK_BACKGROUND_SCHEMA "org.gnome.gnome-flashback.background"
#define KEY_FADE                    "fade"*/

struct _FlashbackDesktopBackgroundPrivate {
	GnomeBG   *gnome_bg;
	GSettings *gnome_settings;
	gulong     screen_size_handler;
	gulong     screen_monitors_handler;
};

G_DEFINE_TYPE (FlashbackDesktopBackground, flashback_desktop_background, G_TYPE_OBJECT);

static void
flashback_desktop_background_draw (FlashbackDesktopBackground *background)
{
	GdkScreen *screen = gdk_screen_get_default ();
	cairo_surface_t *surface = gnome_bg_create_surface (background->priv->gnome_bg,
	                                                    gdk_screen_get_root_window (screen),
	                                                    gdk_screen_get_width (screen),
	                                                    gdk_screen_get_height (screen),
	                                                    TRUE);
	gnome_bg_set_surface_as_root (screen, surface);
	cairo_surface_destroy (surface);
}

static void
flashback_desktop_background_changed (GnomeBG  *bg,
                                      gpointer  user_data)
{
	FlashbackDesktopBackground *background = FLASHBACK_DESKTOP_BACKGROUND (user_data);

	flashback_desktop_background_draw (background);
}

static void
flashback_desktop_background_transitioned (GnomeBG  *bg,
                                           gpointer  user_data)
{
	FlashbackDesktopBackground *background = FLASHBACK_DESKTOP_BACKGROUND (user_data);

	flashback_desktop_background_draw (background);
}

static void
flashback_desktop_background_screen_size_changed (GdkScreen                  *screen,
                                                  FlashbackDesktopBackground *background)
{
	flashback_desktop_background_draw (background);
}

static gboolean
flashback_desktop_background_settings_change_event (GSettings *settings,
                                                    gpointer   keys,
                                                    gint       n_keys,
                                                    gpointer   user_data)
{
	FlashbackDesktopBackground *background = FLASHBACK_DESKTOP_BACKGROUND (user_data);

	gnome_bg_load_from_preferences (background->priv->gnome_bg, background->priv->gnome_settings);

	return TRUE;
}

static void
flashback_desktop_background_finalize (GObject *object)
{
	FlashbackDesktopBackground *background = FLASHBACK_DESKTOP_BACKGROUND (object);

	if (background->priv->screen_size_handler > 0) {
		g_signal_handler_disconnect (gdk_screen_get_default (),
		                             background->priv->screen_size_handler);
		background->priv->screen_size_handler = 0;
	}

	if (background->priv->screen_monitors_handler > 0) {
		g_signal_handler_disconnect (gdk_screen_get_default (),
		                             background->priv->screen_monitors_handler);
		background->priv->screen_monitors_handler = 0;
	}

	g_signal_handlers_disconnect_by_func (background->priv->gnome_settings,
	                                      flashback_desktop_background_settings_change_event,
	                                      background);

	if (background->priv->gnome_bg) {
		g_object_unref (background->priv->gnome_bg);
		background->priv->gnome_bg = NULL;
	}

	if (background->priv->gnome_settings) {
		g_object_unref (background->priv->gnome_settings);
		background->priv->gnome_settings = NULL;
	}

	G_OBJECT_CLASS (flashback_desktop_background_parent_class)->finalize (object);
}

static void
flashback_desktop_background_init (FlashbackDesktopBackground *background)
{
	background->priv = G_TYPE_INSTANCE_GET_PRIVATE (background, FLASHBACK_TYPE_DESKTOP_BACKGROUND, FlashbackDesktopBackgroundPrivate);

	background->priv->gnome_bg = gnome_bg_new ();
	background->priv->gnome_settings = g_settings_new ("org.gnome.desktop.background");

	g_signal_connect (background->priv->gnome_bg, "changed",
	                  G_CALLBACK (flashback_desktop_background_changed), background);
	g_signal_connect (background->priv->gnome_bg, "transitioned",
	                  G_CALLBACK (flashback_desktop_background_transitioned), background);

	background->priv->screen_size_handler = g_signal_connect (gdk_screen_get_default (), "size-changed",
	                                                          G_CALLBACK (flashback_desktop_background_screen_size_changed), background);
	background->priv->screen_monitors_handler = g_signal_connect (gdk_screen_get_default (), "monitors-changed",
	                                                              G_CALLBACK (flashback_desktop_background_screen_size_changed), background);

	gnome_bg_load_from_preferences (background->priv->gnome_bg, background->priv->gnome_settings);

	g_signal_connect (background->priv->gnome_settings, "change-event",
	                  G_CALLBACK (flashback_desktop_background_settings_change_event), background);
}

static void
flashback_desktop_background_class_init (FlashbackDesktopBackgroundClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = flashback_desktop_background_finalize;

	g_type_class_add_private (class, sizeof (FlashbackDesktopBackgroundPrivate));
}

FlashbackDesktopBackground *
flashback_desktop_background_new (void)
{
	return g_object_new (FLASHBACK_TYPE_DESKTOP_BACKGROUND,
	                     NULL);
}
