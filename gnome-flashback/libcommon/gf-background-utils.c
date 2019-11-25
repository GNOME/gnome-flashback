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

static Pixmap
get_xrootpmap_id (GdkDisplay *display)
{
  Display *xdisplay;
  Atom xrootpmap_id_atom;
  int result;
  Atom actual_type;
  int actual_format;
  unsigned long n_items;
  unsigned long bytes_after;
  unsigned char *prop;
  Pixmap pixmap;

  xdisplay = gdk_x11_display_get_xdisplay (display);
  xrootpmap_id_atom = XInternAtom (xdisplay, "_XROOTPMAP_ID", False);

  gdk_x11_display_error_trap_push (display);

  result = XGetWindowProperty (xdisplay,
                               XDefaultRootWindow (xdisplay),
                               xrootpmap_id_atom,
                               0l,
                               1l,
                               False,
                               XA_PIXMAP,
                               &actual_type,
                               &actual_format,
                               &n_items,
                               &bytes_after,
                               &prop);

  gdk_x11_display_error_trap_pop_ignored (display);

  if (result != Success ||
      actual_type != XA_PIXMAP ||
      actual_format != 32 ||
      n_items != 1)
    {
      if (prop != NULL)
        XFree (prop);

      return None;
    }

  pixmap = *(Pixmap *) prop;
  XFree (prop);

  return pixmap;
}

cairo_surface_t *
gf_background_surface_create (GdkDisplay *display,
                              GnomeBG    *bg,
                              GdkWindow  *window,
                              int         width,
                              int         height)
{
  return gnome_bg_create_surface (bg, window, width, height, TRUE);
}

cairo_surface_t *
gf_background_surface_get_from_root (GdkDisplay *display,
                                     int         width,
                                     int         height)
{
  Display *xdisplay;
  Pixmap root_pixmap;
  Visual *xvisual;
  GdkScreen *screen;
  GdkWindow *root;
  int scale;
  cairo_surface_t *pixmap_surface;
  cairo_surface_t *surface;

  xdisplay = gdk_x11_display_get_xdisplay (display);

  root_pixmap = get_xrootpmap_id (display);
  xvisual = DefaultVisual (xdisplay, DefaultScreen (xdisplay));

  screen = gdk_display_get_default_screen (display);
  root = gdk_screen_get_root_window (screen);
  scale = gdk_window_get_scale_factor (root);

  pixmap_surface = cairo_xlib_surface_create (xdisplay,
                                              root_pixmap,
                                              xvisual,
                                              width * scale,
                                              height * scale);

  surface = NULL;
  if (pixmap_surface != NULL)
    {
      cairo_t *cr;

      surface = cairo_surface_create_similar (pixmap_surface,
                                              CAIRO_CONTENT_COLOR,
                                              width * scale,
                                              height * scale);

      cr = cairo_create (surface);
      cairo_set_source_surface (cr, pixmap_surface, 0, 0);
      cairo_surface_destroy (pixmap_surface);

      cairo_paint (cr);

      if (cairo_status (cr) != CAIRO_STATUS_SUCCESS)
        g_clear_pointer (&surface, cairo_surface_destroy);

      cairo_destroy (cr);
    }

  if (surface != NULL)
    {
      cairo_surface_set_device_scale (surface, scale, scale);

      return surface;
    }

  return gdk_window_create_similar_surface (root,
                                            CAIRO_CONTENT_COLOR,
                                            width,
                                            height);
}

void
gf_background_surface_set_as_root (GdkDisplay      *display,
                                   cairo_surface_t *surface)
{
  GdkScreen *screen;

  screen = gdk_display_get_default_screen (display);

  gnome_bg_set_surface_as_root (screen, surface);
}
