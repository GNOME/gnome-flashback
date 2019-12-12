/*
 * Copyright (C) 2004-2006 William Jon McCann
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     William Jon McCann <mccann@jhu.edu>
 */

#ifndef GF_GRAB_H
#define GF_GRAB_H

#include <gdk/gdk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GF_TYPE_GRAB (gf_grab_get_type ())
G_DECLARE_FINAL_TYPE (GfGrab, gf_grab, GF, GRAB, GObject)

GfGrab   *gf_grab_new (void);

gboolean  gf_grab_grab_root      (GfGrab    *self);

gboolean  gf_grab_grab_offscreen (GfGrab    *self);

void      gf_grab_move_to_window (GfGrab    *self,
                                  GdkWindow *window);

void      gf_grab_release        (GfGrab    *self);

G_END_DECLS

#endif
