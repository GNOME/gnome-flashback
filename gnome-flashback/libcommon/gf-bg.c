/*
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2007-2008 Red Hat, Inc.
 * Copyright (C) 2019-2021 Alberts Muktupāvels
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
 * Derived from eel-background.c and eel-gdk-pixbuf-extensions.c by
 * Darin Adler <darin@eazel.com> and Ramiro Estrugo <ramiro@eazel.com>
 *
 * Author:
 *   Soren Sandmann <sandmann@redhat.com>
 */

#include "config.h"
#include "gf-bg.h"

#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>

#include <glib/gstdio.h>
#include <gio/gio.h>

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <cairo.h>
#include <cairo-xlib.h>

#include <libgnome-desktop/gnome-bg-slide-show.h>

#define BG_KEY_PRIMARY_COLOR      "primary-color"
#define BG_KEY_SECONDARY_COLOR    "secondary-color"
#define BG_KEY_COLOR_TYPE         "color-shading-type"
#define BG_KEY_PICTURE_PLACEMENT  "picture-options"
#define BG_KEY_PICTURE_OPACITY    "picture-opacity"
#define BG_KEY_PICTURE_URI        "picture-uri"
#define BG_KEY_PICTURE_URI_DARK   "picture-uri-dark"
#define IFACE_KEY_COLOR_SCHEME    "color-scheme"

/* We keep the large pixbufs around if the next update
   in the slideshow is less than 60 seconds away */
#define KEEP_EXPENSIVE_CACHE_SECS 60

typedef struct FileCacheEntry FileCacheEntry;
#define CACHE_SIZE 4

struct _GfBG
{
	GObject                 parent_instance;

	char *                  schema_id;
	GSettings *             settings;
	GSettings *             interface_settings;
	gboolean                has_dark_key;
	gulong                  change_event_id;
	gulong                  changed_color_scheme_id;

	char *			filename;
	GDesktopBackgroundStyle	placement;
	GDesktopBackgroundShading	color_type;
	GdkRGBA			primary;
	GdkRGBA			secondary;

	GFileMonitor *		file_monitor;

	guint                   changed_id;
	guint                   transitioned_id;
	guint                   blow_caches_id;

	/* Cached information, only access through cache accessor functions */
        GnomeBGSlideShow *	slideshow;
	time_t			file_mtime;
	GdkPixbuf *		pixbuf_cache;
	int			timeout_id;

	GList *		        file_cache;
};

enum
{
  PROP_0,

  PROP_SCHEMA_ID,

  LAST_PROP
};

static GParamSpec *bg_properties[LAST_PROP] = { NULL };

enum {
	CHANGED,
	TRANSITIONED,
	N_SIGNALS
};

static const cairo_user_data_key_t average_color_key;

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (GfBG, gf_bg, G_TYPE_OBJECT)

static GdkPixbuf *pixbuf_scale_to_fit  (GdkPixbuf  *src,
					int         max_width,
					int         max_height);
static GdkPixbuf *pixbuf_scale_to_min  (GdkPixbuf  *src,
					int         min_width,
					int         min_height);
static void       pixbuf_draw_gradient (GdkPixbuf    *pixbuf,
					gboolean      horizontal,
					GdkRGBA      *c1,
					GdkRGBA      *c2,
					GdkRectangle *rect);
static void       pixbuf_tile          (GdkPixbuf  *src,
					GdkPixbuf  *dest);
static void       pixbuf_blend         (GdkPixbuf  *src,
					GdkPixbuf  *dest,
					int         src_x,
					int         src_y,
					int         width,
					int         height,
					int         dest_x,
					int         dest_y,
					double      alpha);

static GdkPixbuf *get_pixbuf_for_size  (GfBG                  *bg,
					gint                   num_monitor,
					int                    width,
					int                    height);
static void       clear_cache          (GfBG                  *bg);
static time_t     get_mtime            (const char            *filename);

static void
color_from_string (const char *string,
		   GdkRGBA   *colorp)
{
	/* If all else fails use black */
	gdk_rgba_parse (colorp, "black");

	if (!string)
		return;

	gdk_rgba_parse (colorp, string);
}

static char *
color_to_string (const GdkRGBA *color)
{
	return g_strdup_printf ("#%02x%02x%02x",
				(int) (0.5 + color->red * 255),
				(int) (0.5 + color->green * 255),
				(int) (0.5 + color->blue * 255));
}

static gboolean
do_changed (GfBG *bg)
{
	bg->changed_id = 0;

	g_signal_emit (G_OBJECT (bg), signals[CHANGED], 0);

	return FALSE;
}

static void
queue_changed (GfBG *bg)
{
	if (bg->changed_id > 0) {
		g_source_remove (bg->changed_id);
	}

	bg->changed_id = g_timeout_add_full (G_PRIORITY_LOW,
					     100,
					     (GSourceFunc)do_changed,
					     bg,
					     NULL);
}

static gboolean
do_transitioned (GfBG *bg)
{
	bg->transitioned_id = 0;

	if (bg->pixbuf_cache) {
		g_object_unref (bg->pixbuf_cache);
		bg->pixbuf_cache = NULL;
	}

	g_signal_emit (G_OBJECT (bg), signals[TRANSITIONED], 0);

	return FALSE;
}

static void
queue_transitioned (GfBG *bg)
{
	if (bg->transitioned_id > 0) {
		g_source_remove (bg->transitioned_id);
	}

	bg->transitioned_id = g_timeout_add_full (G_PRIORITY_LOW,
					     100,
					     (GSourceFunc)do_transitioned,
					     bg,
					     NULL);
}

static gboolean 
bg_gsettings_mapping (GVariant *value,
			gpointer *result,
			gpointer user_data)
{
	const gchar *bg_key_value;
	char *filename = NULL;

	/* The final fallback if nothing matches is with a NULL value. */
	if (value == NULL) {
		*result = NULL;
		return TRUE;
	}

	bg_key_value = g_variant_get_string (value, NULL);

	if (bg_key_value && *bg_key_value != '\0') {
		filename = g_filename_from_uri (bg_key_value, NULL, NULL);

		if (filename != NULL && g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE) {
			g_free (filename);
			return FALSE;
		}

		if (filename != NULL) {
			*result = filename;
			return TRUE;
		}
	}

	return FALSE;
}

static inline gchar *
get_wallpaper_cache_dir (void)
{
	return g_build_filename (g_get_user_cache_dir(), "wallpaper", NULL);
}

static inline gchar *
get_wallpaper_cache_prefix_name (gint                     num_monitor,
				 GDesktopBackgroundStyle  placement,
				 gint                     width,
				 gint                     height)
{
	return g_strdup_printf ("%i_%i_%i_%i", num_monitor, (gint) placement, width, height);
}

static char *
get_wallpaper_cache_filename (const char              *filename,
			      gint                     num_monitor,
			      GDesktopBackgroundStyle  placement,
			      gint                     width,
			      gint                     height)
{
	gchar *cache_filename;
	gchar *cache_prefix_name;
	gchar *md5_filename;
	gchar *cache_basename;
	gchar *cache_dir;

	md5_filename = g_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar *) filename, strlen (filename));
	cache_prefix_name = get_wallpaper_cache_prefix_name (num_monitor, placement, width, height);
	cache_basename = g_strdup_printf ("%s_%s", cache_prefix_name, md5_filename);
	cache_dir = get_wallpaper_cache_dir ();
	cache_filename = g_build_filename (cache_dir, cache_basename, NULL);

	g_free (cache_prefix_name);
	g_free (md5_filename);
	g_free (cache_basename);
	g_free (cache_dir);

	return cache_filename;
}

static void
cleanup_cache_for_monitor (gchar *cache_dir,
			   gint   num_monitor)
{
	GDir            *g_cache_dir;
	gchar           *monitor_prefix;
	const gchar     *file;

	g_cache_dir = g_dir_open (cache_dir, 0, NULL);
	monitor_prefix = g_strdup_printf ("%i_", num_monitor);

	file = g_dir_read_name (g_cache_dir);
	while (file != NULL) {
		gchar *path;

		path = g_build_filename (cache_dir, file, NULL);
		/* purge files with same monitor id */
		if (g_str_has_prefix (file, monitor_prefix) &&
		    g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			if (g_unlink (path) != 0)
				g_warning ("Failed to delete %s", path);
		}

		g_free (path);

		file = g_dir_read_name (g_cache_dir);
	}

	g_free (monitor_prefix);
	g_dir_close (g_cache_dir);
}

static gboolean
cache_file_is_valid (const char *filename,
		     const char *cache_filename)
{
	time_t mtime;
	time_t cache_mtime;

	if (!g_file_test (cache_filename, G_FILE_TEST_IS_REGULAR))
		return FALSE;

	mtime = get_mtime (filename);
	cache_mtime = get_mtime (cache_filename);

	return (mtime < cache_mtime);
}

static void
refresh_cache_file (GfBG      *bg,
                    GdkPixbuf *new_pixbuf,
                    gint       num_monitor,
                    gint       width,
                    gint       height)
{
	gchar           *cache_filename;
	gchar           *cache_dir;
	GdkPixbufFormat *format;
	gchar           *format_name;

	if ((num_monitor == -1) || (width <= 300) || (height <= 300))
		return;

	cache_filename = get_wallpaper_cache_filename (bg->filename, num_monitor, bg->placement, width, height);
	cache_dir = get_wallpaper_cache_dir ();

	/* Only refresh scaled file on disk if useful (and don't cache slideshow) */
	if (!cache_file_is_valid (bg->filename, cache_filename)) {
		format = gdk_pixbuf_get_file_info (bg->filename, NULL, NULL);

		if (format != NULL) {
			if (!g_file_test (cache_dir, G_FILE_TEST_IS_DIR)) {
				if (g_mkdir_with_parents (cache_dir, 0700) != 0)
					g_warning ("Failed to mkdir %s", cache_dir);
			} else {
				cleanup_cache_for_monitor (cache_dir, num_monitor);
			}

			format_name = gdk_pixbuf_format_get_name (format);

			if (strcmp (format_name, "jpeg") == 0)
				gdk_pixbuf_save (new_pixbuf, cache_filename, format_name, NULL, "quality", "100", NULL);
			else
				gdk_pixbuf_save (new_pixbuf, cache_filename, format_name, NULL, NULL);

			g_free (format_name);
		}
	}

	g_free (cache_filename);
	g_free (cache_dir);
}

static void
file_changed (GFileMonitor *file_monitor,
	      GFile *child,
	      GFile *other_file,
	      GFileMonitorEvent event_type,
	      gpointer user_data)
{
	GfBG *bg = GF_BG (user_data);

	clear_cache (bg);
	queue_changed (bg);
}

static void
draw_color_area (GfBG         *bg,
                 GdkPixbuf    *dest,
                 GdkRectangle *rect)
{
	guint32 pixel;
        GdkRectangle extent;

        extent.x = 0;
        extent.y = 0;
        extent.width = gdk_pixbuf_get_width (dest);
        extent.height = gdk_pixbuf_get_height (dest);

        if (!gdk_rectangle_intersect (rect, &extent, rect))
                return;
	
	switch (bg->color_type) {
	case G_DESKTOP_BACKGROUND_SHADING_SOLID:
		/* not really a big deal to ignore the area of interest */
		pixel = ((int) (0.5 + bg->primary.red * 255) << 24)      |
			((int) (0.5 + bg->primary.green * 255) << 16)    |
			((int) (0.5 + bg->primary.blue * 255) << 8)      |
			(0xff);
		
		gdk_pixbuf_fill (dest, pixel);
		break;
		
	case G_DESKTOP_BACKGROUND_SHADING_HORIZONTAL:
		pixbuf_draw_gradient (dest, TRUE, &(bg->primary), &(bg->secondary), rect);
		break;
		
	case G_DESKTOP_BACKGROUND_SHADING_VERTICAL:
		pixbuf_draw_gradient (dest, FALSE, &(bg->primary), &(bg->secondary), rect);
		break;
		
	default:
		break;
	}
}

static void
draw_color (GfBG      *bg,
            GdkPixbuf *dest)
{
	GdkRectangle rect;
	rect.x = 0;
	rect.y = 0;
	rect.width = gdk_pixbuf_get_width (dest);
	rect.height = gdk_pixbuf_get_height (dest);
	draw_color_area (bg, dest, &rect);
}

static void
draw_color_each_monitor (GfBG       *bg,
                         GdkPixbuf  *dest,
                         GdkDisplay *display,
                         gint        scale)
{
  int n_monitors;
  int i;

  n_monitors = gdk_display_get_n_monitors (display);

  for (i = 0; i < n_monitors; i++)
    {
      GdkMonitor *monitor;
      GdkRectangle geometry;

      monitor = gdk_display_get_monitor (display, i);
      gdk_monitor_get_geometry (monitor, &geometry);

      geometry.x *= scale;
      geometry.y *= scale;
      geometry.width *= scale;
      geometry.height *= scale;

      draw_color_area (bg, dest, &geometry);
    }
}

static GdkPixbuf *
pixbuf_clip_to_fit (GdkPixbuf *src,
		    int        max_width,
		    int        max_height)
{
	int src_width, src_height;
	int w, h;
	int src_x, src_y;
	GdkPixbuf *pixbuf;

	src_width = gdk_pixbuf_get_width (src);
	src_height = gdk_pixbuf_get_height (src);

	if (src_width < max_width && src_height < max_height)
		return g_object_ref (src);

	w = MIN(src_width, max_width);
	h = MIN(src_height, max_height);

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 gdk_pixbuf_get_has_alpha (src),
				 8, w, h);

	src_x = (src_width - w) / 2;
	src_y = (src_height - h) / 2;
	gdk_pixbuf_copy_area (src,
			      src_x, src_y,
			      w, h,
			      pixbuf,
			      0, 0);
	return pixbuf;
}

static GdkPixbuf *
get_scaled_pixbuf (GDesktopBackgroundStyle placement,
		   GdkPixbuf *pixbuf,
		   int width, int height,
		   int *x, int *y,
		   int *w, int *h)
{
	GdkPixbuf *new;

#if 0
	g_print ("original_width: %d %d\n",
		 gdk_pixbuf_get_width (pixbuf),
		 gdk_pixbuf_get_height (pixbuf));
#endif
	
	switch (placement) {
	case G_DESKTOP_BACKGROUND_STYLE_SPANNED:
                new = pixbuf_scale_to_fit (pixbuf, width, height);
		break;
	case G_DESKTOP_BACKGROUND_STYLE_ZOOM:
		new = pixbuf_scale_to_min (pixbuf, width, height);
		break;
		
	case G_DESKTOP_BACKGROUND_STYLE_STRETCHED:
		new = gdk_pixbuf_scale_simple (pixbuf, width, height,
					       GDK_INTERP_BILINEAR);
		break;
		
	case G_DESKTOP_BACKGROUND_STYLE_SCALED:
		new = pixbuf_scale_to_fit (pixbuf, width, height);
		break;
		
	case G_DESKTOP_BACKGROUND_STYLE_NONE:
		/* This shouldn’t be true, but if it is, assert and
		 * fall through, in case assertions are disabled.
		 */
		g_assert_not_reached ();
	case G_DESKTOP_BACKGROUND_STYLE_CENTERED:
	case G_DESKTOP_BACKGROUND_STYLE_WALLPAPER:
	default:
		new = pixbuf_clip_to_fit (pixbuf, width, height);
		break;
	}
	
	*w = gdk_pixbuf_get_width (new);
	*h = gdk_pixbuf_get_height (new);
	*x = (width - *w) / 2;
	*y = (height - *h) / 2;
	
	return new;
}

static void
draw_image_area (GfBG         *bg,
                 gint          num_monitor,
                 GdkPixbuf    *pixbuf,
                 GdkPixbuf    *dest,
                 GdkRectangle *area)
{
	int dest_width = area->width;
	int dest_height = area->height;
	int x, y, w, h;
	GdkPixbuf *scaled;
	
	if (!pixbuf)
		return;

	scaled = get_scaled_pixbuf (bg->placement, pixbuf, dest_width, dest_height, &x, &y, &w, &h);

	switch (bg->placement) {
	case G_DESKTOP_BACKGROUND_STYLE_WALLPAPER:
		pixbuf_tile (scaled, dest);
		break;
	case G_DESKTOP_BACKGROUND_STYLE_ZOOM:
	case G_DESKTOP_BACKGROUND_STYLE_CENTERED:
	case G_DESKTOP_BACKGROUND_STYLE_STRETCHED:
	case G_DESKTOP_BACKGROUND_STYLE_SCALED:
		pixbuf_blend (scaled, dest, 0, 0, w, h, x + area->x, y + area->y, 1.0);
		break;
	case G_DESKTOP_BACKGROUND_STYLE_SPANNED:
		pixbuf_blend (scaled, dest, 0, 0, w, h, x, y, 1.0);
		break;
	case G_DESKTOP_BACKGROUND_STYLE_NONE:
	default:
		g_assert_not_reached ();
		break;
	}

	refresh_cache_file (bg, scaled, num_monitor, dest_width, dest_height);

	g_object_unref (scaled);
}

static void
draw_once (GfBG      *bg,
           GdkPixbuf *dest)
{
	GdkRectangle rect;
	GdkPixbuf   *pixbuf;
	gint         num_monitor;

	/* we just draw on the whole screen */
	num_monitor = 0;

	rect.x = 0;
	rect.y = 0;
	rect.width = gdk_pixbuf_get_width (dest);
	rect.height = gdk_pixbuf_get_height (dest);

	pixbuf = get_pixbuf_for_size (bg, num_monitor, rect.width, rect.height);
	if (pixbuf) {
		GdkPixbuf *rotated;

		rotated = gdk_pixbuf_apply_embedded_orientation (pixbuf);
		if (rotated != NULL) {
			g_object_unref (pixbuf);
			pixbuf = rotated;
		}

		draw_image_area (bg,
				 num_monitor,
				 pixbuf,
				 dest,
				 &rect);
		g_object_unref (pixbuf);
	}
}

static void
draw_each_monitor (GfBG       *bg,
                   GdkPixbuf  *dest,
                   GdkDisplay *display,
                   gint        scale)
{
  int n_monitors;
  int i;

  n_monitors = gdk_display_get_n_monitors (display);

  for (i = 0; i < n_monitors; i++)
    {
      GdkMonitor *monitor;
      GdkRectangle geometry;
      GdkPixbuf *pixbuf;

      monitor = gdk_display_get_monitor (display, i);
      gdk_monitor_get_geometry (monitor, &geometry);

      geometry.x *= scale;
      geometry.y *= scale;
      geometry.width *= scale;
      geometry.height *= scale;

      pixbuf = get_pixbuf_for_size (bg, i, geometry.width, geometry.height);

      if (pixbuf != NULL)
        {
          draw_image_area (bg, i, pixbuf, dest, &geometry);
          g_object_unref (pixbuf);
        }
    }
}

static void
gf_bg_draw_at_scale (GfBG       *bg,
                     GdkPixbuf  *dest,
                     gint        scale,
                     GdkDisplay *display,
                     gboolean    is_root)
{
  if (is_root && (bg->placement != G_DESKTOP_BACKGROUND_STYLE_SPANNED))
    {
      draw_color_each_monitor (bg, dest, display, scale);

      if (bg->placement != G_DESKTOP_BACKGROUND_STYLE_NONE)
        draw_each_monitor (bg, dest, display, scale);
    }
  else
    {
      draw_color (bg, dest);

      if (bg->placement != G_DESKTOP_BACKGROUND_STYLE_NONE)
        draw_once (bg, dest);
    }
}

static void
gf_bg_get_pixmap_size (GfBG *bg,
                       int   width,
                       int   height,
                       int  *pixmap_width,
                       int  *pixmap_height)
{
	int dummy;
	
	if (!pixmap_width)
		pixmap_width = &dummy;
	if (!pixmap_height)
		pixmap_height = &dummy;
	
	*pixmap_width = width;
	*pixmap_height = height;

	if (!bg->filename) {
		switch (bg->color_type) {
		case G_DESKTOP_BACKGROUND_SHADING_SOLID:
			*pixmap_width = 1;
			*pixmap_height = 1;
			break;
			
		case G_DESKTOP_BACKGROUND_SHADING_HORIZONTAL:
		case G_DESKTOP_BACKGROUND_SHADING_VERTICAL:
		default:
			break;
		}
		
		return;
	}
}

/* Implementation of the pixbuf cache */
struct _SlideShow
{
	gint ref_count;
	double start_time;
	double total_duration;

	GQueue *slides;
	
	gboolean has_multiple_sizes;

	/* used during parsing */
	struct tm start_tm;
	GQueue *stack;
};


static GdkPixbuf *
blend (GdkPixbuf *p1,
       GdkPixbuf *p2,
       double alpha)
{
	GdkPixbuf *result = gdk_pixbuf_copy (p1);
	GdkPixbuf *tmp;

	if (gdk_pixbuf_get_width (p2) != gdk_pixbuf_get_width (p1) ||
            gdk_pixbuf_get_height (p2) != gdk_pixbuf_get_height (p1)) {
		tmp = gdk_pixbuf_scale_simple (p2, 
					       gdk_pixbuf_get_width (p1),
					       gdk_pixbuf_get_height (p1),
					       GDK_INTERP_BILINEAR);
	}
        else {
		tmp = g_object_ref (p2);
	}
	
	pixbuf_blend (tmp, result, 0, 0, -1, -1, 0, 0, alpha);
        
        g_object_unref (tmp);	

	return result;
}

typedef	enum {
	PIXBUF,
	SLIDESHOW
} FileType;

struct FileCacheEntry
{
	FileType type;
	char *filename;
	union {
		GdkPixbuf *pixbuf;
		GnomeBGSlideShow *slideshow;
	} u;
};

static void
file_cache_entry_delete (FileCacheEntry *ent)
{
	g_free (ent->filename);
	
	switch (ent->type) {
	case PIXBUF:
		g_object_unref (ent->u.pixbuf);
		break;
	case SLIDESHOW:
		g_object_unref (ent->u.slideshow);
		break;
	default:
		break;
	}

	g_free (ent);
}

static void
bound_cache (GfBG *bg)
{
      while (g_list_length (bg->file_cache) >= CACHE_SIZE) {
	      GList *last_link = g_list_last (bg->file_cache);
	      FileCacheEntry *ent = last_link->data;

	      file_cache_entry_delete (ent);

	      bg->file_cache = g_list_delete_link (bg->file_cache, last_link);
      }
}

static const FileCacheEntry *
file_cache_lookup (GfBG       *bg,
                   FileType    type,
                   const char *filename)
{
	GList *list;

	for (list = bg->file_cache; list != NULL; list = list->next) {
		FileCacheEntry *ent = list->data;

		if (ent && ent->type == type &&
		    strcmp (ent->filename, filename) == 0) {
			return ent;
		}
	}

	return NULL;
}

static FileCacheEntry *
file_cache_entry_new (GfBG       *bg,
                      FileType    type,
                      const char *filename)
{
	FileCacheEntry *ent = g_new0 (FileCacheEntry, 1);

	g_assert (!file_cache_lookup (bg, type, filename));
	
	ent->type = type;
	ent->filename = g_strdup (filename);

	bg->file_cache = g_list_prepend (bg->file_cache, ent);

	bound_cache (bg);
	
	return ent;
}

static void
file_cache_add_pixbuf (GfBG       *bg,
                       const char *filename,
                       GdkPixbuf  *pixbuf)
{
	FileCacheEntry *ent = file_cache_entry_new (bg, PIXBUF, filename);
	ent->u.pixbuf = g_object_ref (pixbuf);
}

static void
file_cache_add_slide_show (GfBG             *bg,
                           const char       *filename,
                           GnomeBGSlideShow *show)
{
	FileCacheEntry *ent = file_cache_entry_new (bg, SLIDESHOW, filename);
	ent->u.slideshow = g_object_ref (show);
}

static GdkPixbuf *
load_from_cache_file (GfBG       *bg,
                      const char *filename,
                      gint        num_monitor,
                      gint        best_width,
                      gint        best_height)
{
	GdkPixbuf *pixbuf = NULL;
	gchar *cache_filename;

	cache_filename = get_wallpaper_cache_filename (filename, num_monitor, bg->placement, best_width, best_height);
	if (cache_file_is_valid (filename, cache_filename))
		pixbuf = gdk_pixbuf_new_from_file (cache_filename, NULL);
	g_free (cache_filename);

	return pixbuf;
}

static GdkPixbuf *
get_as_pixbuf_for_size (GfBG       *bg,
                        const char *filename,
                        gint        num_monitor,
                        gint        best_width,
                        gint        best_height)
{
	const FileCacheEntry *ent;
	if ((ent = file_cache_lookup (bg, PIXBUF, filename))) {
		return g_object_ref (ent->u.pixbuf);
	}
	else {
		GdkPixbufFormat *format;
		GdkPixbuf *pixbuf;
                gchar *tmp;
		pixbuf = NULL;

		/* Try to hit local cache first if relevant */
		if (num_monitor != -1)
			pixbuf = load_from_cache_file (bg, filename, num_monitor, best_width, best_height);

		if (!pixbuf) {
			/* If scalable choose maximum size */
			format = gdk_pixbuf_get_file_info (filename, NULL, NULL);

			if (format != NULL) {
				tmp = gdk_pixbuf_format_get_name (format);
			} else {
				tmp = NULL;
			}

			if (tmp != NULL &&
			    strcmp (tmp, "svg") == 0 &&
			    (best_width > 0 && best_height > 0) &&
			    (bg->placement == G_DESKTOP_BACKGROUND_STYLE_STRETCHED ||
			     bg->placement == G_DESKTOP_BACKGROUND_STYLE_SCALED ||
			     bg->placement == G_DESKTOP_BACKGROUND_STYLE_ZOOM))
				pixbuf = gdk_pixbuf_new_from_file_at_size (filename, best_width, best_height, NULL);
			else
				pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
			g_free (tmp);
		}

		if (pixbuf)
			file_cache_add_pixbuf (bg, filename, pixbuf);

		return pixbuf;
	}
}

static GnomeBGSlideShow *
read_slideshow_file (const char  *filename,
                     GError     **err)
{
  GnomeBGSlideShow *show;

  show = gnome_bg_slide_show_new (filename);

  if (!gnome_bg_slide_show_load (show, err))
    {
      g_object_unref (show);
      return NULL;
    }

  return show;
}

static GnomeBGSlideShow *
get_as_slideshow (GfBG       *bg,
                  const char *filename)
{
	const FileCacheEntry *ent;
	if ((ent = file_cache_lookup (bg, SLIDESHOW, filename))) {
		return g_object_ref (ent->u.slideshow);
	}
	else {
		GnomeBGSlideShow *show = read_slideshow_file (filename, NULL);

		if (show)
			file_cache_add_slide_show (bg, filename, show);

		return show;
	}
}

static gboolean
blow_expensive_caches (gpointer data)
{
	GfBG *bg = data;
	GList *list, *next;
	
	bg->blow_caches_id = 0;
	
	for (list = bg->file_cache; list != NULL; list = next) {
		FileCacheEntry *ent = list->data;
		next = list->next;
		
		if (ent->type == PIXBUF) {
			file_cache_entry_delete (ent);
			bg->file_cache = g_list_delete_link (bg->file_cache,
							     list);
		}
	}

	if (bg->pixbuf_cache) {
		g_object_unref (bg->pixbuf_cache);
		bg->pixbuf_cache = NULL;
	}

	return FALSE;
}

static void
blow_expensive_caches_in_idle (GfBG *bg)
{
	if (bg->blow_caches_id == 0) {
		bg->blow_caches_id =
			g_idle_add (blow_expensive_caches,
				    bg);
	}
}


static gboolean
on_timeout (gpointer data)
{
	GfBG *bg = data;

	bg->timeout_id = 0;
	
	queue_transitioned (bg);

	return FALSE;
}

static double
get_slide_timeout (gboolean is_fixed,
                   gdouble  duration)
{
	double timeout;
	if (is_fixed) {
		timeout = duration;
	} else {
		/* Maybe the number of steps should be configurable? */
		
		/* In the worst case we will do a fade from 0 to 256, which mean
		 * we will never use more than 255 steps, however in most cases
		 * the first and last value are similar and users can't percieve
		 * changes in pixel values as small as 1/255th. So, lets not waste
		 * CPU cycles on transitioning to often.
		 *
		 * 64 steps is enough for each step to be just detectable in a 16bit
		 * color mode in the worst case, so we'll use this as an approximation
		 * of whats detectable.
		 */
		timeout = duration / 64.0;
	}
	return timeout;
}

static void
ensure_timeout (GfBG    *bg,
                gdouble  timeout)
{
	if (!bg->timeout_id) {
		/* G_MAXUINT means "only one slide" */
		if (timeout < G_MAXUINT) {
			bg->timeout_id = g_timeout_add_full (
				G_PRIORITY_LOW,
				timeout * 1000, on_timeout, bg, NULL);
		}
	}
}

static time_t
get_mtime (const char *filename)
{
	GFile     *file;
	GFileInfo *info;
	time_t     mtime;
	
	mtime = (time_t)-1;
	
	if (filename) {
		file = g_file_new_for_path (filename);
		info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
					  G_FILE_QUERY_INFO_NONE, NULL, NULL);
		if (info) {
			mtime = g_file_info_get_attribute_uint64 (info,
								  G_FILE_ATTRIBUTE_TIME_MODIFIED);
			g_object_unref (info);
		}
		g_object_unref (file);
	}
	
	return mtime;
}

static GdkPixbuf *
get_pixbuf_for_size (GfBG *bg,
                     gint  num_monitor,
                     gint  best_width,
                     gint  best_height)
{
	guint time_until_next_change;
	gboolean hit_cache = FALSE;

	/* only hit the cache if the aspect ratio matches */
	if (bg->pixbuf_cache) {
		int width, height;
		width = gdk_pixbuf_get_width (bg->pixbuf_cache);
		height = gdk_pixbuf_get_height (bg->pixbuf_cache);
		hit_cache = 0.2 > fabs ((best_width / (double)best_height) - (width / (double)height));
		if (!hit_cache) {
			g_object_unref (bg->pixbuf_cache);
			bg->pixbuf_cache = NULL;
		}
	}

	if (!hit_cache && bg->filename) {
		bg->file_mtime = get_mtime (bg->filename);

		bg->pixbuf_cache = get_as_pixbuf_for_size (bg, bg->filename, num_monitor, best_width, best_height);
		time_until_next_change = G_MAXUINT;
		if (!bg->pixbuf_cache) {
			GnomeBGSlideShow *show = get_as_slideshow (bg, bg->filename);

			if (show) {
				double alpha;
                                double duration;
                                gboolean is_fixed;
                                const char *file1;
                                const char *file2;

				g_object_ref (show);

				gnome_bg_slide_show_get_current_slide (show,
                                                                       best_width,
                                                                       best_height,
                                                                       &alpha,
                                                                       &duration,
                                                                       &is_fixed,
                                                                       &file1,
                                                                       &file2);
				time_until_next_change = (guint)get_slide_timeout (is_fixed, duration);
				if (is_fixed) {
					bg->pixbuf_cache = get_as_pixbuf_for_size (bg, file1, num_monitor, best_width, best_height);
				}
				else {
					GdkPixbuf *p1, *p2;
					p1 = get_as_pixbuf_for_size (bg, file1, num_monitor, best_width, best_height);
					p2 = get_as_pixbuf_for_size (bg, file2, num_monitor, best_width, best_height);

					if (p1 && p2) {
						bg->pixbuf_cache = blend (p1, p2, alpha);
					}
					if (p1)
						g_object_unref (p1);
					if (p2)
						g_object_unref (p2);
				}

				ensure_timeout (bg, time_until_next_change);

				g_object_unref (show);
			}
		}

		/* If the next slideshow step is a long time away then
		   we blow away the expensive stuff (large pixbufs) from
		   the cache */
		if (time_until_next_change > KEEP_EXPENSIVE_CACHE_SECS)
		    blow_expensive_caches_in_idle (bg);
	}

	if (bg->pixbuf_cache)
		g_object_ref (bg->pixbuf_cache);

	return bg->pixbuf_cache;
}

static gboolean
is_different (GfBG       *bg,
              const char *filename)
{
	if (!filename && bg->filename) {
		return TRUE;
	}
	else if (filename && !bg->filename) {
		return TRUE;
	}
	else if (!filename && !bg->filename) {
		return FALSE;
	}
	else {
		time_t mtime = get_mtime (filename);
		
		if (mtime != bg->file_mtime)
			return TRUE;
		
		if (strcmp (filename, bg->filename) != 0)
			return TRUE;
		
		return FALSE;
	}
}

static void
clear_cache (GfBG *bg)
{
	GList *list;

	if (bg->file_cache) {
		for (list = bg->file_cache; list != NULL; list = list->next) {
			FileCacheEntry *ent = list->data;
			
			file_cache_entry_delete (ent);
		}
		g_list_free (bg->file_cache);
		bg->file_cache = NULL;
	}
	
	if (bg->pixbuf_cache) {
		g_object_unref (bg->pixbuf_cache);
		
		bg->pixbuf_cache = NULL;
	}

	if (bg->timeout_id) {
		g_source_remove (bg->timeout_id);

		bg->timeout_id = 0;
	}
}

static void
pixbuf_average_value (GdkPixbuf *pixbuf,
                      GdkRGBA   *result)
{
	guint64 a_total, r_total, g_total, b_total;
	guint row, column;
	int row_stride;
	const guchar *pixels, *p;
	int r, g, b, a;
	guint64 dividend;
	guint width, height;
	gdouble dd;
	
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	row_stride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	
	/* iterate through the pixbuf, counting up each component */
	a_total = 0;
	r_total = 0;
	g_total = 0;
	b_total = 0;
	
	if (gdk_pixbuf_get_has_alpha (pixbuf)) {
		for (row = 0; row < height; row++) {
			p = pixels + (row * row_stride);
			for (column = 0; column < width; column++) {
				r = *p++;
				g = *p++;
				b = *p++;
				a = *p++;
				
				a_total += a;
				r_total += (guint64) r * a;
				g_total += (guint64) g * a;
				b_total += (guint64) b * a;
			}
		}
		dividend = (guint64) height * width * 0xFF;
		a_total *= 0xFF;
	} else {
		for (row = 0; row < height; row++) {
			p = pixels + (row * row_stride);
			for (column = 0; column < width; column++) {
				r = *p++;
				g = *p++;
				b = *p++;
				
				r_total += r;
				g_total += g;
				b_total += b;
			}
		}
		dividend = (guint64) height * width;
		a_total = dividend * 0xFF;
	}

	dd = dividend * 0xFF;
	result->alpha = a_total / dd;
	result->red = r_total / dd;
	result->green = g_total / dd;
	result->blue = b_total / dd;
}

static GdkPixbuf *
pixbuf_scale_to_fit (GdkPixbuf *src, int max_width, int max_height)
{
	double factor;
	int src_width, src_height;
	int new_width, new_height;
	
	src_width = gdk_pixbuf_get_width (src);
	src_height = gdk_pixbuf_get_height (src);
	
	factor = MIN (max_width  / (double) src_width, max_height / (double) src_height);
	
	new_width  = floor (src_width * factor + 0.5);
	new_height = floor (src_height * factor + 0.5);
	
	return gdk_pixbuf_scale_simple (src, new_width, new_height, GDK_INTERP_BILINEAR);	
}

static GdkPixbuf *
pixbuf_scale_to_min (GdkPixbuf *src, int min_width, int min_height)
{
	double factor;
	int src_width, src_height;
	int new_width, new_height;
	GdkPixbuf *dest;

	src_width = gdk_pixbuf_get_width (src);
	src_height = gdk_pixbuf_get_height (src);

	factor = MAX (min_width / (double) src_width, min_height / (double) src_height);

	new_width = floor (src_width * factor + 0.5);
	new_height = floor (src_height * factor + 0.5);

	dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			       gdk_pixbuf_get_has_alpha (src),
			       8, min_width, min_height);
	if (!dest)
		return NULL;

	/* crop the result */
	gdk_pixbuf_scale (src, dest,
			  0, 0,
			  min_width, min_height,
			  (new_width - min_width) / -2,
			  (new_height - min_height) / -2,
			  factor,
			  factor,
			  GDK_INTERP_BILINEAR);
	return dest;
}

static guchar *
create_gradient (const GdkRGBA *primary,
		 const GdkRGBA *secondary,
		 int	        n_pixels)
{
	guchar *result = g_malloc (n_pixels * 3);
	int i;
	
	for (i = 0; i < n_pixels; ++i) {
		double ratio = (i + 0.5) / n_pixels;
		
		result[3 * i + 0] = (int) (0.5 + (primary->red * (1 - ratio) + secondary->red * ratio) * 255);
		result[3 * i + 1] = (int) (0.5 + (primary->green * (1 - ratio) + secondary->green * ratio) * 255);
		result[3 * i + 2] = (int) (0.5 + (primary->blue * (1 - ratio) + secondary->blue * ratio) * 255);
	}
	
	return result;
}	

static void
pixbuf_draw_gradient (GdkPixbuf    *pixbuf,
		      gboolean      horizontal,
		      GdkRGBA      *primary,
		      GdkRGBA      *secondary,
		      GdkRectangle *rect)
{
	int width;
	int height;
	int rowstride;
	guchar *dst;
	int n_channels = 3;

	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	width = rect->width;
	height = rect->height;
	dst = gdk_pixbuf_get_pixels (pixbuf) + rect->x * n_channels + rowstride * rect->y;

	if (horizontal) {
		guchar *gradient = create_gradient (primary, secondary, width);
		int copy_bytes_per_row = width * n_channels;
		int i;

		for (i = 0; i < height; i++) {
			guchar *d;
			d = dst + rowstride * i;
			memcpy (d, gradient, copy_bytes_per_row);
		}
		g_free (gradient);
	} else {
		guchar *gb, *gradient;
		int i;

		gradient = create_gradient (primary, secondary, height);
		for (i = 0; i < height; i++) {
			int j;
			guchar *d;

			d = dst + rowstride * i;
			gb = gradient + n_channels * i;
			for (j = width; j > 0; j--) {
				int k;

				for (k = 0; k < n_channels; k++) {
					*(d++) = gb[k];
				}
			}
		}

		g_free (gradient);
	}
}

static void
pixbuf_blend (GdkPixbuf *src,
	      GdkPixbuf *dest,
	      int	 src_x,
	      int	 src_y,
	      int	 src_width,
	      int        src_height,
	      int	 dest_x,
	      int	 dest_y,
	      double	 alpha)
{
	int dest_width = gdk_pixbuf_get_width (dest);
	int dest_height = gdk_pixbuf_get_height (dest);
	int offset_x = dest_x - src_x;
	int offset_y = dest_y - src_y;

	if (src_width < 0)
		src_width = gdk_pixbuf_get_width (src);

	if (src_height < 0)
		src_height = gdk_pixbuf_get_height (src);
	
	if (dest_x < 0)
		dest_x = 0;
	
	if (dest_y < 0)
		dest_y = 0;
	
	if (dest_x + src_width > dest_width) {
		src_width = dest_width - dest_x;
	}
	
	if (dest_y + src_height > dest_height) {
		src_height = dest_height - dest_y;
	}

	gdk_pixbuf_composite (src, dest,
			      dest_x, dest_y,
			      src_width, src_height,
			      offset_x, offset_y,
			      1, 1, GDK_INTERP_NEAREST,
			      alpha * 0xFF + 0.5);
}

static void
pixbuf_tile (GdkPixbuf *src, GdkPixbuf *dest)
{
	int x, y;
	int tile_width, tile_height;
	int dest_width = gdk_pixbuf_get_width (dest);
	int dest_height = gdk_pixbuf_get_height (dest);
	tile_width = gdk_pixbuf_get_width (src);
	tile_height = gdk_pixbuf_get_height (src);
	
	for (y = 0; y < dest_height; y += tile_height) {
		for (x = 0; x < dest_width; x += tile_width) {
			pixbuf_blend (src, dest, 0, 0,
				      tile_width, tile_height, x, y, 1.0);
		}
	}
}

static Pixmap
create_persistent_pixmap (int width,
                          int height)
{
  Display *xdisplay;
  int depth;
  Pixmap pixmap;

  xdisplay = XOpenDisplay (NULL);
  if (xdisplay == NULL)
    return None;

  /* Desktop background pixmap should be created from
   * dummy X client since most applications will try to
   * kill it with XKillClient later when changing pixmap
   */
  XSetCloseDownMode (xdisplay, RetainPermanent);

  depth = DefaultDepth (xdisplay, DefaultScreen (xdisplay));
  pixmap = XCreatePixmap (xdisplay,
                          XDefaultRootWindow (xdisplay),
                          width,
                          height,
                          depth);

  XCloseDisplay (xdisplay);

  return pixmap;
}

static cairo_surface_t *
create_persistent_surface (GdkDisplay *display,
                           int         width,
                           int         height)
{
  Display *xdisplay;
  Pixmap pixmap;
  Visual *xvisual;
  cairo_surface_t *surface;

  xdisplay = gdk_x11_display_get_xdisplay (display);

  pixmap = create_persistent_pixmap (width, height);
  xvisual = DefaultVisual (xdisplay, DefaultScreen (xdisplay));

  surface = cairo_xlib_surface_create (xdisplay,
                                       pixmap,
                                       xvisual,
                                       width,
                                       height);

  return surface;
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

static gboolean
settings_change_event_cb (GSettings *settings,
                          void      *keys,
                          int        n_keys,
                          GfBG      *self)
{
  gf_bg_load_from_preferences (self);

  return TRUE;
}

static gboolean
color_scheme_changed_cb (GSettings  *interface_settings,
                         const char *key,
                         GfBG       *self)
{
  gf_bg_load_from_preferences (self);

  return FALSE;
}

static void
gf_bg_constructed (GObject *object)
{
  GfBG *self;
  GSettingsSchema *schema;

  self = GF_BG (object);

  G_OBJECT_CLASS (gf_bg_parent_class)->constructed (object);

  self->settings = g_settings_new (self->schema_id);
  self->interface_settings = g_settings_new ("org.gnome.desktop.interface");

  g_object_get (self->settings, "settings-schema", &schema, NULL);
  self->has_dark_key = g_settings_schema_has_key (schema, BG_KEY_PICTURE_URI_DARK);
  g_settings_schema_unref (schema);

  self->change_event_id = g_signal_connect (self->settings,
                                            "change-event",
                                            G_CALLBACK (settings_change_event_cb),
                                            self);

  self->changed_color_scheme_id = g_signal_connect (self->interface_settings,
                                                    "changed::color-scheme",
                                                    G_CALLBACK (color_scheme_changed_cb),
                                                    self);
}

static void
gf_bg_dispose (GObject *object)
{
  GfBG *self;

  self = GF_BG (object);

  if (self->change_event_id != 0)
    {
      g_signal_handler_disconnect (self->settings, self->change_event_id);
      self->change_event_id = 0;
    }

  if (self->changed_color_scheme_id != 0)
    {
      g_signal_handler_disconnect (self->interface_settings, self->changed_color_scheme_id);
      self->changed_color_scheme_id = 0;
    }

  g_clear_object (&self->settings);
  g_clear_object (&self->interface_settings);
  g_clear_object (&self->file_monitor);

  clear_cache (self);

  G_OBJECT_CLASS (gf_bg_parent_class)->dispose (object);
}

static void
gf_bg_finalize (GObject *object)
{
  GfBG *self;

  self = GF_BG (object);

  g_clear_pointer (&self->schema_id, g_free);

  if (self->changed_id != 0)
    {
      g_source_remove (self->changed_id);
      self->changed_id = 0;
    }

  if (self->transitioned_id != 0)
    {
      g_source_remove (self->transitioned_id);
      self->transitioned_id = 0;
    }

  if (self->blow_caches_id != 0)
    {
      g_source_remove (self->blow_caches_id);
      self->blow_caches_id = 0;
    }

  g_clear_pointer (&self->filename, g_free);

  G_OBJECT_CLASS (gf_bg_parent_class)->finalize (object);
}

static void
gf_bg_set_property (GObject      *object,
                    unsigned int  property_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
  GfBG *self;

  self = GF_BG (object);

  switch (property_id)
    {
      case PROP_SCHEMA_ID:
        g_assert (self->schema_id == NULL);
        self->schema_id = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  bg_properties[PROP_SCHEMA_ID] =
    g_param_spec_string ("schema-id",
                         "schema-id",
                         "schema-id",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, bg_properties);
}

static void
gf_bg_class_init (GfBGClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = gf_bg_constructed;
  object_class->dispose = gf_bg_dispose;
  object_class->finalize = gf_bg_finalize;
  object_class->set_property = gf_bg_set_property;

  install_properties (object_class);

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  signals[TRANSITIONED] =
    g_signal_new ("transitioned",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

static void
gf_bg_init (GfBG *self)
{
}

GfBG *
gf_bg_new (const char *schema_id)
{
  return g_object_new (GF_TYPE_BG,
                       "schema-id", schema_id,
                       NULL);
}

void
gf_bg_load_from_preferences (GfBG *self)
{
  char *tmp;
  char *filename;
  GDesktopBackgroundShading ctype;
  GdkRGBA c1;
  GdkRGBA c2;
  GDesktopBackgroundStyle placement;

  g_return_if_fail (GF_IS_BG (self));

  /* Filename */
  tmp = g_settings_get_string (self->interface_settings, IFACE_KEY_COLOR_SCHEME);
  if (self->has_dark_key && strcmp (tmp, "prefer-dark") == 0)
    filename = g_settings_get_mapped (self->settings,
                                      BG_KEY_PICTURE_URI_DARK,
                                      bg_gsettings_mapping,
                                      NULL);
  else
    filename = g_settings_get_mapped (self->settings,
                                      BG_KEY_PICTURE_URI,
                                      bg_gsettings_mapping,
                                      NULL);
  g_free (tmp);

  /* Colors */
  tmp = g_settings_get_string (self->settings, BG_KEY_PRIMARY_COLOR);
  color_from_string (tmp, &c1);
  g_free (tmp);

  tmp = g_settings_get_string (self->settings, BG_KEY_SECONDARY_COLOR);
  color_from_string (tmp, &c2);
  g_free (tmp);

  /* Color type */
  ctype = g_settings_get_enum (self->settings, BG_KEY_COLOR_TYPE);

  /* Placement */
  placement = g_settings_get_enum (self->settings, BG_KEY_PICTURE_PLACEMENT);

  gf_bg_set_rgba (self, ctype, &c1, &c2);
  gf_bg_set_placement (self, placement);
  gf_bg_set_filename (self, filename);

  g_free (filename);
}

void
gf_bg_save_to_preferences (GfBG *self)
{
  gchar *primary;
  gchar *secondary;
  gchar *uri;

  g_return_if_fail (GF_IS_BG (self));

  primary = color_to_string (&self->primary);
  secondary = color_to_string (&self->secondary);

  g_settings_delay (self->settings);

  uri = NULL;

  if (self->filename != NULL)
    uri = g_filename_to_uri (self->filename, NULL, NULL);

  if (uri == NULL)
    uri = g_strdup ("");

  g_settings_set_string (self->settings, BG_KEY_PICTURE_URI, uri);
  g_settings_set_string (self->settings, BG_KEY_PRIMARY_COLOR, primary);
  g_settings_set_string (self->settings, BG_KEY_SECONDARY_COLOR, secondary);
  g_settings_set_enum (self->settings, BG_KEY_COLOR_TYPE, self->color_type);
  g_settings_set_enum (self->settings, BG_KEY_PICTURE_PLACEMENT, self->placement);

  /* Apply changes atomically. */
  g_settings_apply (self->settings);

  g_free (primary);
  g_free (secondary);
  g_free (uri);
}

void
gf_bg_set_filename (GfBG       *self,
                    const char *filename)
{
  g_return_if_fail (self != NULL);

  if (!is_different (self, filename))
    return;

  g_free (self->filename);

  self->filename = g_strdup (filename);
  self->file_mtime = get_mtime (self->filename);

  g_clear_object (&self->file_monitor);

  if (self->filename != NULL)
    {
      GFile *f;

      f = g_file_new_for_path (self->filename);
      self->file_monitor = g_file_monitor_file (f, 0, NULL, NULL);
      g_object_unref (f);

      g_signal_connect (self->file_monitor,
                        "changed",
                        G_CALLBACK (file_changed),
                        self);
    }

  clear_cache (self);
  queue_changed (self);
}

void
gf_bg_set_placement (GfBG                    *self,
                     GDesktopBackgroundStyle  placement)
{
  g_return_if_fail (self != NULL);

  if (self->placement != placement)
    {
      self->placement = placement;

      queue_changed (self);
    }
}

void
gf_bg_set_rgba (GfBG                      *self,
                GDesktopBackgroundShading  type,
                GdkRGBA                   *primary,
                GdkRGBA                   *secondary)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (primary != NULL);

  if (self->color_type != type ||
      !gdk_rgba_equal (&self->primary, primary) ||
      (secondary != NULL && !gdk_rgba_equal (&self->secondary, secondary)))
    {
      self->color_type = type;
      self->primary = *primary;

      if (secondary != NULL)
        self->secondary = *secondary;

      queue_changed (self);
    }
}

cairo_surface_t *
gf_bg_create_surface (GfBG      *self,
                      GdkWindow *window,
                      int        width,
                      int        height,
                      gboolean   root)
{
  gint scale;
  int pm_width;
  int pm_height;
  cairo_surface_t *surface;
  GdkRGBA average;
  cairo_t *cr;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (window != NULL, NULL);

  scale = gdk_window_get_scale_factor (window);

  if (self->pixbuf_cache &&
      gdk_pixbuf_get_width (self->pixbuf_cache) != width &&
      gdk_pixbuf_get_height (self->pixbuf_cache) != height)
    {
      g_object_unref (self->pixbuf_cache);
      self->pixbuf_cache = NULL;
    }

  /* has the side effect of loading and caching pixbuf only when in tile mode */
  gf_bg_get_pixmap_size (self, width, height, &pm_width, &pm_height);

  if (root)
    {
      surface = create_persistent_surface (gdk_window_get_display (window),
                                           scale * pm_width,
                                           scale * pm_height);

      cairo_surface_set_device_scale (surface, scale, scale);
    }
  else
    {
      surface = gdk_window_create_similar_surface (window,
                                                   CAIRO_CONTENT_COLOR,
                                                   pm_width,
                                                   pm_height);
    }

  if (surface == NULL)
    return NULL;

  cr = cairo_create (surface);

  if (self->filename == NULL &&
      self->color_type == G_DESKTOP_BACKGROUND_SHADING_SOLID)
    {
      gdk_cairo_set_source_rgba (cr, &self->primary);
      average = self->primary;
    }
  else
    {
      GdkPixbuf *pixbuf;
      cairo_surface_t *pixbuf_surface;

      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                               FALSE,
                               8,
                               scale * width,
                               scale * height);

      gf_bg_draw_at_scale (self,
                           pixbuf,
                           scale,
                           gdk_window_get_display (window),
                           root);

      pixbuf_average_value (pixbuf, &average);

      pixbuf_surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, 0, window);
      cairo_set_source_surface (cr, pixbuf_surface, 0, 0);

      cairo_surface_destroy (pixbuf_surface);
      g_object_unref (pixbuf);
    }

  cairo_paint (cr);
  cairo_destroy (cr);

  cairo_surface_set_user_data (surface,
                               &average_color_key,
                               gdk_rgba_copy (&average),
                               (cairo_destroy_func_t) gdk_rgba_free);

  return surface;
}

void
gf_bg_set_surface_as_root (GdkDisplay      *display,
                           cairo_surface_t *surface)
{
  Display *xdisplay;
  Pixmap pixmap;
  GdkRGBA *average;

  g_return_if_fail (surface != NULL);
  g_return_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_XLIB);

  xdisplay = gdk_x11_display_get_xdisplay (display);
  pixmap = cairo_xlib_surface_get_drawable (surface);
  average = cairo_surface_get_user_data (surface, &average_color_key);

  gdk_x11_display_grab (display);

  set_root_pixmap (display, pixmap);
  set_average_color (display, average);

  XSetWindowBackgroundPixmap (xdisplay, XDefaultRootWindow (xdisplay), pixmap);
  XClearWindow (xdisplay, XDefaultRootWindow (xdisplay));
  XFlush (xdisplay);

  gdk_x11_display_ungrab (display);
}

cairo_surface_t *
gf_bg_get_surface_from_root (GdkDisplay *display,
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

GdkRGBA *
gf_bg_get_average_color_from_surface (cairo_surface_t *surface)
{
  return cairo_surface_get_user_data (surface, &average_color_key);
}
