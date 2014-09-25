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

enum {
	UPDATE_SIGNAL,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

static Atom _NET_CLIENT_LIST = None;
static Atom _NET_WM_WINDOW_TYPE = None;
static Atom _NET_WM_WINDOW_TYPE_DESKTOP = None;

static Window *
get_windows (Display       *display,
             unsigned long *items)
{
	Atom           type;
	unsigned long  left;
    unsigned char *list;
    int            result;
    int            format;

	result = XGetWindowProperty (display, XDefaultRootWindow (display), _NET_CLIENT_LIST,
	                             0L, 1024L, False, XA_WINDOW, &type, &format,
	                             items, &left, &list);

	if (result != Success)
		return (Window *) 0;

	return (Window *) list;
}

static gboolean
is_desktop_window (Display *display,
                   Window   window)
{
	Atom           type;
	Atom          *atoms;
	int            result;
	int            format;
	unsigned long  items;
	unsigned long  left;
	unsigned char *data;

	result = XGetWindowProperty (display, window, _NET_WM_WINDOW_TYPE,
	                             0L, 1L, False, XA_ATOM, &type, &format,
	                             &items, &left, &data);

	if (result != Success)
		return FALSE;

	atoms = (Atom *) data;

	if (items && atoms[0] == _NET_WM_WINDOW_TYPE_DESKTOP) {
		XFree (data);
		return TRUE;
	}

	XFree (data);
	return FALSE;
}

static Window *
get_desktop_windows_list (Display       *display,
                          unsigned long *items)
{
	Window        *list;
	unsigned long  all_items;
	int            i;
	Window        *desktops;

	*items = 0;
	list = get_windows (display, &all_items);
	desktops = g_new0 (Window, all_items);

	for (i = 0; i < all_items; i++) {
		if (is_desktop_window (display, list[i])) {
			desktops[(*items)++] = list[i];
		}
	}

	return desktops;
}

static void
desktop_window_ensure_below (GtkWidget *widget)
{
	GdkWindow     *our_desktop_window;
	GdkWindow     *other_desktop_window;
	GdkDisplay    *display;
	Window        *list;
	unsigned long  items;
	int            i;

	gdk_error_trap_push ();

	our_desktop_window = gtk_widget_get_window (widget);
	display = gdk_display_get_default ();
	list = get_desktop_windows_list (gdk_x11_display_get_xdisplay (display), &items);

	for (i = 0; i < items; i++) {
		other_desktop_window = gdk_x11_window_foreign_new_for_display (display, list[i]);
		if (other_desktop_window != our_desktop_window) {
			gdk_window_raise (other_desktop_window);
		}
	}

	gdk_error_trap_pop_ignored ();

	g_free (list);
}

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

	g_signal_emit (window, signals [UPDATE_SIGNAL], 0);
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
desktop_window_relaize (GtkWidget *widget)
{
	GdkWindow *window;
	GdkAtom    atom;

	GTK_WIDGET_CLASS (desktop_window_parent_class)->realize (widget);

	window = gtk_widget_get_window (widget);
	atom = gdk_atom_intern ("_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);
	gdk_property_change (window,
	                     gdk_atom_intern ("_NET_WM_WINDOW_TYPE", FALSE),
	                     gdk_x11_xatom_to_atom (XA_ATOM), 32,
	                     GDK_PROP_MODE_REPLACE, (guchar *) &atom, 1);
}

static void
desktop_window_unrelaize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (desktop_window_parent_class)->unrealize (widget);
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
	Display              *display;

	priv = window->priv = desktop_window_get_instance_private (window);
	screen = gdk_screen_get_default ();
	display = gdk_x11_display_get_xdisplay (gdk_display_get_default ());

	id = g_signal_connect (screen, "monitors-changed",
	                       G_CALLBACK (desktop_window_screen_changed), window);
	priv->monitors_changed_id = id;

	id = g_signal_connect (screen, "size-changed",
	                       G_CALLBACK (desktop_window_screen_changed), window);
	priv->size_changed_id = id;

	desktop_window_screen_changed (screen, window);

	_NET_CLIENT_LIST = XInternAtom (display, "_NET_CLIENT_LIST", False);
	_NET_WM_WINDOW_TYPE = XInternAtom (display, "_NET_WM_WINDOW_TYPE", False);
	_NET_WM_WINDOW_TYPE_DESKTOP = XInternAtom (display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
}

static gboolean
desktop_window_configure_event (GtkWidget         *widget,
                                GdkEventConfigure *event)
{
	if (GTK_WIDGET_CLASS (desktop_window_parent_class)->configure_event)
		GTK_WIDGET_CLASS (desktop_window_parent_class)->configure_event (widget, event);

	desktop_window_ensure_below (widget);

	return TRUE;
}

static void
desktop_window_style_updated (GtkWidget *widget)
{
	DesktopWindow *window = DESKTOP_WINDOW (widget);

	GTK_WIDGET_CLASS (desktop_window_parent_class)->style_updated (widget);

	g_signal_emit (window, signals [UPDATE_SIGNAL], 0);
}

static void
desktop_window_class_init (DesktopWindowClass *class)
{
	GObjectClass   *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	object_class->dispose = desktop_window_dispose;

	widget_class->realize = desktop_window_relaize;
	widget_class->unrealize = desktop_window_unrelaize;
	widget_class->map = desktop_window_map;
	widget_class->configure_event = desktop_window_configure_event;
	widget_class->style_updated = desktop_window_style_updated;

	signals [UPDATE_SIGNAL] =
		g_signal_new ("update",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		              0,
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE,
		              0);
}

GtkWidget *
desktop_window_new (void)
{
	GObject   *object;
	GtkWidget *widget;
	GtkWindow *window;

	object = g_object_new (DESKTOP_WINDOW_TYPE,
	                       "type", GTK_WINDOW_TOPLEVEL,
	                       "decorated", FALSE,
	                       "skip-pager-hint", TRUE,
	                       "skip-taskbar-hint", TRUE,
	                       "resizable", FALSE,
	                       "app-paintable", TRUE,
	                       NULL);
	widget = GTK_WIDGET (object);
	window = GTK_WINDOW (widget);

	gtk_window_set_keep_below (window, TRUE);
	gtk_window_present (window);

	return widget;
}
