/*
 * Copyright (C) 2019 Alberts Muktupāvels
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
#include "gf-background-utils.h"

#include <cairo-xlib.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

static const cairo_user_data_key_t average_color_key;

static Pixmap
get_persistent_pixmap (cairo_surface_t *surface)
{
  Display *xdisplay;
  int width;
  int height;
  int depth;
  Pixmap persistent_pixmap;
  Visual *xvisual;
  cairo_surface_t *pixmap_surface;
  double x_scale;
  double y_scale;
  cairo_t *cr;

  xdisplay = XOpenDisplay (NULL);
  if (xdisplay == NULL)
    return None;

  width = cairo_xlib_surface_get_width (surface);
  height = cairo_xlib_surface_get_height (surface);

  /* Desktop background pixmap should be created from
   * dummy X client since most applications will try to
   * kill it with XKillClient later when changing pixmap
   */
  XSetCloseDownMode (xdisplay, RetainPermanent);

  depth = DefaultDepth (xdisplay, DefaultScreen (xdisplay));
  persistent_pixmap = XCreatePixmap (xdisplay,
                                     XDefaultRootWindow (xdisplay),
                                     width,
                                     height,
                                     depth);

  xvisual = DefaultVisual (xdisplay, DefaultScreen (xdisplay));

  pixmap_surface = cairo_xlib_surface_create (xdisplay,
                                              persistent_pixmap,
                                              xvisual,
                                              width,
                                              height);

  cairo_surface_get_device_scale (surface, &x_scale, &y_scale);
  cairo_surface_set_device_scale (pixmap_surface, x_scale, y_scale);

  cr = cairo_create (pixmap_surface);
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (pixmap_surface);

  XCloseDisplay (xdisplay);

  return persistent_pixmap;
}

static void
set_root_pixmap (GdkDisplay *display,
                 Pixmap      pixmap)
{
  Display *xdisplay;
  Atom esetroot_pmap_id_atom;
  Atom xrootpmap_id_atom;
  int result;
  Atom actual_type;
  int actual_format;
  unsigned long n_items;
  unsigned long bytes_after;
  unsigned char *prop;

  xdisplay = gdk_x11_display_get_xdisplay (display);

  esetroot_pmap_id_atom = XInternAtom (xdisplay, "ESETROOT_PMAP_ID", False);
  xrootpmap_id_atom = XInternAtom (xdisplay, "_XROOTPMAP_ID", False);
  prop = NULL;

  result = XGetWindowProperty (xdisplay,
                               XDefaultRootWindow (xdisplay),
                               esetroot_pmap_id_atom,
                               0l,
                               1l,
                               False,
                               XA_PIXMAP,
                               &actual_type,
                               &actual_format,
                               &n_items,
                               &bytes_after,
                               &prop);

  if (result == Success &&
      actual_type == XA_PIXMAP &&
      actual_format == 32 &&
      n_items == 1 &&
      prop != NULL)
    {
      gdk_x11_display_error_trap_push (display);

      XKillClient (xdisplay, *(Pixmap *) prop);

      gdk_x11_display_error_trap_pop_ignored (display);
    }

  XFree (prop);

  XChangeProperty (xdisplay,
                   XDefaultRootWindow (xdisplay),
                   esetroot_pmap_id_atom,
                   XA_PIXMAP,
                   32,
                   PropModeReplace,
                   (guchar *) &pixmap,
                   1);

  XChangeProperty (xdisplay,
                   XDefaultRootWindow (xdisplay),
                   xrootpmap_id_atom,
                   XA_PIXMAP,
                   32,
                   PropModeReplace,
                   (guchar *) &pixmap,
                   1);
}

static void
set_average_color (GdkDisplay *display,
                   GdkRGBA    *color)
{
  Display *xdisplay;
  Atom representative_colors_atom;

  xdisplay = gdk_x11_display_get_xdisplay (display);

  representative_colors_atom = XInternAtom (xdisplay,
                                            "_GNOME_BACKGROUND_REPRESENTATIVE_COLORS",
                                            False);

  if (color != NULL)
    {
      gchar *string;

      string = gdk_rgba_to_string (color);

      XChangeProperty (xdisplay,
                       XDefaultRootWindow (xdisplay),
                       representative_colors_atom,
                       XA_STRING,
                       8,
                       PropModeReplace,
                       (guchar *) string,
                       strlen (string) + 1);

      g_free (string);
    }
  else
    {
      XDeleteProperty (xdisplay,
                       XDefaultRootWindow (xdisplay),
                       representative_colors_atom);
    }
}

void
gf_background_surface_set_as_root (GdkDisplay      *display,
                                   cairo_surface_t *surface)
{
  Display *xdisplay;
  Pixmap pixmap;
  GdkRGBA *color;

  xdisplay = gdk_x11_display_get_xdisplay (display);

  pixmap = get_persistent_pixmap (surface);
  color = cairo_surface_get_user_data (surface, &average_color_key);

  gdk_x11_display_grab (display);

  set_root_pixmap (display, pixmap);
  set_average_color (display, color);

  XSetWindowBackgroundPixmap (xdisplay, XDefaultRootWindow (xdisplay), pixmap);
  XClearWindow (xdisplay, XDefaultRootWindow (xdisplay));
  XFlush (xdisplay);

  gdk_x11_display_ungrab (display);
}
