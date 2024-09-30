/*
 * Copyright (C) 2017 Red Hat
 * Copyright (C) 2019 Alberts Muktupāvels
 * Copyright (C) 2020 NVIDIA CORPORATION
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

#ifndef GF_OUTPUT_XRANDR_PRIVATE_H
#define GF_OUTPUT_XRANDR_PRIVATE_H

#include <X11/extensions/Xrandr.h>

#include "gf-gpu-xrandr-private.h"
#include "gf-output-private.h"

G_BEGIN_DECLS

#define GF_TYPE_OUTPUT_XRANDR (gf_output_xrandr_get_type ())
G_DECLARE_FINAL_TYPE (GfOutputXrandr, gf_output_xrandr,
                      GF, OUTPUT_XRANDR, GfOutput)

GfOutputXrandr *gf_output_xrandr_new             (GfGpuXrandr    *gpu_xrandr,
                                                  XRROutputInfo  *xrandr_output,
                                                  RROutput        output_id,
                                                  RROutput        primary_output);

GBytes        *gf_output_xrandr_read_edid        (GfOutputXrandr *self);

void           gf_output_xrandr_apply_mode       (GfOutputXrandr *self);

void           gf_output_xrandr_set_ctm          (GfOutputXrandr    *self,
                                                  const GfOutputCtm *ctm);

G_END_DECLS

#endif
