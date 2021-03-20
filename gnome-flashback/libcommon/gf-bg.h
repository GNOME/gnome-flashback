/* gnome-bg.h - 

   Copyright 2007, Red Hat, Inc.

   This file is part of the Gnome Library.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: Soren Sandmann <sandmann@redhat.com>
*/

#ifndef GF_BG_H
#define GF_BG_H

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gdesktop-enums.h>
#include <libgnome-desktop/gnome-bg-crossfade.h>
#include <gdesktop-enums.h>

G_BEGIN_DECLS

#define GF_TYPE_BG (gf_bg_get_type ())
G_DECLARE_FINAL_TYPE (GfBG, gf_bg, GF, BG, GObject)

GfBG *           gf_bg_new                      (void);
void             gf_bg_load_from_preferences    (GfBG                  *bg,
						 GSettings             *settings);
void             gf_bg_save_to_preferences      (GfBG                  *bg,
						 GSettings             *settings);
/* Setters */
void             gf_bg_set_filename             (GfBG                  *bg,
						 const char            *filename);
void             gf_bg_set_placement            (GfBG                  *bg,
						 GDesktopBackgroundStyle placement);
void             gf_bg_set_rgba                 (GfBG                  *bg,
						 GDesktopBackgroundShading type,
						 GdkRGBA               *primary,
						 GdkRGBA               *secondary);

cairo_surface_t *gf_bg_create_surface           (GfBG                  *bg,
						 GdkWindow             *window,
						 int                    width,
						 int                    height,
						 gboolean               root);
gboolean         gf_bg_is_dark                  (GfBG                  *bg,
                                                 int                    dest_width,
						 int                    dest_height);

void             gf_bg_set_surface_as_root      (GdkScreen             *screen,
						 cairo_surface_t       *surface);

GnomeBGCrossfade *gf_bg_set_surface_as_root_with_crossfade    (GdkScreen *screen,
                                                              cairo_surface_t *surface);
cairo_surface_t *gf_bg_get_surface_from_root (GdkScreen *screen);

G_END_DECLS

#endif
