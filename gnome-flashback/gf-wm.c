/*
 * Copyright (C) 2022 Alberts Muktupāvels
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
#include "gf-wm.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

struct _GfWm
{
  GObject parent;

  Atom    wm_check_atom;
  Atom    wm_name_atom;
  Atom    utf8_string_atom;

  Window  wm_check;
};

G_DEFINE_TYPE (GfWm, gf_wm, G_TYPE_OBJECT)

static char *
get_wm_name (GfWm *self)
{
  GdkDisplay *display;
  Display *xdisplay;
  int result;
  Atom actual_type;
  int actual_format;
  unsigned long n_items;
  unsigned long bytes_after;
  unsigned char *prop;
  char *wm_name;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  gdk_x11_display_error_trap_push (display);

  result = XGetWindowProperty (xdisplay,
                               self->wm_check,
                               self->wm_name_atom,
                               0,
                               G_MAXLONG,
                               False,
                               self->utf8_string_atom,
                               &actual_type,
                               &actual_format,
                               &n_items,
                               &bytes_after,
                               &prop);

  gdk_x11_display_error_trap_pop_ignored (display);

  if (result != Success ||
      actual_type != self->utf8_string_atom ||
      actual_format != 8 ||
      n_items == 0)
    {
      XFree (prop);
      return NULL;
    }

  wm_name = g_strndup ((const char *) prop, n_items);
  XFree (prop);

  if (!g_utf8_validate (wm_name, -1, NULL))
    {
      g_free (wm_name);
      return NULL;
    }

  return wm_name;
}

static void
update_gnome_wm_keybindings (GfWm *self)
{
  GdkDisplay *display;
  Display *xdisplay;
  char *wm_name;
  char *gnome_wm_keybindings;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  wm_name = get_wm_name (self);
  gnome_wm_keybindings = g_strdup_printf ("%s,GNOME Flashback",
                                          wm_name ? wm_name : "Unknown");

  gdk_x11_display_error_trap_push (display);

  XChangeProperty (xdisplay,
                   self->wm_check,
                   XInternAtom (xdisplay, "_GNOME_WM_KEYBINDINGS", False),
                   self->utf8_string_atom,
                   8,
                   PropModeReplace,
                   (guchar *) gnome_wm_keybindings,
                   strlen (gnome_wm_keybindings));

  gdk_x11_display_error_trap_pop_ignored (display);

  g_free (gnome_wm_keybindings);
  g_free (wm_name);
}

static Window
get_net_supporting_wm_check (GfWm       *self,
                             GdkDisplay *display,
                             Window      window)
{
  Display *xdisplay;
  int result;
  Atom actual_type;
  int actual_format;
  unsigned long n_items;
  unsigned long bytes_after;
  unsigned char *prop;
  Window wm_check;

  xdisplay = gdk_x11_display_get_xdisplay (display);

  gdk_x11_display_error_trap_push (display);

  result = XGetWindowProperty (xdisplay,
                               window,
                               self->wm_check_atom,
                               0,
                               G_MAXLONG,
                               False,
                               XA_WINDOW,
                               &actual_type,
                               &actual_format,
                               &n_items,
                               &bytes_after,
                               &prop);

  gdk_x11_display_error_trap_pop_ignored (display);

  if (result != Success ||
      actual_type != XA_WINDOW ||
      n_items == 0)
    {
      XFree (prop);
      return None;
    }

  wm_check = *(Window *) prop;
  XFree (prop);

  return wm_check;
}

static void
update_wm_check (GfWm *self)
{
  GdkDisplay *display;
  Display *xdisplay;
  Window wm_check;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  wm_check = get_net_supporting_wm_check (self,
                                          display,
                                          XDefaultRootWindow (xdisplay));

  if (wm_check == None)
    return;

  if (wm_check != get_net_supporting_wm_check (self, display, wm_check))
    return;

  self->wm_check = wm_check;

  gdk_x11_display_error_trap_push (display);
  XSelectInput (xdisplay, self->wm_check, StructureNotifyMask);

  if (gdk_x11_display_error_trap_pop (display) != 0)
    {
      self->wm_check = None;
    }

  update_gnome_wm_keybindings (self);
}

static GdkFilterReturn
event_filter_cb (GdkXEvent *xevent,
                 GdkEvent  *event,
                 gpointer   data)
{
  GfWm *self;
  XEvent *e;

  self = GF_WM (data);
  e = (XEvent *) xevent;

  if (self->wm_check != None)
    {
      if (e->type == DestroyNotify &&
          e->xdestroywindow.window == self->wm_check)
        {
          update_wm_check (self);
        }
      else if (e->type == PropertyNotify &&
               e->xproperty.window == self->wm_check)
        {
          if (e->xproperty.atom == self->wm_check_atom)
            update_wm_check (self);
          else if (e->xproperty.atom == self->wm_name_atom)
            update_gnome_wm_keybindings (self);
        }
    }
  else
    {
      GdkDisplay *display;
      Display *xdisplay;

      display = gdk_display_get_default ();
      xdisplay = gdk_x11_display_get_xdisplay (display);

      if (e->type == PropertyNotify &&
          e->xproperty.window == XDefaultRootWindow (xdisplay) &&
          e->xproperty.atom == self->wm_check_atom)
        {
          update_wm_check (self);
        }
    }

  return GDK_FILTER_CONTINUE;
}

static void
gf_wm_finalize (GObject *object)
{
  GfWm *self;

  self = GF_WM (object);

  gdk_window_remove_filter (NULL, event_filter_cb, self);

  G_OBJECT_CLASS (gf_wm_parent_class)->finalize (object);
}

static void
gf_wm_class_init (GfWmClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->finalize = gf_wm_finalize;
}

static void
gf_wm_init (GfWm *self)
{
  GdkDisplay *display;
  Display *xdisplay;
  Window xroot;
  XWindowAttributes attrs;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);
  xroot = DefaultRootWindow (xdisplay);

  self->wm_check_atom = XInternAtom (xdisplay,
                                     "_NET_SUPPORTING_WM_CHECK",
                                     False);

  self->wm_name_atom = XInternAtom (xdisplay, "_NET_WM_NAME", False);
  self->utf8_string_atom = XInternAtom (xdisplay, "UTF8_STRING", False);

  gdk_window_add_filter (NULL, event_filter_cb, self);

  XGetWindowAttributes (xdisplay, xroot, &attrs);
  XSelectInput (xdisplay, xroot, PropertyChangeMask | attrs.your_event_mask);
  XSync (xdisplay, False);

  update_wm_check (self);
}

GfWm *
gf_wm_new (void)
{
  return g_object_new (GF_TYPE_WM, NULL);
}
