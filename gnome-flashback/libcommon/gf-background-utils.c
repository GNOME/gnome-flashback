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

static cairo_surface_t *
get_surface_as_image_surface (cairo_surface_t *surface)
{
  cairo_surface_t *image;
  cairo_t *cr;

  if (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE)
    return cairo_surface_reference (surface);

  image = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                      cairo_xlib_surface_get_width (surface),
                                      cairo_xlib_surface_get_height (surface));

  cr = cairo_create (image);

  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);

  return image;
}

static void
get_average_color (cairo_surface_t *surface,
                   GdkRGBA         *color)
{
  cairo_surface_t *image;
  unsigned char *data;
  int width;
  int height;
  int stride;
  guint64 red_total;
  guint64 green_total;
  guint64 blue_total;
  guint64 pixels_total;
  int row;
  int column;

  image = get_surface_as_image_surface (surface);

  data = cairo_image_surface_get_data (image);
  width = cairo_image_surface_get_width (image);
  height = cairo_image_surface_get_height (image);
  stride = cairo_image_surface_get_stride (image);

  red_total = 0;
  green_total = 0;
  blue_total = 0;
  pixels_total = (guint64) width * height;

  for (row = 0; row < height; row++)
    {
      unsigned char *p;

      p = data + (row * stride);

      for (column = 0; column < width; column++)
        {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
          blue_total += *p++;
          green_total += *p++;
          red_total += *p++;
          p++;
#else
          p++;
          red_total += *p++;
          green_total += *p++;
          blue_total += *p++;
#endif
        }
    }

  cairo_surface_destroy (image);

  pixels_total *= 0xff;
  color->red = (double) red_total / pixels_total;
  color->green = (double) green_total / pixels_total;
  color->blue = (double) blue_total / pixels_total;
  color->alpha = 1.0;
}

static void
destroy_color (void *data)
{
  gdk_rgba_free (data);
}

static gboolean
is_valid_pixmap (GdkDisplay *display,
                 Pixmap      pixmap)
{
  Display *xdisplay;
  Window root_return;
  int x_return;
  int y_return;
  unsigned int width_return;
  unsigned int height_return;
  unsigned int border_width_return;
  unsigned int depth_return;
  Status status;

  xdisplay = gdk_x11_display_get_xdisplay (display);

  gdk_x11_display_error_trap_push (display);

  status = XGetGeometry (xdisplay, pixmap, &root_return,
                         &x_return, &y_return, &width_return, &height_return,
                         &border_width_return, &depth_return);

  if (gdk_x11_display_error_trap_pop (display) != 0 || status == 0)
    return FALSE;

  return TRUE;
}

static Pixmap
get_root_pixmap (GdkDisplay *display)
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
  prop = NULL;

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

  if (result != Success ||
      actual_type != XA_PIXMAP ||
      actual_format != 32 ||
      n_items != 1)
    {
      XFree (prop);

      return None;
    }

  pixmap = *(Pixmap *) prop;
  XFree (prop);

  if (!is_valid_pixmap (display, pixmap))
    return None;

  return pixmap;
}

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

cairo_surface_t *
gf_background_surface_create (GdkDisplay *display,
                              GnomeBG    *bg,
                              GdkWindow  *window,
                              int         width,
                              int         height)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkRGBA color;
  int n_monitors;
  int i;

  surface = gdk_window_create_similar_surface (window,
                                               CAIRO_CONTENT_COLOR,
                                               width,
                                               height);

  cr = cairo_create (surface);

  color = (GdkRGBA) {};

  n_monitors = gdk_display_get_n_monitors (display);

  for (i = 0; i < n_monitors; i++)
    {
      GdkMonitor *monitor;
      GdkRectangle geometry;
      cairo_surface_t *monitor_surface;
      GdkRGBA monitor_color;

      monitor = gdk_display_get_monitor (display, i);
      gdk_monitor_get_geometry (monitor, &geometry);

#ifdef HAVE_GNOME_DESKTOP_3_35_4
      monitor_surface = gnome_bg_create_surface (bg,
                                                 window,
                                                 geometry.width,
                                                 geometry.height);
#else
      monitor_surface = gnome_bg_create_surface (bg,
                                                 window,
                                                 geometry.width,
                                                 geometry.height,
                                                 FALSE);
#endif

      cairo_set_source_surface (cr, monitor_surface, geometry.x, geometry.y);
      cairo_paint (cr);

      get_average_color (monitor_surface, &monitor_color);
      cairo_surface_destroy (monitor_surface);

      color.red += monitor_color.red;
      color.green += monitor_color.green;
      color.blue += monitor_color.blue;
    }

  cairo_destroy (cr);

  color.red /= n_monitors;
  color.green /= n_monitors;
  color.blue /= n_monitors;
  color.alpha = 1.0;

  cairo_surface_set_user_data (surface,
                               &average_color_key,
                               gdk_rgba_copy (&color),
                               destroy_color);

  return surface;
}

cairo_surface_t *
gf_background_surface_get_from_root (GdkDisplay *display,
                                     int         width,
                                     int         height)
{
  Display *xdisplay;
  Pixmap root_pixmap;
  GdkScreen *screen;
  GdkWindow *root;
  int scale;
  cairo_surface_t *pixmap_surface;
  cairo_surface_t *surface;

  xdisplay = gdk_x11_display_get_xdisplay (display);

  root_pixmap = get_root_pixmap (display);

  screen = gdk_display_get_default_screen (display);
  root = gdk_screen_get_root_window (screen);
  scale = gdk_window_get_scale_factor (root);

  pixmap_surface = NULL;
  surface = NULL;

  if (root_pixmap != None)
    {
      Visual *xvisual;

      xvisual = DefaultVisual (xdisplay, DefaultScreen (xdisplay));

      pixmap_surface = cairo_xlib_surface_create (xdisplay,
                                                  root_pixmap,
                                                  xvisual,
                                                  width * scale,
                                                  height * scale);
    }

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

GdkRGBA *
gf_background_surface_get_average_color (cairo_surface_t *surface)
{
  return cairo_surface_get_user_data (surface, &average_color_key);
}
