/*
 * Copyright (C) 2017-2019 Alberts Muktupāvels
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
 * - src/backends/x11/cm/meta-backend-x11-cm.c
 */

#include "config.h"
#include "gf-backend-x11-cm-private.h"
#include "gf-gpu-xrandr-private.h"
#include "gf-monitor-manager-xrandr-private.h"

struct _GfBackendX11Cm
{
  GfBackendX11 parent;
};

G_DEFINE_TYPE (GfBackendX11Cm, gf_backend_x11_cm, GF_TYPE_BACKEND_X11)

static GfMonitorManager *
gf_backend_x11_cm_create_monitor_manager (GfBackend  *backend,
                                          GError    **error)
{
  return g_object_new (GF_TYPE_MONITOR_MANAGER_XRANDR,
                       "backend", backend,
                       NULL);
}

static gboolean
gf_backend_x11_cm_handle_host_xevent (GfBackendX11 *x11,
                                      XEvent       *event)
{
  GfBackend *backend;
  GfMonitorManager *monitor_manager;
  GfMonitorManagerXrandr *xrandr;

  backend = GF_BACKEND (x11);
  monitor_manager = gf_backend_get_monitor_manager (backend);
  xrandr = GF_MONITOR_MANAGER_XRANDR (monitor_manager);

  return gf_monitor_manager_xrandr_handle_xevent (xrandr, event);
}

static void
gf_backend_x11_cm_class_init (GfBackendX11CmClass *x11_cm_class)
{
  GfBackendClass *backend_class;
  GfBackendX11Class *backend_x11_class;

  backend_class = GF_BACKEND_CLASS (x11_cm_class);
  backend_x11_class = GF_BACKEND_X11_CLASS (x11_cm_class);

  backend_class->create_monitor_manager = gf_backend_x11_cm_create_monitor_manager;

  backend_x11_class->handle_host_xevent = gf_backend_x11_cm_handle_host_xevent;
}

static void
gf_backend_x11_cm_init (GfBackendX11Cm *self)
{
  GfGpuXrandr *gpu_xrandr;

  /*
   * The X server deals with multiple GPUs for us, so we just see what the X
   * server gives us as one single GPU, even though it may actually be backed
   * by multiple.
   */
  gpu_xrandr = gf_gpu_xrandr_new (GF_BACKEND_X11 (self));
  gf_backend_add_gpu (GF_BACKEND (self), GF_GPU (gpu_xrandr));
}
