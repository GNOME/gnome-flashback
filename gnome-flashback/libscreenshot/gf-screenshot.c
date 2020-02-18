/*
 * Copyright (C) 2015-2017 Alberts Muktupāvels
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
 *
 * Based on code in gnome-screenshot:
 * https://git.gnome.org/browse/gnome-screenshot/tree/src/screenshot-utils.c
 * Copyright (C) 2001 - 2006 Jonathan Blandford, 2008 Cosimo Cecchi
 */

#include "config.h"
#include "gf-screenshot.h"

#include <canberra-gtk.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xatom.h>

#include "dbus/gf-screenshot-gen.h"
#include "gf-flashspot.h"
#include "gf-select-area.h"

#define SCREENSHOT_DBUS_NAME "org.gnome.Shell.Screenshot"
#define SCREENSHOT_DBUS_PATH "/org/gnome/Shell/Screenshot"

typedef void (*GfInvocationCallback) (GfScreenshotGen       *screenshot_gen,
                                      GDBusMethodInvocation *invocation,
                                      gboolean               result,
                                      const gchar           *filename);

struct _GfScreenshot
{
  GObject          parent;

  GfScreenshotGen *screenshot_gen;
  gint             bus_name;

  GHashTable      *senders;

  GSettings       *lockdown;

  GDateTime       *datetime;
};

typedef struct
{
  GfScreenshot *screenshot;
  gchar        *sender;
} FlashspotData;

typedef enum
{
  SCREENSHOT_SCREEN,
  SCREENSHOT_WINDOW,
  SCREENSHOT_AREA
} ScreenshotType;

G_DEFINE_TYPE (GfScreenshot, gf_screenshot, G_TYPE_OBJECT)

static void
free_pixels (guchar   *pixels,
             gpointer  data)
{
  g_free (pixels);
}

static GdkPixbuf *
pixels_to_pixbuf (gulong *pixels,
                  gint    width,
                  gint    height)
{
  guchar *data;
  gint i, j;

  data = g_new0 (guchar, width * height * 4);

  for (i = j = 0; i < width * height; i++, j += 4)
    {
      guint32 argb;

      argb = (guint32) pixels[i];
      argb = (argb << 8) | (argb >> 24);

      data[j] = argb >> 24;
      data[j + 1] = (argb >> 16) & 0xff;
      data[j + 2] = (argb >> 8) & 0xff;
      data[j + 3] = argb & 0xff;
    }

  return gdk_pixbuf_new_from_data (data, GDK_COLORSPACE_RGB, TRUE, 8,
                                   width, height, width * 4,
                                   free_pixels, NULL);
}

static GdkPixbuf *
get_cursor_pixbuf (GdkDisplay *display,
                   gint       *xhot,
                   gint       *yhot)
{
  Display *xdisplay;
  gint event_base;
  gint error_base;
  XFixesCursorImage *image;
  GdkPixbuf *pixbuf;

  xdisplay = gdk_x11_display_get_xdisplay (display);

  if (!XFixesQueryExtension (xdisplay, &event_base, &error_base))
    return NULL;

  image = XFixesGetCursorImage (xdisplay);

  if (!image)
    return NULL;

  pixbuf = pixels_to_pixbuf (image->pixels, image->width, image->height);

  *xhot = image->xhot;
  *yhot = image->yhot;

  XFree (image);

  return pixbuf;
}

static gint
get_window_scaling_factor (void)
{
  GValue gvalue = G_VALUE_INIT;
  GdkScreen *screen;

  g_value_init (&gvalue, G_TYPE_INT);

  screen = gdk_screen_get_default ();
  if (gdk_screen_get_setting (screen, "gdk-window-scaling-factor", &gvalue))
    return g_value_get_int (&gvalue);

  return 1;
}

static void
screenshot_add_cursor (GdkPixbuf      *pixbuf,
                       ScreenshotType  type,
                       gboolean        include_cursor,
                       GdkWindow      *window,
                       gint            frame_offset_x,
                       gint            frame_offset_y)
{
  GdkDisplay *display;
  GdkPixbuf *cursor_pixbuf;
  gint scale;
  gint xhot;
  gint yhot;

  /* If we have a selected area, there were by definition no cursor
   * in the screenshot.
   */
  if (!include_cursor || type == SCREENSHOT_AREA)
    return;

  display = gdk_display_get_default ();
  cursor_pixbuf = get_cursor_pixbuf (display, &xhot, &yhot);
  scale = get_window_scaling_factor ();

  if (cursor_pixbuf == NULL)
    {
      GdkCursor *cursor;
      cairo_surface_t *surface;
      gdouble x_hot;
      gdouble y_hot;
      gint width;
      gint height;

      cursor = gdk_cursor_new_for_display (display, GDK_LEFT_PTR);

      if (cursor == NULL)
        return;

      surface = gdk_cursor_get_surface (cursor, &x_hot, &y_hot);
      g_object_unref (cursor);

      if (surface == NULL)
        return;

      width = cairo_image_surface_get_width (surface);
      height = cairo_image_surface_get_height (surface);

      cursor_pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
      cairo_surface_destroy (surface);

      xhot = x_hot * scale;
      yhot = y_hot * scale;
    }

  if (cursor_pixbuf != NULL)
    {
      GdkSeat *seat;
      GdkDevice *device;
      gdouble cx;
      gdouble cy;
      GdkRectangle pixbuf_rect;
      GdkRectangle cursor_rect;

      seat = gdk_display_get_default_seat (display);
      device = gdk_seat_get_pointer (seat);

      gdk_window_get_device_position_double (window, device, &cx, &cy, NULL);

      pixbuf_rect.x = pixbuf_rect.y = 0;
      pixbuf_rect.width = gdk_pixbuf_get_width (pixbuf);
      pixbuf_rect.height = gdk_pixbuf_get_height (pixbuf);

      cursor_rect.x = cx * scale - xhot - frame_offset_x;
      cursor_rect.y = cy * scale - yhot - frame_offset_y;
      cursor_rect.width = gdk_pixbuf_get_width (cursor_pixbuf);
      cursor_rect.height = gdk_pixbuf_get_height (cursor_pixbuf);

      /* see if the pointer is inside the window */
      if (gdk_rectangle_intersect (&pixbuf_rect, &cursor_rect, &cursor_rect))
        {
          gint cursor_x;
          gint cursor_y;

          cursor_x = cx * scale - xhot - frame_offset_x;
          cursor_y = cy * scale - yhot - frame_offset_y;

          gdk_pixbuf_composite (cursor_pixbuf, pixbuf,
                                cursor_rect.x, cursor_rect.y,
                                cursor_rect.width, cursor_rect.height,
                                cursor_x, cursor_y,
                                1.0, 1.0,
                                GDK_INTERP_BILINEAR,
                                255);
        }

      g_object_unref (cursor_pixbuf);
    }
}

static void
play_sound_effect (const char *event_id,
                   const char *event_desc)
{
  ca_context *c;

  c = ca_gtk_context_get ();

  ca_context_play (c, 0,
                   CA_PROP_EVENT_ID, event_id,
                   CA_PROP_EVENT_DESCRIPTION, event_desc,
                   CA_PROP_CANBERRA_CACHE_CONTROL, "permanent",
                   NULL);
}

static void
save_to_clipboard (GfScreenshot *self,
                   GdkPixbuf    *pixbuf)
{
  GdkDisplay *display;
  GtkClipboard *clipboard;

  display = gdk_display_get_default ();
  clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_CLIPBOARD);

  play_sound_effect ("screen-capture", _("Screenshot taken"));

  gtk_clipboard_set_image (clipboard, pixbuf);
  g_object_unref (pixbuf);
}

static gchar *
get_unique_path (const gchar *path,
                 const gchar *filename)
{
  gchar *ptr;
  gchar *real_filename;
  gchar *real_path;
  gint idx;

  ptr = g_strrstr (filename, ".png");

  if (ptr != NULL)
    real_filename = g_strndup (filename, ptr - filename);
  else
    real_filename = g_strdup (filename);

  real_path = NULL;
  idx = 0;

  do
    {
      gchar *name;

      if (idx == 0)
        name = g_strdup_printf ("%s.png", real_filename);
      else
        name = g_strdup_printf ("%s - %d.png", real_filename, idx);

      g_free (real_path);
      real_path = g_build_filename (path, name, NULL);
      g_free (name);

      idx++;
    }
  while (g_file_test (real_path, G_FILE_TEST_EXISTS));

  g_free (real_filename);

  return real_path;
}

static gchar *
get_filename (const gchar *filename)
{
  const gchar *path;

  if (g_path_is_absolute (filename))
    return g_strdup (filename);

  path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);

  if (path == NULL || !g_file_test (path, G_FILE_TEST_EXISTS))
    {
      path = g_get_home_dir ();

      if (!g_file_test (path, G_FILE_TEST_EXISTS))
        return NULL;
    }

  return get_unique_path (path, filename);
}

static gboolean
save_screenshot (GfScreenshot  *screenshot,
                 GdkPixbuf     *pixbuf,
                 const gchar   *filename_in,
                 gchar        **filename_out)
{
  gboolean result;
  gchar *filename;
  gchar *creation_time;
  GError *error;

  if (pixbuf == NULL)
    return FALSE;

  if (filename_in == NULL || *filename_in == '\0')
    {
      save_to_clipboard (screenshot, pixbuf);
      return TRUE;
    }

  filename = get_filename (filename_in);

  creation_time = g_date_time_format (screenshot->datetime, "%c");

  error = NULL;
  result = gdk_pixbuf_save (pixbuf, filename, "png", &error,
                            "tEXt::Creation Time", creation_time,
                            NULL);

  if (result)
    *filename_out = filename;
  else
    g_free (filename);

  if (error != NULL)
    {
      g_warning ("Failed to save screenshot: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (pixbuf);
  g_free (creation_time);

  return result;
}

static void
blank_rectangle_in_pixbuf (GdkPixbuf    *pixbuf,
                           GdkRectangle *rect)
{
  gint x;
  gint y;
  gint x2;
  gint y2;
  guchar *pixels;
  gint rowstride;
  gint n_channels;
  guchar *row;
  gboolean has_alpha;

  g_assert (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);

  x2 = rect->x + rect->width;
  y2 = rect->y + rect->height;

  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
  n_channels = gdk_pixbuf_get_n_channels (pixbuf);

  for (y = rect->y; y < y2; y++)
    {
      guchar *p;

      row = pixels + y * rowstride;
      p = row + rect->x * n_channels;

      for (x = rect->x; x < x2; x++)
        {
          *p++ = 0;
          *p++ = 0;
          *p++ = 0;

          if (has_alpha)
            *p++ = 255;
        }
    }
}

static void
blank_region_in_pixbuf (GdkPixbuf      *pixbuf,
                        cairo_region_t *region)
{
  gint n_rects;
  gint i;
  gint width;
  gint height;
  cairo_rectangle_int_t pixbuf_rect;

  n_rects = cairo_region_num_rectangles (region);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  pixbuf_rect.x = 0;
  pixbuf_rect.y = 0;
  pixbuf_rect.width = width;
  pixbuf_rect.height = height;

  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_rectangle_int_t dest;

      cairo_region_get_rectangle (region, i, &rect);

      if (gdk_rectangle_intersect (&rect, &pixbuf_rect, &dest))
        blank_rectangle_in_pixbuf (pixbuf, &dest);
    }
}

static cairo_region_t *
make_region_with_monitors (void)
{
  GdkDisplay *display;
  gint num_monitors;
  cairo_region_t *region;
  gint i;

  display = gdk_display_get_default ();
  num_monitors = gdk_display_get_n_monitors (display);
  region = cairo_region_create ();

  for (i = 0; i < num_monitors; i++)
    {
      GdkMonitor *monitor;
      gint scale;
      GdkRectangle rect;

      monitor = gdk_display_get_monitor (display, i);
      scale = gdk_monitor_get_scale_factor (monitor);

      gdk_monitor_get_geometry (monitor, &rect);

      rect.x *= scale;
      rect.y *= scale;
      rect.width *= scale;
      rect.height *= scale;

      cairo_region_union_rectangle (region, &rect);
    }

  return region;
}

static void
get_screen_size (gint *width,
                 gint *height,
                 gint  scale)
{
  GdkScreen *screen;
  GdkWindow *window;

  screen = gdk_screen_get_default ();
  window = gdk_screen_get_root_window (screen);

  *width = gdk_window_get_width (window) / scale;
  *height = gdk_window_get_height (window) / scale;
}

static void
mask_monitors (GdkPixbuf *pixbuf,
               GdkWindow *root_window)
{
  cairo_rectangle_int_t rect;
  cairo_region_t *invisible_region;
  cairo_region_t *region_with_monitors;

  rect.x = rect.y = 0;
  get_screen_size (&rect.width, &rect.height, 1);

  invisible_region = cairo_region_create_rectangle (&rect);
  region_with_monitors = make_region_with_monitors ();

  cairo_region_subtract (invisible_region, region_with_monitors);
  blank_region_in_pixbuf (pixbuf, invisible_region);

  cairo_region_destroy (region_with_monitors);
  cairo_region_destroy (invisible_region);
}

static Window
find_wm_window (GdkDisplay *display,
                GdkWindow  *window)
{
  Display *xdisplay;
  Window xid;
  Window root;
  Window parent;
  Window *children;
  unsigned int nchildren;

  if (window == gdk_get_default_root_window ())
    return None;

  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  xid = GDK_WINDOW_XID (window);

  while (TRUE)
    {
      if (XQueryTree (xdisplay, xid, &root, &parent, &children, &nchildren) == 0)
        {
          g_warning ("Couldn't find window manager window");
          return None;
        }

      if (root == parent)
        {
          if (children != NULL)
            XFree (children);

          return xid;
        }

      xid = parent;

      if (children != NULL)
        XFree (children);
    }
}

static gboolean
get_gtk_frame_extents (GdkWindow *window,
                       GtkBorder *extents)
{
  GdkDisplay *display;
  Display *xdisplay;
  Window xwindow;
  Atom gtk_frame_extents;
  Atom type;
  gint format;
  gulong n_items;
  gulong bytes_after;
  guchar *data;
  gint result;
  gulong *borders;
  gint scale;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);
  xwindow = gdk_x11_window_get_xid (window);
  gtk_frame_extents = XInternAtom (xdisplay, "_GTK_FRAME_EXTENTS", False);
  data = NULL;

  gdk_x11_display_error_trap_push (display);
  result = XGetWindowProperty (xdisplay, xwindow, gtk_frame_extents,
                               0, G_MAXLONG, False, XA_CARDINAL,
                               &type, &format, &n_items, &bytes_after, &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (data == NULL)
    return FALSE;

  if (result != Success || type != XA_CARDINAL || format != 32 || n_items != 4)
    {
      XFree (data);

      return FALSE;
    }

  borders = (gulong *) data;
  scale = get_window_scaling_factor ();

  extents->left = borders[0] / scale;
  extents->right = borders[1] / scale;
  extents->top = borders[2] / scale;
  extents->bottom = borders[3] / scale;

  XFree (data);

  return TRUE;
}

static void
get_window_rect_coords (GdkWindow    *window,
                        gboolean      include_frame,
                        GdkRectangle *real_out,
                        GdkRectangle *screenshot_out)
{
  GdkRectangle real;
  gint x;
  gint y;
  gint width;
  gint height;
  gint scale;
  gint screen_width;
  gint screen_height;

  if (include_frame)
    {
      gdk_window_get_frame_extents (window, &real);
    }
  else
    {
      real.width = gdk_window_get_width (window);
      real.height = gdk_window_get_height (window);

      gdk_window_get_origin (window, &real.x, &real.y);
    }

  if (real_out != NULL)
    *real_out = real;

  x = real.x;
  y = real.y;
  width = real.width;
  height = real.height;

  if (x < 0)
    {
      width = width + x;
      x = 0;
    }

  if (y < 0)
    {
      height = height + y;
      y = 0;
    }

  scale = get_window_scaling_factor ();
  get_screen_size (&screen_width, &screen_height, scale);

  if (x + width > screen_width)
    width = screen_width - x;

  if (y + height > screen_height)
    height = screen_height - y;

  if (screenshot_out != NULL)
    {
      screenshot_out->x = x;
      screenshot_out->y = y;
      screenshot_out->width = width;
      screenshot_out->height = height;
    }
}

static gboolean
window_is_desktop (GdkWindow *window)
{
  GdkWindow *root_window;
  GdkWindowTypeHint type_hint;

  root_window = gdk_get_default_root_window ();

  if (window == root_window)
    return TRUE;

  type_hint = gdk_window_get_type_hint (window);

  if (type_hint == GDK_WINDOW_TYPE_HINT_DESKTOP)
    return TRUE;

  return FALSE;
}

static Window
get_active_window (void)
{
  GdkDisplay *display;
  Display *xdisplay;
  Atom _net_active_window;
  int status;
  Atom actual_type;
  int actual_format;
  unsigned long n_items;
  unsigned long bytes_after;
  unsigned char *prop;
  Window window;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  _net_active_window = XInternAtom (xdisplay, "_NET_ACTIVE_WINDOW", True);
  if (_net_active_window == None)
    return None;

  gdk_x11_display_error_trap_push (display);

  prop = NULL;
  status = XGetWindowProperty (xdisplay,
                               DefaultRootWindow (xdisplay),
                               _net_active_window,
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

  if (status != Success ||
      actual_type != XA_WINDOW)
    {
      XFree (prop);

      return None;
    }

  window = *(Window *) prop;
  XFree (prop);

  return window;
}

static GdkWindow *
find_active_window (void)
{
  Window xwindow;
  GdkDisplay *display;

  xwindow = get_active_window ();
  if (xwindow == None)
    return NULL;

  display = gdk_display_get_default ();

  return gdk_x11_window_foreign_new_for_display (display, xwindow);
}

static GdkWindow *
find_current_window (GdkDisplay *display)
{
  GdkWindow *window;
  GdkWindow *toplevel;

  window = find_active_window ();

  if (window == NULL)
    {
      GdkSeat *seat;
      GdkDevice *device;

      seat = gdk_display_get_default_seat (display);
      device = gdk_seat_get_pointer (seat);

      window = gdk_device_get_window_at_position (device, NULL, NULL);

      if (window)
        g_object_ref (window);
    }

  if (window)
    {
      if (window_is_desktop (window))
        {
          g_object_unref (window);
          return NULL;
        }

      toplevel = gdk_window_get_toplevel (window);

      g_object_unref (window);
      return toplevel;
    }

  return NULL;
}

static GdkWindow *
get_current_window (GdkDisplay     *display,
                    ScreenshotType  type)
{
  if (type == SCREENSHOT_WINDOW)
    return find_current_window (display);

  return gdk_get_default_root_window ();
}

static gboolean
take_screenshot_real (GfScreenshot    *screenshot,
                      ScreenshotType   type,
                      gboolean         include_frame,
                      gboolean         include_cursor,
                      gint            *x,
                      gint            *y,
                      gint            *width,
                      gint            *height,
                      const gchar     *filename_in,
                      gchar          **filename_out)
{
  GdkDisplay *display;
  GdkWindow *window;
  gint scale;
  GtkBorder extents;
  GdkRectangle real;
  GdkRectangle s;
  Window wm;
  GdkWindow *wm_window;
  GtkBorder frame_offset;
  GdkWindow *root;
  GdkPixbuf *pixbuf;

  display = gdk_display_get_default ();
  window = get_current_window (display, type);

  if (window == NULL)
    return FALSE;

  scale = get_window_scaling_factor ();

  if (type == SCREENSHOT_WINDOW && get_gtk_frame_extents (window, &extents))
    {
      gdk_window_get_frame_extents (window, &real);

      if (include_frame)
        {
          real.x = 0;
          real.y = 0;
        }
      else
        {
          real.x = extents.left;
          real.y = extents.top;
          real.width -= extents.left + extents.right;
          real.height -= extents.top + extents.bottom;
        }

      pixbuf = gdk_pixbuf_get_from_window (window, real.x, real.y,
                                           real.width, real.height);

      screenshot_add_cursor (pixbuf, type, include_cursor, window,
                             real.x * scale, real.y *scale);

      gdk_window_get_frame_extents (window, &real);

      *x = real.x;
      *y = real.y;
      *width = real.width;
      *height = real.height;

      if (!include_frame)
        {
          *x += extents.left;
          *y += extents.top;
          *width -= extents.left + extents.right;
          *height -= extents.top + extents.bottom;
        }

      return save_screenshot (screenshot, pixbuf, filename_in, filename_out);
    }

  get_window_rect_coords (window, include_frame, &real, &s);

  wm = find_wm_window (display, window);

  if (wm != None)
    {
      GdkRectangle wm_real;

      wm_window = gdk_x11_window_foreign_new_for_display (display, wm);
      get_window_rect_coords (wm_window, FALSE, &wm_real, NULL);

      frame_offset.left = (gdouble) (real.x - wm_real.x);
      frame_offset.top = (gdouble) (real.y - wm_real.y);
      frame_offset.right = (gdouble) (wm_real.width - real.width - frame_offset.left);
      frame_offset.bottom = (gdouble) (wm_real.height - real.height - frame_offset.top);
    }
  else
    {
      wm_window = NULL;

      frame_offset.left = 0;
      frame_offset.top = 0;
      frame_offset.right = 0;
      frame_offset.bottom = 0;
    }

  if (type != SCREENSHOT_WINDOW)
    {
      s.x = *x - s.x;
      s.y = *y - s.y;
      s.width  = *width;
      s.height = *height;
    }

  root = gdk_get_default_root_window ();
  pixbuf = gdk_pixbuf_get_from_window (root, s.x, s.y, s.width, s.height);

  if (type != SCREENSHOT_WINDOW && type != SCREENSHOT_AREA)
    mask_monitors (pixbuf, root);

  if (include_frame && wm != None)
    {
      XRectangle *rectangles;
      GdkPixbuf *tmp;
      gint rectangle_count;
      gint rectangle_order;
      gint i;

      rectangles = XShapeGetRectangles (GDK_DISPLAY_XDISPLAY (display),
                                        wm, ShapeBounding,
                                        &rectangle_count, &rectangle_order);

      if (rectangles && rectangle_count > 0)
        {
          gboolean has_alpha;

          has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

          tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
                                s.width * scale, s.height * scale);

          gdk_pixbuf_fill (tmp, 0);

          for (i = 0; i < rectangle_count; i++)
            {
              gint rec_x;
              gint rec_y;
              gint rec_width;
              gint rec_height;
              gint screen_width;
              gint screen_height;
              gint y2;

              /* If we're using invisible borders, the ShapeBounding might not
               * have the same size as the frame extents, as it would include
               * the areas for the invisible borders themselves.
               * In that case, trim every rectangle we get by the offset between
               * the WM window size and the frame extents.
               */
              rec_x = rectangles[i].x;
              rec_y = rectangles[i].y;
              rec_width = rectangles[i].width;
              rec_height = rectangles[i].height;

              rec_width -= frame_offset.left * scale + frame_offset.right * scale;
              rec_height -= frame_offset.top * scale + frame_offset.bottom * scale;

              if (real.x < 0)
                {
                  rec_x += real.x * scale;
                  rec_x = MAX(rec_x, 0);
                  rec_width += real.x * scale;
                }

              if (real.y < 0)
                {
                  rec_y += real.y * scale;
                  rec_y = MAX(rec_y, 0);
                  rec_height += real.y * scale;
                }

              get_screen_size (&screen_width, &screen_height, 1);

              if (s.x * scale + rec_x + rec_width > screen_width)
                rec_width = screen_width - s.x * scale - rec_x;

              if (s.y * scale + rec_y + rec_height > screen_height)
                rec_height = screen_height - s.y * scale - rec_y;

              for (y2 = rec_y; y2 < rec_y + rec_height; y2++)
                {
                  guchar *src_pixels;
                  guchar *dest_pixels;
                  gint x2;

                  src_pixels = gdk_pixbuf_get_pixels (pixbuf)
                             + y2 * gdk_pixbuf_get_rowstride(pixbuf)
                             + rec_x * (has_alpha ? 4 : 3);
                  dest_pixels = gdk_pixbuf_get_pixels (tmp)
                              + y2 * gdk_pixbuf_get_rowstride (tmp)
                              + rec_x * 4;

                  for (x2 = 0; x2 < rec_width; x2++)
                    {
                      *dest_pixels++ = *src_pixels++;
                      *dest_pixels++ = *src_pixels++;
                      *dest_pixels++ = *src_pixels++;

                      if (has_alpha)
                        *dest_pixels++ = *src_pixels++;
                      else
                        *dest_pixels++ = 255;
                    }
                }
            }

          g_object_unref (pixbuf);
          pixbuf = tmp;

          XFree (rectangles);
        }
    }

  screenshot_add_cursor (pixbuf, type, include_cursor,
                         wm_window != NULL ? wm_window : window,
                         frame_offset.left * scale,
                         frame_offset.top * scale);

  if (type == SCREENSHOT_WINDOW)
    {
      GdkRectangle rect;

      get_window_rect_coords (window, include_frame, NULL, &rect);

      *x = rect.x;
      *y = rect.y;
      *width = rect.width;
      *height = rect.height;
    }

  return save_screenshot (screenshot, pixbuf, filename_in, filename_out);
}

static void
remove_sender (GfScreenshot *screenshot,
               const gchar  *sender)
{
  gpointer name_id;

  name_id = g_hash_table_lookup (screenshot->senders, sender);

  if (name_id == NULL)
    return;

  g_bus_unwatch_name (GPOINTER_TO_UINT (name_id));
  g_hash_table_remove (screenshot->senders, sender);
}

static FlashspotData *
flashspot_data_new (GfScreenshot *screenshot,
                    const gchar  *sender)
{
  FlashspotData *data;

  data = g_new0 (FlashspotData, 1);

  data->screenshot = g_object_ref (screenshot);
  data->sender = g_strdup (sender);

  return data;
}

static void
flashspot_data_free (gpointer data)
{
  FlashspotData *real_data;

  real_data = (FlashspotData *) data;

  g_object_unref (real_data->screenshot);
  g_free (real_data->sender);

  g_free (real_data);
}

static void
flashspot_finished (GfFlashspot *flashspot,
                    gpointer     user_data)
{
  FlashspotData *data;

  data = (FlashspotData *) g_object_get_data (G_OBJECT (flashspot), "data");

  remove_sender (data->screenshot, data->sender);
}

static void
name_vanished_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  GfScreenshot *screenshot;

  screenshot = GF_SCREENSHOT (user_data);

  remove_sender (screenshot, name);
}

static void
take_screenshot (GfScreenshot          *screenshot,
                 GDBusMethodInvocation *invocation,
                 ScreenshotType         type,
                 gboolean               include_frame,
                 gboolean               include_cursor,
                 gint                   x,
                 gint                   y,
                 gint                   width,
                 gint                   height,
                 GfInvocationCallback   callback,
                 gboolean               flash,
                 const gchar           *filename_in)
{
  const gchar *sender;
  gboolean disabled;
  guint name_id;
  gboolean result;
  gchar *filename_out;

  sender = g_dbus_method_invocation_get_sender (invocation);
  disabled = g_settings_get_boolean (screenshot->lockdown, "disable-save-to-disk");

  if (g_hash_table_lookup (screenshot->senders, sender) != NULL || disabled)
    {
      callback (screenshot->screenshot_gen, invocation, FALSE, "");
      return;
    }

  name_id = g_bus_watch_name (G_BUS_TYPE_SESSION, sender,
                              G_BUS_NAME_WATCHER_FLAGS_NONE,
                              NULL, name_vanished_handler,
                              screenshot, NULL);

  g_hash_table_insert (screenshot->senders, g_strdup (sender),
                       GUINT_TO_POINTER (name_id));

  filename_out = NULL;
  result = take_screenshot_real (screenshot, type,
                                 include_frame, include_cursor,
                                 &x, &y, &width, &height,
                                 filename_in, &filename_out);

  if (result && flash)
    {
      GfFlashspot *flashspot;
      FlashspotData *data;

      flashspot = gf_flashspot_new ();
      data = flashspot_data_new (screenshot, sender);

      g_object_set_data_full (G_OBJECT (flashspot), "data", data,
                              flashspot_data_free);

      g_signal_connect (flashspot, "finished",
                        G_CALLBACK (flashspot_finished), NULL);

      gf_flashspot_fire (flashspot, x, y, width, height);
      g_object_unref (flashspot);
    }
  else
    {
      remove_sender (screenshot, sender);
    }

  callback (screenshot->screenshot_gen, invocation,
            result, filename_out ? filename_out : "");

  g_free (filename_out);
}

static gboolean
check_area (gint x,
            gint y,
            gint width,
            gint height)
{
  gint scale;
  gint screen_width;
  gint screen_height;

  scale = get_window_scaling_factor ();
  get_screen_size (&screen_width, &screen_height, scale);

  return x >= 0 && y >= 0 && width > 0 && height > 0 &&
         x + width <= screen_width && y + height <= screen_height;
}

static gboolean
handle_screenshot (GfScreenshotGen       *screenshot_gen,
                   GDBusMethodInvocation *invocation,
                   gboolean               include_cursor,
                   gboolean               flash,
                   const gchar           *filename,
                   GfScreenshot          *screenshot)
{
  gint scale;
  gint width;
  gint height;

  scale = get_window_scaling_factor ();
  get_screen_size (&width, &height, scale);

  take_screenshot (screenshot, invocation, SCREENSHOT_SCREEN,
                   FALSE, include_cursor, 0, 0, width, height,
                   gf_screenshot_gen_complete_screenshot,
                   flash, filename);

  return TRUE;
}

static gboolean
handle_screenshot_window (GfScreenshotGen       *screenshot_gen,
                          GDBusMethodInvocation *invocation,
                          gboolean               include_frame,
                          gboolean               include_cursor,
                          gboolean               flash,
                          const gchar           *filename,
                          GfScreenshot          *screenshot)
{
  take_screenshot (screenshot, invocation, SCREENSHOT_WINDOW,
                   include_frame, include_cursor, 0, 0, 0, 0,
                   gf_screenshot_gen_complete_screenshot_window,
                   flash, filename);

  return TRUE;
}

static gboolean
handle_screenshot_area (GfScreenshotGen       *screenshot_gen,
                        GDBusMethodInvocation *invocation,
                        gint                   x,
                        gint                   y,
                        gint                   width,
                        gint                   height,
                        gboolean               flash,
                        const gchar           *filename,
                        GfScreenshot          *screenshot)
{
  if (!check_area (x, y, width, height))
    {
      g_dbus_method_invocation_return_error (invocation, G_IO_ERROR,
                                             G_IO_ERROR_CANCELLED,
                                             "Invalid params");

      return TRUE;
    }

  take_screenshot (screenshot, invocation, SCREENSHOT_AREA,
                   FALSE, FALSE, x, y, width, height,
                   gf_screenshot_gen_complete_screenshot_area,
                   flash, filename);

  return TRUE;
}

static gboolean
handle_flash_area (GfScreenshotGen       *screenshot_gen,
                   GDBusMethodInvocation *invocation,
                   gint                   x,
                   gint                   y,
                   gint                   width,
                   gint                   height,
                   gpointer               user_data)
{
  GfFlashspot *flashspot;

  if (!check_area (x, y, width, height))
    {
      g_dbus_method_invocation_return_error (invocation, G_IO_ERROR,
                                             G_IO_ERROR_CANCELLED,
                                             "Invalid params");

      return TRUE;
    }

  flashspot = gf_flashspot_new ();
  gf_flashspot_fire (flashspot, x, y, width, height);
  g_object_unref (flashspot);

  gf_screenshot_gen_complete_flash_area (screenshot_gen, invocation);

  return TRUE;
}

static gboolean
handle_select_area (GfScreenshotGen       *screenshot_gen,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  GfSelectArea *select_area;
  gint x;
  gint y;
  gint width;
  gint height;
  gboolean selected;
  GdkDisplay *display;

  select_area = gf_select_area_new ();
  x = y = width = height = 0;

  selected = gf_select_area_select (select_area, &x, &y, &width, &height);
  g_object_unref (select_area);

  display = gdk_display_get_default ();
  gdk_display_flush (display);

  if (selected)
    {
      /* wait 200ms to allow compositor finish redrawing selected area
       * without selection overlay.
       */
      g_usleep (G_USEC_PER_SEC / 5);

      gf_screenshot_gen_complete_select_area (screenshot_gen, invocation,
                                              x, y, width, height);
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation, G_IO_ERROR,
                                             G_IO_ERROR_CANCELLED,
                                             "Operation was cancelled");
    }

  return TRUE;
}

static void
bus_acquired_handler (GDBusConnection *connection,
                      const gchar     *name,
                      gpointer         user_data)
{
  GfScreenshot *screenshot;
  GfScreenshotGen *screenshot_gen;
  GDBusInterfaceSkeleton *skeleton;
  GError *error;
  gboolean exported;

  screenshot = GF_SCREENSHOT (user_data);

  screenshot_gen = screenshot->screenshot_gen;
  skeleton = G_DBUS_INTERFACE_SKELETON (screenshot_gen);

  g_signal_connect (screenshot_gen, "handle-screenshot",
                    G_CALLBACK (handle_screenshot), screenshot);
  g_signal_connect (screenshot_gen, "handle-screenshot-window",
                    G_CALLBACK (handle_screenshot_window), screenshot);
  g_signal_connect (screenshot_gen, "handle-screenshot-area",
                    G_CALLBACK (handle_screenshot_area), screenshot);
  g_signal_connect (screenshot_gen, "handle-flash-area",
                    G_CALLBACK (handle_flash_area), screenshot);
  g_signal_connect (screenshot_gen, "handle-select-area",
                    G_CALLBACK (handle_select_area), screenshot);

  error = NULL;
  exported = g_dbus_interface_skeleton_export (skeleton, connection,
                                               SCREENSHOT_DBUS_PATH,
                                               &error);

  if (!exported)
    {
      g_warning ("Failed to export interface: %s", error->message);
      g_error_free (error);
      return;
    }
}

static void
gf_screenshot_dispose (GObject *object)
{
  GfScreenshot *screenshot;
  GDBusInterfaceSkeleton *skeleton;

  screenshot = GF_SCREENSHOT (object);

  if (screenshot->bus_name)
    {
      g_bus_unown_name (screenshot->bus_name);
      screenshot->bus_name = 0;
    }

  if (screenshot->screenshot_gen)
    {
      skeleton = G_DBUS_INTERFACE_SKELETON (screenshot->screenshot_gen);

      g_dbus_interface_skeleton_unexport (skeleton);
      g_clear_object (&screenshot->screenshot_gen);
    }

  if (screenshot->senders)
    {
      g_hash_table_destroy (screenshot->senders);
      screenshot->senders = NULL;
    }

  g_clear_object (&screenshot->lockdown);

  g_clear_pointer (&screenshot->datetime, g_date_time_unref);

  G_OBJECT_CLASS (gf_screenshot_parent_class)->dispose (object);
}

static void
gf_screenshot_class_init (GfScreenshotClass *screenshot_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (screenshot_class);

  object_class->dispose = gf_screenshot_dispose;
}

static void
gf_screenshot_init (GfScreenshot *screenshot)
{
  screenshot->screenshot_gen = gf_screenshot_gen_skeleton_new ();

  screenshot->bus_name = g_bus_own_name (G_BUS_TYPE_SESSION,
                                         SCREENSHOT_DBUS_NAME,
                                         G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                         G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                         (GBusAcquiredCallback) bus_acquired_handler,
                                         NULL, NULL, screenshot, NULL);

  screenshot->senders = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, NULL);

  screenshot->lockdown = g_settings_new ("org.gnome.desktop.lockdown");

  screenshot->datetime = g_date_time_new_now_local ();
}

GfScreenshot *
gf_screenshot_new (void)
{
  return g_object_new (GF_TYPE_SCREENSHOT, NULL);
}
