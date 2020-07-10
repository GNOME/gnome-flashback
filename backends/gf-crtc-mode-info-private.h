/*
 * Copyright (C) 2017-2020 Red Hat
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
 */

#ifndef GF_CRTC_MODE_INFO_PRIVATE_H
#define GF_CRTC_MODE_INFO_PRIVATE_H

#include <glib-object.h>

G_BEGIN_DECLS

/* Same as KMS mode flags and X11 randr flags */
typedef enum
{
  GF_CRTC_MODE_FLAG_NONE = 0,

  GF_CRTC_MODE_FLAG_PHSYNC = (1 << 0),
  GF_CRTC_MODE_FLAG_NHSYNC = (1 << 1),
  GF_CRTC_MODE_FLAG_PVSYNC = (1 << 2),
  GF_CRTC_MODE_FLAG_NVSYNC = (1 << 3),
  GF_CRTC_MODE_FLAG_INTERLACE = (1 << 4),
  GF_CRTC_MODE_FLAG_DBLSCAN = (1 << 5),
  GF_CRTC_MODE_FLAG_CSYNC = (1 << 6),
  GF_CRTC_MODE_FLAG_PCSYNC = (1 << 7),
  GF_CRTC_MODE_FLAG_NCSYNC = (1 << 8),
  GF_CRTC_MODE_FLAG_HSKEW = (1 << 9),
  GF_CRTC_MODE_FLAG_BCAST = (1 << 10),
  GF_CRTC_MODE_FLAG_PIXMUX = (1 << 11),
  GF_CRTC_MODE_FLAG_DBLCLK = (1 << 12),
  GF_CRTC_MODE_FLAG_CLKDIV2 = (1 << 13),

  GF_CRTC_MODE_FLAG_MASK = 0x3fff
} GfCrtcModeFlag;

typedef struct
{
  grefcount      ref_count;

  int            width;
  int            height;
  float          refresh_rate;
  GfCrtcModeFlag flags;
} GfCrtcModeInfo;

#define GF_TYPE_CRTC_MODE_INFO (gf_crtc_mode_info_get_type ())

GType           gf_crtc_mode_info_get_type (void);

GfCrtcModeInfo *gf_crtc_mode_info_new      (void);

GfCrtcModeInfo *gf_crtc_mode_info_ref      (GfCrtcModeInfo *self);

void            gf_crtc_mode_info_unref    (GfCrtcModeInfo *self);

G_END_DECLS

#endif
