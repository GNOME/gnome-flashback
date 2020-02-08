/*
 * Copyright (C) 2004-2005 William Jon McCann
 * Copyright (C) 2020 Alberts Muktupāvels
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

#ifndef GF_WINDOW_H
#define GF_WINDOW_H

#include <gtk/gtk.h>
#include <libinput-sources/gf-input-sources.h>

G_BEGIN_DECLS

#define GF_TYPE_WINDOW (gf_window_get_type ())
G_DECLARE_FINAL_TYPE (GfWindow, gf_window, GF, WINDOW, GtkWindow)

GfWindow   *gf_window_new                     (GdkMonitor      *monitor);

void        gf_window_set_input_sources       (GfWindow        *self,
                                               GfInputSources  *input_sources);

GdkMonitor *gf_window_get_monitor             (GfWindow        *self);

void        gf_window_set_background          (GfWindow        *self,
                                               cairo_surface_t *surface);

void        gf_window_set_lock_enabled        (GfWindow        *self,
                                               gboolean         lock_enabled);

void        gf_window_set_user_switch_enabled (GfWindow        *self,
                                               gboolean         user_switch_enabled);

void        gf_window_show_message            (GfWindow        *self,
                                               const char      *summary,
                                               const char      *body,
                                               const char      *icon);

void        gf_window_request_unlock          (GfWindow        *self);

void        gf_window_cancel_unlock_request   (GfWindow        *self);

G_END_DECLS

#endif
