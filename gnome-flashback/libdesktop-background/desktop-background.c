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

#include "desktop-window.h"
#include "desktop-background.h"

struct _DesktopBackgroundPrivate {
	GnomeBG          *bg;
	GnomeBGCrossfade *fade;

	GSettings        *gnome_settings;
	GSettings        *background_settings;

	GtkWidget        *background;

	cairo_surface_t  *surface;
	int               width;
	int               height;

	guint             change_idle_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (DesktopBackground, desktop_background, G_TYPE_OBJECT);

static void
free_fade (DesktopBackground *background)
{
	g_clear_object (&background->priv->fade);
}

static void
free_surface (DesktopBackground *background)
{
	if (background->priv->surface != NULL) {
		cairo_surface_destroy (background->priv->surface);
		background->priv->surface = NULL;
	}
}

static void
background_unrealize (DesktopBackground *background)
{
	free_surface (background);

	background->priv->width = 0;
	background->priv->height = 0;
}

static void
init_fade (DesktopBackground *background)
{
	DesktopBackgroundPrivate *priv;
	gboolean                  fade;
	GdkScreen                *screen;

	priv = background->priv;
	fade = g_settings_get_boolean (priv->background_settings, "fade");
	screen = gdk_screen_get_default ();

	if (!fade)
		return;

	if (priv->fade == NULL) {
		GdkWindow *window = gtk_widget_get_window (priv->background);
		int width = gdk_screen_get_width (screen);
		int height = gdk_screen_get_height (screen);

		if (width == gdk_window_get_width (window) && height == gdk_window_get_height (window)) {
			priv->fade = gnome_bg_crossfade_new (width, height);
			g_signal_connect_swapped (priv->fade, "finished", G_CALLBACK (free_fade), background);
		}
	}

	if (priv->fade != NULL && !gnome_bg_crossfade_is_started (priv->fade)) {
		cairo_surface_t *surface;

		if (priv->surface == NULL) {
			surface = gnome_bg_get_surface_from_root (screen);
		} else {
			surface = cairo_surface_reference (priv->surface);
		}

		gnome_bg_crossfade_set_start_surface (priv->fade, surface);
		cairo_surface_destroy (surface);
	}
}

static void
background_ensure_realized (DesktopBackground *bg)
{
	int        width;
	int        height;
	GdkScreen *screen;
	GdkWindow *window;

	screen = gdk_screen_get_default ();
	height = gdk_screen_get_height (screen);
	width = gdk_screen_get_width (screen);

	if (width == bg->priv->width && height == bg->priv->height)
		return;

	free_surface (bg);

	window = gtk_widget_get_window (bg->priv->background);
	bg->priv->surface = gnome_bg_create_surface (bg->priv->bg, window, width, height, TRUE);

	bg->priv->width = width;
	bg->priv->height = height;
}

static void
on_fade_finished (GnomeBGCrossfade *fade,
                  GdkWindow        *window,
                  gpointer          user_data)
{
    DesktopBackground        *background;
    DesktopBackgroundPrivate *priv;

	background = DESKTOP_BACKGROUND (user_data);
	priv = background->priv;

	background_ensure_realized (background);

	if (priv->surface != NULL)
		gnome_bg_set_surface_as_root (gdk_window_get_screen (window), priv->surface);
}

static gboolean
fade_to_surface (DesktopBackground *background,
                 GdkWindow         *window,
                 cairo_surface_t   *surface)
{
	DesktopBackgroundPrivate *priv;

	priv = background->priv;

	if (priv->fade == NULL || !gnome_bg_crossfade_set_end_surface (priv->fade, surface))
		return FALSE;

	if (!gnome_bg_crossfade_is_started (priv->fade)) {
		gnome_bg_crossfade_start (priv->fade, window);
		g_signal_connect (priv->fade, "finished", G_CALLBACK (on_fade_finished), background);
	}

	return gnome_bg_crossfade_is_started (priv->fade);
}

static void
background_set_up (DesktopBackground *background)
{
	DesktopBackgroundPrivate *priv;
	GdkWindow                *window;

	priv = background->priv;

	background_ensure_realized (background);

	if (priv->surface == NULL)
		return;

	window = gtk_widget_get_window (priv->background);

	if (!fade_to_surface (background, window, priv->surface)) {
		cairo_pattern_t *pattern;

		pattern = cairo_pattern_create_for_surface (priv->surface);
		gdk_window_set_background_pattern (window, pattern);
		cairo_pattern_destroy (pattern);

		gnome_bg_set_surface_as_root (gdk_screen_get_default (), priv->surface);
	}
}

static gboolean
background_changed_cb (gpointer user_data)
{
	DesktopBackground *background = DESKTOP_BACKGROUND (user_data);

	background->priv->change_idle_id = 0;

	background_unrealize (background);
	background_set_up (background);

	gtk_widget_queue_draw (background->priv->background);

	return G_SOURCE_REMOVE;
}

static void
queue_background_change (DesktopBackground *background)
{
	DesktopBackgroundPrivate *priv;

	priv = background->priv;

	if (priv->change_idle_id != 0) {
		g_source_remove (priv->change_idle_id);
	}

	priv->change_idle_id = g_idle_add (background_changed_cb, background);
}

static void
desktop_background_changed (GnomeBG  *bg,
                            gpointer  user_data)
{
	DesktopBackground *background = DESKTOP_BACKGROUND (user_data);

	init_fade (background);
	queue_background_change (background);
}

static void
desktop_background_transitioned (GnomeBG  *bg,
                                 gpointer  user_data)
{
	DesktopBackground *background = DESKTOP_BACKGROUND (user_data);

	init_fade (background);
	queue_background_change (background);
}

static gboolean
desktop_background_change_event (GSettings *settings,
                                 gpointer   keys,
                                 gint       n_keys,
                                 gpointer   user_data)
{
	DesktopBackground *background = DESKTOP_BACKGROUND (user_data);

	gnome_bg_load_from_preferences (background->priv->bg, background->priv->gnome_settings);

	return TRUE;
}

static void
desktop_background_finalize (GObject *object)
{
	DesktopBackground        *background;
	DesktopBackgroundPrivate *priv;

	background = DESKTOP_BACKGROUND (object);
	priv = background->priv;

	g_signal_handlers_disconnect_by_func (priv->gnome_settings,
	                                      desktop_background_change_event,
	                                      background);

	g_clear_object (&priv->bg);
	g_clear_object (&priv->gnome_settings);

	free_surface (background);
	free_fade (background);

	g_clear_object (&priv->gnome_settings);
	g_clear_object (&priv->background_settings);

	G_OBJECT_CLASS (desktop_background_parent_class)->finalize (object);
}

static void
desktop_background_update (DesktopWindow *window,
                           gpointer       user_data)
{
	DesktopBackground *background = DESKTOP_BACKGROUND (user_data);

	queue_background_change (user_data);
}

static void
desktop_background_init (DesktopBackground *background)
{
	DesktopBackgroundPrivate *priv;

	priv = background->priv = desktop_background_get_instance_private (background);

	priv->bg = gnome_bg_new ();
	priv->gnome_settings = g_settings_new ("org.gnome.desktop.background");
	priv->background_settings = g_settings_new ("org.gnome.gnome-flashback.desktop-background");

	g_signal_connect (priv->bg, "changed",
	                  G_CALLBACK (desktop_background_changed), background);
	g_signal_connect (priv->bg, "transitioned",
	                  G_CALLBACK (desktop_background_transitioned), background);

	gnome_bg_load_from_preferences (priv->bg, priv->gnome_settings);

	g_signal_connect (priv->gnome_settings, "change-event",
	                  G_CALLBACK (desktop_background_change_event), background);

	priv->background = desktop_window_new ();
	g_signal_connect (priv->background, "update",
	                  G_CALLBACK (desktop_background_update), background);

	queue_background_change (background);
}

static void
desktop_background_class_init (DesktopBackgroundClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = desktop_background_finalize;
}

DesktopBackground *
desktop_background_new (void)
{
	return DESKTOP_BACKGROUND (g_object_new (DESKTOP_BACKGROUND_TYPE, NULL));
}
