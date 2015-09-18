/*
 * Copyright (C) 2014 - 2015 Alberts Muktupāvels
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

#include <gdk/gdkx.h>
#include <X11/Xatom.h>

#include "gf-background-window.h"

struct _GfBackgroundWindow
{
  GtkWindow  parent;

  Atom       desktop_manager_atom;
  Atom       client_list_atom;
  Atom       type_atom;
  Atom       type_desktop_atom;

  Window     xbackground;
  GdkWindow *background;

  Window     xdesktop;
  GdkWindow *desktop;
};

G_DEFINE_TYPE (GfBackgroundWindow, gf_background_window, GTK_TYPE_WINDOW)

static GdkWindow *
create_window_from_xwindow (Window xwindow)
{
  GdkDisplay *display;
  GdkWindow *window;
  GdkEventMask mask;

  display = gdk_display_get_default ();

  gdk_error_trap_push ();
  window = gdk_x11_window_foreign_new_for_display (display, xwindow);

  if (gdk_error_trap_pop () != 0)
    return NULL;

  mask = gdk_window_get_events (window);
  gdk_window_set_events (window, mask | GDK_STRUCTURE_MASK);

  return window;
}

static gboolean
is_desktop_window (GfBackgroundWindow *window,
                   Display            *display,
                   Window              xwindow)
{
  Atom type;
  Atom *atoms;
  int result;
  int format;
  unsigned long items;
  unsigned long left;

  if (window->type_atom == None)
    window->type_atom = XInternAtom (display, "_NET_WM_WINDOW_TYPE", False);

  if (window->type_desktop_atom == None)
    window->type_desktop_atom = XInternAtom (display,
                                             "_NET_WM_WINDOW_TYPE_DESKTOP",
                                             False);

  gdk_error_trap_push ();
  result = XGetWindowProperty (display, xwindow, window->type_atom,
                               0L, 1L, False, XA_ATOM, &type, &format,
                               &items, &left, (unsigned char **) &atoms);

  if (gdk_error_trap_pop () != 0 || result != Success)
    return FALSE;

  if (items && atoms[0] == window->type_desktop_atom)
    {
      XFree (atoms);
      return TRUE;
    }

  XFree (atoms);
  return FALSE;
}

static gboolean
get_desktop (GfBackgroundWindow *window)
{
  GdkDisplay *display;
  Display *xdisplay;
  Window root;
  Atom type;
  int result;
  int format;
  unsigned long items;
  unsigned long left;
  unsigned long i;
  Window *windows;
  Window desktop;

  display = gdk_display_get_default ();
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  if (window->client_list_atom == None)
    window->client_list_atom = XInternAtom (xdisplay, "_NET_CLIENT_LIST",
                                            False);

  gdk_error_trap_push ();

  root = XDefaultRootWindow (xdisplay);
  result = XGetWindowProperty (xdisplay, root, window->client_list_atom,
                               0L, 1024L, False, XA_WINDOW, &type, &format,
                               &items, &left, (unsigned char **) &windows);

  if (gdk_error_trap_pop () != 0 || result != Success)
    return FALSE;

  desktop = None;
  for (i = 0; i < items; i++)
    {
      if (windows[i] == window->xbackground)
        continue;

      if (is_desktop_window (window, xdisplay, windows[i]))
        {
          desktop = windows[i];
          break;
        }
    }

  XFree (windows);

  if (desktop == None)
    return FALSE;

  window->xdesktop = desktop;
  window->desktop = create_window_from_xwindow (desktop);

  return TRUE;
}

static Window
get_desktop_manager (GfBackgroundWindow *window)
{
  GdkDisplay *display;
  Display *xdisplay;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  if (window->desktop_manager_atom == None)
    {
      GdkScreen *screen;
      gint screen_number;
      gchar *name;

      screen = gdk_display_get_default_screen (display);
      screen_number = gdk_screen_get_number (screen);

      name = g_strdup_printf ("_NET_DESKTOP_MANAGER_S%d", screen_number);
      window->desktop_manager_atom = XInternAtom (xdisplay, name, False);
      g_free (name);
    }

  return XGetSelectionOwner (xdisplay, window->desktop_manager_atom);
}

static void
handle_xevent (GfBackgroundWindow *window,
               XEvent             *event)
{
  static gboolean is_desktop_above_background;

  if (event->type == DestroyNotify && window->xdesktop != None)
    {
      if (window->xdesktop == event->xdestroywindow.window)
        {
          is_desktop_above_background = FALSE;

          window->xdesktop = None;
          g_clear_object (&window->desktop);

          return;
        }
    }

  if (get_desktop_manager (window) == None)
    return;

  if (window->xdesktop == None && get_desktop (window) == FALSE)
    return;

  if (event->type == ConfigureNotify)
    {
      if (event->xconfigure.window == window->xdesktop)
        is_desktop_above_background = FALSE;
      else if (event->xconfigure.window == window->xbackground)
        is_desktop_above_background = FALSE;
    }

  if (is_desktop_above_background == FALSE)
    {
      if (window->desktop == NULL)
        return;

      gdk_window_restack (window->desktop, window->background, TRUE);
      gdk_window_lower (window->background);

      is_desktop_above_background = TRUE;
    }
}

static GdkFilterReturn
event_filter_func (GdkXEvent *xevent,
                   GdkEvent  *event,
                   gpointer   data)
{
  GfBackgroundWindow *window;
  XEvent *e;

  window = GF_BACKGROUND_WINDOW (data);
  e = (GdkXEvent *) xevent;

  switch (e->type)
    {
      case CirculateNotify:
      case ConfigureNotify:
      case CreateNotify:
      case DestroyNotify:
      case GravityNotify:
      case MapNotify:
      case MappingNotify:
      case ReparentNotify:
      case UnmapNotify:
      case VisibilityNotify:
        handle_xevent (window, e);
        break;

      default:
        break;
    }

  return GDK_FILTER_CONTINUE;
}

static void
update_wm_hints (Display *display,
                 Window   window)
{
  XWMHints wm_hints;

  wm_hints.flags = InputHint;
  wm_hints.input = False;

  gdk_error_trap_push ();
  XSetWMHints (display, window, &wm_hints);
  gdk_error_trap_pop_ignored ();
}

static void
update_wm_protocols (Display *display,
                     Window   window)
{
  Atom *protocols;
  int count;
  Atom take_focus;
  int i;

  if (XGetWMProtocols (display, window, &protocols, &count) == 0)
    return;

  take_focus = XInternAtom (display, "WM_TAKE_FOCUS", False);

  for (i = 0; i < count; i++)
    {
      if (protocols[i] == take_focus)
        {
          protocols[i] = None;
          break;
        }
    }

  XSetWMProtocols (display, window, protocols, count);
  XFree (protocols);
}

static void
set_no_input_and_no_focus (GdkWindow *window)
{
  GdkDisplay *display;
  Display *xdisplay;
  Window xwindow;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);
  xwindow = gdk_x11_window_get_xid (window);

  update_wm_hints (xdisplay, xwindow);
  update_wm_protocols (xdisplay, xwindow);
}

static void
gf_background_window_screen_changed (GdkScreen *screen,
                                     gpointer   user_data)
{
  GfBackgroundWindow *window;
  GtkWidget *widget;
  gint width;
  gint height;

  window = GF_BACKGROUND_WINDOW (user_data);
  widget = GTK_WIDGET (window);

  width = gdk_screen_get_width (screen);
  height = gdk_screen_get_height (screen);

  gtk_widget_set_size_request (widget, width, height);
}

static void
gf_background_window_dispose (GObject *object)
{
  GfBackgroundWindow *window;

  window = GF_BACKGROUND_WINDOW (object);

  g_clear_object (&window->desktop);

  G_OBJECT_CLASS (gf_background_window_parent_class)->dispose (object);
}

static void
gf_background_window_finalize (GObject *object)
{
  GfBackgroundWindow *window;
  GdkScreen *screen;
  GdkWindow *root;

  window = GF_BACKGROUND_WINDOW (object);

  screen = gdk_screen_get_default ();
  root = gdk_screen_get_root_window (screen);

  gdk_window_remove_filter (root, event_filter_func, window);

  G_OBJECT_CLASS (gf_background_window_parent_class)->finalize (object);
}

static void
gf_background_window_map (GtkWidget *widget)
{
  GfBackgroundWindow *window;

  window = GF_BACKGROUND_WINDOW (widget);

  GTK_WIDGET_CLASS (gf_background_window_parent_class)->map (widget);

  gdk_window_lower (window->background);
}

static void
gf_background_window_realize (GtkWidget *widget)
{
  GfBackgroundWindow *window;
  GdkWindow *gdk_window;

  window = GF_BACKGROUND_WINDOW (widget);

  GTK_WIDGET_CLASS (gf_background_window_parent_class)->realize (widget);

  gdk_window = gtk_widget_get_window (widget);

  window->xbackground = gdk_x11_window_get_xid (gdk_window);
  window->background = gdk_window;

  set_no_input_and_no_focus (gdk_window);
}

static void
gf_background_window_class_init (GfBackgroundWindowClass *window_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class= G_OBJECT_CLASS (window_class);
  widget_class = GTK_WIDGET_CLASS (window_class);

  object_class->dispose = gf_background_window_dispose;
  object_class->finalize = gf_background_window_finalize;

  widget_class->map = gf_background_window_map;
  widget_class->realize = gf_background_window_realize;
}

static void
gf_background_window_init (GfBackgroundWindow *window)
{
  GdkScreen *screen;
  GtkWindow *gtk_window;
  GtkWidget *gtk_widget;
  GdkWindow *root;
  GdkEventMask mask;

  screen = gdk_screen_get_default ();
  gtk_window = GTK_WINDOW (window);
  gtk_widget = GTK_WIDGET (window);

  g_signal_connect_object (screen, "monitors-changed",
                           G_CALLBACK (gf_background_window_screen_changed),
                           window, G_CONNECT_AFTER);
  g_signal_connect_object (screen, "size-changed",
                           G_CALLBACK (gf_background_window_screen_changed),
                           window, G_CONNECT_AFTER);

  gf_background_window_screen_changed (screen, window);
  gtk_window_set_keep_below (gtk_window, TRUE);
  gtk_widget_add_events (gtk_widget, GDK_STRUCTURE_MASK);
  gtk_widget_realize (gtk_widget);

  root = gdk_screen_get_root_window (screen);
  mask = gdk_window_get_events (root);

  gdk_window_set_events (root, mask | GDK_SUBSTRUCTURE_MASK);
  gdk_window_add_filter (root, event_filter_func, window);
}

GtkWidget *
gf_background_window_new (void)
{
  return g_object_new (GF_TYPE_BACKGROUND_WINDOW,
                       "accept-focus", FALSE,
                       "app-paintable", TRUE,
                       "decorated", FALSE,
                       "resizable", FALSE,
                       "skip-pager-hint", TRUE,
                       "skip-taskbar-hint", TRUE,
                       "type", GTK_WINDOW_TOPLEVEL,
                       "type-hint", GDK_WINDOW_TYPE_HINT_DESKTOP,
                        NULL);
}
