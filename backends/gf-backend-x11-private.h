/*
 * Copyright (C) 2014 Red Hat
 * Copyright (C) 2017 Alberts Muktupāvels
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
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 *
 * Adapted from mutter:
 * - src/backends/x11/meta-backend-x11.h
 */

#ifndef GF_BACKEND_X11_PRIVATE_H
#define GF_BACKEND_X11_PRIVATE_H

#include <X11/Xlib.h>
#include "gf-backend-private.h"

G_BEGIN_DECLS

#define GF_TYPE_BACKEND_X11 (gf_backend_x11_get_type ())
G_DECLARE_DERIVABLE_TYPE (GfBackendX11, gf_backend_x11,
                          GF, BACKEND_X11, GfBackend)

struct _GfBackendX11Class
{
  GfBackendClass parent_class;

  gboolean (* handle_host_xevent) (GfBackendX11 *x11,
                                   XEvent       *event);
};

Display *gf_backend_x11_get_xdisplay (GfBackendX11 *x11);

G_END_DECLS

#endif
