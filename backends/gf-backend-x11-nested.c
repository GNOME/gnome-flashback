/*
 * Copyright (C) 2017 Alberts Muktupāvels
 * Copyright (C) 2017 Red Hat
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
 * Adapted from mutter:
 * - src/backends/x11/nested/meta-backend-x11-nested.c
 */

#include "config.h"
#include "gf-backend-x11-nested-private.h"
#include "gf-monitor-manager-dummy-private.h"

struct _GfBackendX11Nested
{
  GfBackendX11 parent;
};

G_DEFINE_TYPE (GfBackendX11Nested, gf_backend_x11_nested, GF_TYPE_BACKEND_X11)

static GfMonitorManager *
gf_backend_x11_nested_create_monitor_manager (GfBackend *backend)
{
  return g_object_new (GF_TYPE_MONITOR_MANAGER_DUMMY,
                       "backend", backend,
                       NULL);
}

static gboolean
gf_backend_x11_nested_handle_host_xevent (GfBackendX11 *x11,
                                          XEvent       *event)
{
  return FALSE;
}

static void
gf_backend_x11_nested_class_init (GfBackendX11NestedClass *x11_nested_class)
{
  GfBackendClass *backend_class;
  GfBackendX11Class *backend_x11_class;

  backend_class = GF_BACKEND_CLASS (x11_nested_class);
  backend_x11_class = GF_BACKEND_X11_CLASS (x11_nested_class);

  backend_class->create_monitor_manager = gf_backend_x11_nested_create_monitor_manager;

  backend_x11_class->handle_host_xevent = gf_backend_x11_nested_handle_host_xevent;
}

static void
gf_backend_x11_nested_init (GfBackendX11Nested *x11_nested)
{
}
