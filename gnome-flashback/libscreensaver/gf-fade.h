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

#ifndef GF_FADE_H
#define GF_FADE_H

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GF_TYPE_FADE (gf_fade_get_type ())
G_DECLARE_FINAL_TYPE (GfFade, gf_fade, GF, FADE, GObject)

GfFade   *gf_fade_new    (void);

void      gf_fade_async  (GfFade               *self,
                          guint                 timeout,
                          GCancellable         *cancellable,
                          GAsyncReadyCallback   callback,
                          gpointer              user_data);

gboolean  gf_fade_finish (GfFade               *self,
                          GAsyncResult         *result,
                          GError              **error);

void      gf_fade_reset  (GfFade               *self);

G_END_DECLS

#endif
