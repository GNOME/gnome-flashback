/*
 * Copyright (C) 2015 Alberts Muktupāvels
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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>

#include "gf-dbus-screenshot.h"
#include "gf-flashspot.h"
#include "gf-screenshot.h"
#include "gf-select-area.h"

#define SCREENSHOT_DBUS_NAME "org.gnome.Shell.Screenshot"
#define SCREENSHOT_DBUS_PATH "/org/gnome/Shell/Screenshot"

typedef void (*GfInvocationCallback) (GfDBusScreenshot      *dbus_screenshot,
                                      GDBusMethodInvocation *invocation,
                                      gboolean               result,
                                      const gchar           *filename);

struct _GfScreenshot
{
  GObject           parent;

  GfDBusScreenshot *dbus_screenshot;
  gint              bus_name;

  GHashTable       *senders;

  GSettings        *lockdown;
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

  if (!g_file_test (path, G_FILE_TEST_EXISTS))
    {
      path = g_get_home_dir ();

      if (!g_file_test (path, G_FILE_TEST_EXISTS))
        return NULL;
    }

  return get_unique_path (path, filename);
}

static gboolean
save_screenshot (GdkPixbuf    *pixbuf,
                 const gchar  *filename_in,
                 gchar       **filename_out)
{
  gboolean result;
  gchar *filename;
  GError *error;

  if (pixbuf == NULL)
    return FALSE;

  filename = get_filename (filename_in);

  error = NULL;
  result = gdk_pixbuf_save (pixbuf, filename, "png", &error, NULL);

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

  return result;
}

static void
blank_rectangle_in_pixbuf (GdkPixbuf *pixbuf,
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
make_region_with_monitors (GdkScreen *screen)
{
  cairo_region_t *region;
  gint num_monitors;
  gint i;

  num_monitors = gdk_screen_get_n_monitors (screen);
  region = cairo_region_create ();

  for (i = 0; i < num_monitors; i++)
    {
      GdkRectangle rect;

      gdk_screen_get_monitor_geometry (screen, i, &rect);
      cairo_region_union_rectangle (region, &rect);
    }

  return region;
}

static void
mask_monitors (GdkPixbuf *pixbuf,
               GdkWindow *root_window)
{
  GdkScreen *screen;
  cairo_region_t *region_with_monitors;
  cairo_region_t *invisible_region;
  cairo_rectangle_int_t rect;

  screen = gdk_window_get_screen (root_window);
  region_with_monitors = make_region_with_monitors (screen);

  rect.x = 0;
  rect.y = 0;
  rect.width = gdk_screen_get_width (screen);
  rect.height = gdk_screen_get_height (screen);

  invisible_region = cairo_region_create_rectangle (&rect);
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

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);
  xwindow = gdk_x11_window_get_xid (window);
  gtk_frame_extents = XInternAtom (xdisplay, "_GTK_FRAME_EXTENTS", False);

  gdk_error_trap_push ();
  result = XGetWindowProperty (xdisplay, xwindow, gtk_frame_extents,
                               0, G_MAXLONG, False, XA_CARDINAL,
                               &type, &format, &n_items, &bytes_after, &data);
  gdk_error_trap_pop_ignored ();

  if (data == NULL)
    return FALSE;

  if (result != Success || type != XA_CARDINAL || format != 32 || n_items != 4)
    {
      XFree (data);

      return FALSE;
    }

  borders = (gulong *) data;

  extents->left = borders[0];
  extents->right = borders[1];
  extents->top = borders[2];
  extents->bottom = borders[3];

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

  screen_width = gdk_screen_width ();
  if (x + width > screen_width)
    width = screen_width - x;

  screen_height = gdk_screen_height ();
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

static GdkWindow *
find_active_window (void)
{
  GdkScreen *screen;

  screen = gdk_screen_get_default ();

  return gdk_screen_get_active_window (screen);
}

static GdkWindow *
find_current_window (GdkDisplay *display)
{
  GdkWindow *window;
  GdkWindow *toplevel;

  window = find_active_window ();

  if (window == NULL)
    {
      GdkDeviceManager *manager;
      GdkDevice *device;

      manager = gdk_display_get_device_manager (display);
      device = gdk_device_manager_get_client_pointer (manager);

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

      return save_screenshot (pixbuf, filename_in, filename_out);
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

          tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, s.width, s.height);
          gdk_pixbuf_fill (tmp, 0);

          for (i = 0; i < rectangle_count; i++)
            {
              gint rec_x;
              gint rec_y;
              gint rec_width;
              gint rec_height;
              gint y2;

              /* If we're using invisible borders, the ShapeBounding might not
               * have the same size as the frame extents, as it would include
               * the areas for the invisible borders themselves.
               * In that case, trim every rectangle we get by the offset between
               * the WM window size and the frame extents. */
              rec_x = rectangles[i].x;
              rec_y = rectangles[i].y;
              rec_width = rectangles[i].width - (frame_offset.left + frame_offset.right);
              rec_height = rectangles[i].height - (frame_offset.top + frame_offset.bottom);

              if (real.x < 0)
                {
                  rec_x += real.x;
                  rec_x = MAX(rec_x, 0);
                  rec_width += real.x;
                }

              if (real.y < 0)
                {
                  rec_y += real.y;
                  rec_y = MAX(rec_y, 0);
                  rec_height += real.y;
                }

              if (s.x + rec_x + rec_width > gdk_screen_width ())
                rec_width = gdk_screen_width () - s.x - rec_x;

              if (s.y + rec_y + rec_height > gdk_screen_height ())
                rec_height = gdk_screen_height () - s.y - rec_y;

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

  /* If we have a selected area, there were by definition no cursor in the
   * screenshot. */
  if (include_cursor && type == SCREENSHOT_AREA)
    {
      GdkCursor *cursor;
      GdkPixbuf *cursor_pixbuf;

      cursor = gdk_cursor_new_for_display (display, GDK_LEFT_PTR);
      cursor_pixbuf = gdk_cursor_get_image (cursor);

      if (cursor_pixbuf != NULL)
        {
          GdkDeviceManager *manager;
          GdkDevice *device;
          GdkRectangle rect;
          gint cx;
          gint cy;
          gint xhot;
          gint yhot;

          manager = gdk_display_get_device_manager (display);
          device = gdk_device_manager_get_client_pointer (manager);

          if (wm_window != NULL)
            gdk_window_get_device_position (wm_window, device, &cx, &cy, NULL);
          else
            gdk_window_get_device_position (window, device, &cx, &cy, NULL);

          sscanf (gdk_pixbuf_get_option (cursor_pixbuf, "x_hot"), "%d", &xhot);
          sscanf (gdk_pixbuf_get_option (cursor_pixbuf, "y_hot"), "%d", &yhot);

          /* in rect we have the cursor window coordinates */
          rect.x = cx + real.x;
          rect.y = cy + real.y;
          rect.width = gdk_pixbuf_get_width (cursor_pixbuf);
          rect.height = gdk_pixbuf_get_height (cursor_pixbuf);

          /* see if the pointer is inside the window */
          if (gdk_rectangle_intersect (&real, &rect, &rect))
            {
              gint cursor_x;
              gint cursor_y;

              cursor_x = cx - xhot - frame_offset.left;
              cursor_y = cy - yhot - frame_offset.top;

              gdk_pixbuf_composite (cursor_pixbuf, pixbuf,
                                    cursor_x, cursor_y,
                                    rect.width, rect.height,
                                    cursor_x, cursor_y,
                                    1.0, 1.0,
                                    GDK_INTERP_BILINEAR,
                                    255);
            }
        }

      g_clear_object (&cursor_pixbuf);
      g_clear_object (&cursor);
    }

  if (type == SCREENSHOT_WINDOW)
    {
      GdkRectangle rect;

      get_window_rect_coords (window, include_frame, NULL, &rect);

      *x = rect.x;
      *y = rect.y;
      *width = rect.width;
      *height = rect.height;
    }

  return save_screenshot (pixbuf, filename_in, filename_out);
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

  data = (FlashspotData *) g_new0 (FlashspotData *, 1);

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
      callback (screenshot->dbus_screenshot, invocation, FALSE, "");
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

  callback (screenshot->dbus_screenshot, invocation,
            result, filename_out ? filename_out : "");

  g_free (filename_out);
}

static void
scale_area (gint *x,
            gint *y,
            gint *width,
            gint *height)
{
}

static void
unscale_area (gint *x,
              gint *y,
              gint *width,
              gint *height)
{
}

static gboolean
check_area (gint x,
            gint y,
            gint width,
            gint height)
{
  GdkScreen *screen;
  gint screen_width;
  gint screen_height;

  screen = gdk_screen_get_default ();
  screen_width = gdk_screen_get_width (screen);
  screen_height = gdk_screen_get_height (screen);

  return x >= 0 && y >= 0 && width > 0 && height > 0 &&
         x + width <= screen_width && y + height <= screen_height;
}

static gboolean
handle_screenshot (GfDBusScreenshot      *dbus_screenshot,
                   GDBusMethodInvocation *invocation,
                   gboolean               include_cursor,
                   gboolean               flash,
                   const gchar           *filename,
                   gpointer               user_data)
{
  GfScreenshot *screenshot;
  GdkScreen *screen;
  gint width;
  gint height;

  screenshot = GF_SCREENSHOT (user_data);
  screen = gdk_screen_get_default ();
  width = gdk_screen_get_width (screen);
  height = gdk_screen_get_height (screen);

  take_screenshot (screenshot, invocation, SCREENSHOT_SCREEN,
                   FALSE, include_cursor, 0, 0, width, height,
                   gf_dbus_screenshot_complete_screenshot,
                   flash, filename);

  return TRUE;
}

static gboolean
handle_screenshot_window (GfDBusScreenshot      *dbus_screenshot,
                          GDBusMethodInvocation *invocation,
                          gboolean               include_frame,
                          gboolean               include_cursor,
                          gboolean               flash,
                          const gchar           *filename,
                          gpointer               user_data)
{
  GfScreenshot *screenshot;

  screenshot = GF_SCREENSHOT (user_data);

  take_screenshot (screenshot, invocation, SCREENSHOT_WINDOW,
                   include_frame, include_cursor, 0, 0, 0, 0,
                   gf_dbus_screenshot_complete_screenshot_window,
                   flash, filename);

  return TRUE;
}

static gboolean
handle_screenshot_area (GfDBusScreenshot      *dbus_screenshot,
                        GDBusMethodInvocation *invocation,
                        gint                   x,
                        gint                   y,
                        gint                   width,
                        gint                   height,
                        gboolean               flash,
                        const gchar           *filename,
                        gpointer               user_data)
{
  GfScreenshot *screenshot;

  screenshot = GF_SCREENSHOT (user_data);

  if (!check_area (x, y, width, height))
    {
      g_dbus_method_invocation_return_error (invocation, G_IO_ERROR,
                                             G_IO_ERROR_CANCELLED,
                                             "Invalid params");

      return TRUE;
    }

  scale_area (&x, &y, &width, &height);
  take_screenshot (screenshot, invocation, SCREENSHOT_AREA,
                   FALSE, FALSE, x, y, width, height,
                   gf_dbus_screenshot_complete_screenshot_area,
                   flash, filename);

  return TRUE;
}

static gboolean
handle_flash_area (GfDBusScreenshot      *dbus_screenshot,
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

  scale_area (&x, &y, &width, &height);

  flashspot = gf_flashspot_new ();
  gf_flashspot_fire (flashspot, x, y, width, height);
  g_object_unref (flashspot);

  gf_dbus_screenshot_complete_flash_area (dbus_screenshot, invocation);

  return TRUE;
}

static gboolean
handle_select_area (GfDBusScreenshot      *dbus_screenshot,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  GfSelectArea *select_area;
  gint x;
  gint y;
  gint width;
  gint height;

  select_area = gf_select_area_new ();
  x = y = width = height = 0;

  if (gf_select_area_select (select_area, &x, &y, &width, &height))
    {
      unscale_area (&x, &y, &width, &height);
      gf_dbus_screenshot_complete_select_area (dbus_screenshot, invocation,
                                               x, y, width, height);
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation, G_IO_ERROR,
                                             G_IO_ERROR_CANCELLED,
                                             "Operation was cancelled");
    }

  g_object_unref (select_area);

  return TRUE;
}

static void
bus_acquired_handler (GDBusConnection *connection,
                      const gchar     *name,
                      gpointer         user_data)
{
  GfScreenshot *screenshot;
  GfDBusScreenshot *dbus_screenshot;
  GDBusInterfaceSkeleton *skeleton;
  GError *error;
  gboolean exported;

  screenshot = GF_SCREENSHOT (user_data);

  dbus_screenshot = screenshot->dbus_screenshot;
  skeleton = G_DBUS_INTERFACE_SKELETON (dbus_screenshot);

  g_signal_connect (dbus_screenshot, "handle-screenshot",
                    G_CALLBACK (handle_screenshot), screenshot);
  g_signal_connect (dbus_screenshot, "handle-screenshot-window",
                    G_CALLBACK (handle_screenshot_window), screenshot);
  g_signal_connect (dbus_screenshot, "handle-screenshot-area",
                    G_CALLBACK (handle_screenshot_area), screenshot);
  g_signal_connect (dbus_screenshot, "handle-flash-area",
                    G_CALLBACK (handle_flash_area), screenshot);
  g_signal_connect (dbus_screenshot, "handle-select-area",
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

  if (screenshot->dbus_screenshot)
    {
      skeleton = G_DBUS_INTERFACE_SKELETON (screenshot->dbus_screenshot);

      g_dbus_interface_skeleton_unexport (skeleton);
      g_clear_object (&screenshot->dbus_screenshot);
    }

  if (screenshot->senders)
    {
      g_hash_table_destroy (screenshot->senders);
      screenshot->senders = NULL;
    }

  g_clear_object (&screenshot->lockdown);

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
  screenshot->dbus_screenshot = gf_dbus_screenshot_skeleton_new ();

  screenshot->bus_name = g_bus_own_name (G_BUS_TYPE_SESSION,
                                         SCREENSHOT_DBUS_NAME,
                                         G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                         G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                         (GBusAcquiredCallback) bus_acquired_handler,
                                         NULL, NULL, screenshot, NULL);

  screenshot->senders = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, NULL);

  screenshot->lockdown = g_settings_new ("org.gnome.desktop.lockdown");
}

GfScreenshot *
gf_screenshot_new (void)
{
  return g_object_new (GF_TYPE_SCREENSHOT, NULL);
}
