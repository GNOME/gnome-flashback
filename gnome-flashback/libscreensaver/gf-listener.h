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

#ifndef GF_LISTENER_H
#define GF_LISTENER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GF_TYPE_LISTENER gf_listener_get_type ()
G_DECLARE_FINAL_TYPE (GfListener, gf_listener, GF, LISTENER, GObject)

GfListener *gf_listener_new              (void);

gboolean    gf_listener_set_active       (GfListener *listener,
                                          gboolean    active);

gboolean    gf_listener_get_active       (GfListener *listener);

gboolean    gf_listener_set_session_idle (GfListener *listener,
                                          gboolean    session_idle);

G_END_DECLS

#endif
