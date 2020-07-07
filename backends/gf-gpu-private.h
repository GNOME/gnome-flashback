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

#ifndef GF_GPU_PRIVATE_H
#define GF_GPU_PRIVATE_H

#include <glib-object.h>

#include "gf-monitor-manager-private.h"

G_BEGIN_DECLS

#define GF_TYPE_GPU (gf_gpu_get_type ())
G_DECLARE_DERIVABLE_TYPE (GfGpu, gf_gpu, GF, GPU, GObject)

struct _GfGpuClass
{
  GObjectClass parent_class;

  gboolean (* read_current) (GfGpu   *gpu,
                             GError **error);
};

gboolean   gf_gpu_read_current            (GfGpu   *self,
                                           GError **error);

gboolean   gf_gpu_has_hotplug_mode_update (GfGpu   *self);

GfBackend *gf_gpu_get_backend             (GfGpu   *self);

GList     *gf_gpu_get_outputs             (GfGpu   *self);

GList     *gf_gpu_get_crtcs               (GfGpu   *self);

GList     *gf_gpu_get_modes               (GfGpu   *self);

void       gf_gpu_take_outputs            (GfGpu   *self,
                                           GList   *outputs);

void       gf_gpu_take_crtcs              (GfGpu   *self,
                                           GList   *crtcs);

void       gf_gpu_take_modes              (GfGpu   *self,
                                           GList   *modes);

G_END_DECLS

#endif
