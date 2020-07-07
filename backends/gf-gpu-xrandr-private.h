/*
 * Copyright (C) 2017 Red Hat
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
 */

#ifndef GF_GPU_XRANDR_PRIVATE_H
#define GF_GPU_XRANDR_PRIVATE_H

#include <glib-object.h>
#include <X11/extensions/Xrandr.h>

#include "gf-backend-x11-private.h"
#include "gf-gpu-private.h"

G_BEGIN_DECLS

#define GF_TYPE_GPU_XRANDR (gf_gpu_xrandr_get_type ())
G_DECLARE_FINAL_TYPE (GfGpuXrandr, gf_gpu_xrandr, GF, GPU_XRANDR, GfGpu)

GfGpuXrandr        *gf_gpu_xrandr_new                 (GfBackendX11 *backend_x11);

XRRScreenResources *gf_gpu_xrandr_get_resources       (GfGpuXrandr  *self);

void                gf_gpu_xrandr_get_max_screen_size (GfGpuXrandr  *self,
                                                       int          *max_width,
                                                       int          *max_height);

G_END_DECLS

#endif
