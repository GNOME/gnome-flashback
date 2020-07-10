/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
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
 *
 * Adapted from mutter:
 * - src/backends/meta-monitor-manager-private.h
 */

#ifndef GF_CRTC_PRIVATE_H
#define GF_CRTC_PRIVATE_H

#include <glib-object.h>
#include <stdint.h>

#include "gf-gpu-private.h"
#include "gf-monitor-manager-enums-private.h"
#include "gf-monitor-manager-types-private.h"
#include "gf-rectangle.h"

G_BEGIN_DECLS

typedef struct
{
  GfRectangle         layout;
  GfMonitorTransform  transform;

  GfCrtcMode         *mode;
} GfCrtcConfig;

struct _GfCrtc
{
  GObject         parent;

  gpointer        driver_private;
  GDestroyNotify  driver_notify;
};

struct _GfCrtcMode
{
  GObject         parent;

  /* The low-level ID of this mode, used to apply back configuration */
  glong           mode_id;
  gchar          *name;

  gint            width;
  gint            height;
  gfloat          refresh_rate;
  GfCrtcModeFlag  flags;

  gpointer        driver_private;
  GDestroyNotify  driver_notify;
};

typedef struct
{
  GfCrtc             *crtc;
  GfCrtcMode         *mode;
  GfRectangle         layout;
  GfMonitorTransform  transform;
  GPtrArray          *outputs;
} GfCrtcAssignment;

#define GF_TYPE_CRTC (gf_crtc_get_type ())
G_DECLARE_FINAL_TYPE (GfCrtc, gf_crtc, GF, CRTC, GObject)

#define GF_TYPE_CRTC_MODE (gf_crtc_mode_get_type ())
G_DECLARE_FINAL_TYPE (GfCrtcMode, gf_crtc_mode, GF, CRTC_MODE, GObject)

uint64_t            gf_crtc_get_id             (GfCrtc             *self);

GfGpu              *gf_crtc_get_gpu            (GfCrtc             *self);

GfMonitorTransform  gf_crtc_get_all_transforms (GfCrtc             *self);

void                gf_crtc_set_config         (GfCrtc             *self,
                                                GfRectangle        *layout,
                                                GfCrtcMode         *mode,
                                                GfMonitorTransform  transform);

void                gf_crtc_unset_config       (GfCrtc             *self);


const GfCrtcConfig *gf_crtc_get_config         (GfCrtc             *self);

G_END_DECLS

#endif
