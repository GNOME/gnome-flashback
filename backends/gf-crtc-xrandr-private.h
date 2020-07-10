/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013-2017 Red Hat Inc.
 * Copyright (C) 2017-2019 Alberts Muktupāvels
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

#ifndef GF_CRTC_XRANDR_PRIVATE_H
#define GF_CRTC_XRANDR_PRIVATE_H

#include <X11/extensions/Xrandr.h>
#include <xcb/randr.h>

#include "gf-crtc-private.h"
#include "gf-gpu-xrandr-private.h"

G_BEGIN_DECLS

#define GF_TYPE_CRTC_XRANDR (gf_crtc_xrandr_get_type ())
G_DECLARE_FINAL_TYPE (GfCrtcXrandr, gf_crtc_xrandr, GF, CRTC_XRANDR, GfCrtc)

GfCrtcXrandr *gf_crtc_xrandr_new                   (GfGpuXrandr          *gpu_xrandr,
                                                    XRRCrtcInfo          *xrandr_crtc,
                                                    RRCrtc                crtc_id,
                                                    XRRScreenResources   *resources);

gboolean      gf_crtc_xrandr_set_config            (GfCrtcXrandr         *self,
                                                    xcb_randr_crtc_t      xrandr_crtc,
                                                    xcb_timestamp_t       timestamp,
                                                    int                   x,
                                                    int                   y,
                                                    xcb_randr_mode_t      mode,
                                                    xcb_randr_rotation_t  rotation,
                                                    xcb_randr_output_t   *outputs,
                                                    int                   n_outputs,
                                                    xcb_timestamp_t      *out_timestamp);

gboolean      gf_crtc_xrandr_is_assignment_changed (GfCrtcXrandr         *self,
                                                    GfCrtcAssignment     *crtc_assignment);

GfCrtcMode   *gf_crtc_xrandr_get_current_mode      (GfCrtcXrandr         *self);

G_END_DECLS

#endif
