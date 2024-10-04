/*
 * Copyright 2007, Red Hat, Inc.
 * Copyright 2019-2021 Alberts Muktupāvels
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
 * Author:
 *   Soren Sandmann <sandmann@redhat.com>
 */

#ifndef GF_BG_H
#define GF_BG_H

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gdesktop-enums.h>

G_BEGIN_DECLS

#define GF_TYPE_BG (gf_bg_get_type ())
G_DECLARE_FINAL_TYPE (GfBG, gf_bg, GF, BG, GObject)

GfBG            *gf_bg_new                            (const char                *schema_id);

void             gf_bg_load_from_preferences          (GfBG                      *self);

void             gf_bg_save_to_preferences            (GfBG                      *self);

void             gf_bg_set_filename                   (GfBG                      *self,
                                                       const char                *filename);

void             gf_bg_set_placement                  (GfBG                      *self,
                                                       GDesktopBackgroundStyle    placement);

void             gf_bg_set_rgba                       (GfBG                      *self,
                                                       GDesktopBackgroundShading  type,
                                                       GdkRGBA                   *primary,
                                                       GdkRGBA                   *secondary);

cairo_surface_t *gf_bg_create_surface                 (GfBG                      *self,
                                                       GdkWindow                 *window,
                                                       int                        width,
                                                       int                        height,
                                                       gboolean                   root);

void             gf_bg_set_surface_as_root            (GdkDisplay                *display,
                                                       cairo_surface_t           *surface);

cairo_surface_t *gf_bg_get_surface_from_root          (GdkDisplay                *display,
                                                       int                        width,
                                                       int                        height);

GdkRGBA         *gf_bg_get_average_color_from_surface (cairo_surface_t           *surface);

G_END_DECLS

#endif
