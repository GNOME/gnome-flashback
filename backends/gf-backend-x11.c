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
 * - src/backends/x11/meta-backend-x11.c
 */

#include "config.h"

#include <gio/gio.h>

#include "gf-backend-x11-private.h"

static void
initable_iface_init (GInitableIface *initable_iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GfBackendX11, gf_backend_x11, GF_TYPE_BACKEND,
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         initable_iface_init))

static gboolean
gf_backend_x11_initable_init (GInitable     *initable,
                              GCancellable  *cancellable,
                              GError       **error)
{
  GfBackendX11 *x11;
  GInitableIface *parent_iface;

  x11 = GF_BACKEND_X11 (initable);
  parent_iface = g_type_interface_peek_parent (G_INITABLE_GET_IFACE (x11));

  return parent_iface->init (initable, cancellable, error);
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = gf_backend_x11_initable_init;
}

static void
gf_backend_x11_class_init (GfBackendX11Class *x11_class)
{
}

static void
gf_backend_x11_init (GfBackendX11 *x11)
{
}
