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

#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include "desktop-window.h"

struct _DesktopWindowPrivate {
	gulong size_changed_id;
	gulong monitors_changed_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (DesktopWindow, desktop_window, GTK_TYPE_WINDOW);

static void
desktop_window_screen_changed (GdkScreen *screen,
                               gpointer   user_data)
{
	DesktopWindow *window;
	gint           width;
	gint           height;

	window = DESKTOP_WINDOW (user_data);
	width = gdk_screen_get_width (screen);
	height = gdk_screen_get_height (screen);

	g_object_set (window,
	              "width-request", width,
	              "height-request", height,
	              NULL);
}

static void
desktop_window_dispose (GObject *object)
{
	DesktopWindow        *window;
	DesktopWindowPrivate *priv;
	GdkScreen            *screen;

	window = DESKTOP_WINDOW (object);
	priv = window->priv;
	screen = gdk_screen_get_default ();

	if (priv->size_changed_id > 0) {
		g_signal_handler_disconnect (screen, priv->size_changed_id);
		priv->size_changed_id = 0;
	}

	if (priv->monitors_changed_id > 0) {
		g_signal_handler_disconnect (screen, priv->monitors_changed_id);
		priv->monitors_changed_id = 0;
	}

	G_OBJECT_CLASS (desktop_window_parent_class)->dispose (object);
}

static void
desktop_window_map (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (desktop_window_parent_class)->map (widget);

	gdk_window_lower (gtk_widget_get_window (widget));
}

static void
desktop_window_init (DesktopWindow *window)
{
	DesktopWindowPrivate *priv;
	gint                  id;
	GdkScreen            *screen;

	priv = window->priv = desktop_window_get_instance_private (window);
	screen = gdk_screen_get_default ();

	id = g_signal_connect (screen, "monitors-changed",
	                       G_CALLBACK (desktop_window_screen_changed), window);
	priv->monitors_changed_id = id;

	id = g_signal_connect (screen, "size-changed",
	                       G_CALLBACK (desktop_window_screen_changed), window);
	priv->size_changed_id = id;

	desktop_window_screen_changed (screen, window);
}

static void
desktop_window_class_init (DesktopWindowClass *class)
{
	GObjectClass   *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	object_class->dispose = desktop_window_dispose;

	widget_class->map = desktop_window_map;
}

GtkWidget *
desktop_window_new (void)
{
	GObject   *object;
	GtkWidget *widget;
	GtkWindow *window;

	object = g_object_new (DESKTOP_WINDOW_TYPE,
	                       "type", GTK_WINDOW_POPUP,
	                       "type-hint", GDK_WINDOW_TYPE_HINT_DESKTOP,
	                       "decorated", FALSE,
	                       "skip-pager-hint", TRUE,
	                       "skip-taskbar-hint", TRUE,
	                       "resizable", FALSE,
	                       "app-paintable", TRUE,
	                       NULL);
	widget = GTK_WIDGET (object);
	window = GTK_WINDOW (widget);

	gtk_window_set_keep_below (window, TRUE);
	gtk_widget_realize (widget);

	return widget;
}
