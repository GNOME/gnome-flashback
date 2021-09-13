/*
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2018 Robert Mader
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

#ifndef GF_MONITOR_TRANSFORM_H
#define GF_MONITOR_TRANSFORM_H

#include <glib-object.h>

#include "gf-orientation-manager-private.h"

G_BEGIN_DECLS

typedef enum
{
  GF_MONITOR_TRANSFORM_NORMAL,
  GF_MONITOR_TRANSFORM_90,
  GF_MONITOR_TRANSFORM_180,
  GF_MONITOR_TRANSFORM_270,
  GF_MONITOR_TRANSFORM_FLIPPED,
  GF_MONITOR_TRANSFORM_FLIPPED_90,
  GF_MONITOR_TRANSFORM_FLIPPED_180,
  GF_MONITOR_TRANSFORM_FLIPPED_270,
} GfMonitorTransform;

#define GF_MONITOR_N_TRANSFORMS (GF_MONITOR_TRANSFORM_FLIPPED_270 + 1)
#define GF_MONITOR_ALL_TRANSFORMS ((1 << GF_MONITOR_N_TRANSFORMS) - 1)

static inline gboolean
gf_monitor_transform_is_rotated (GfMonitorTransform transform)
{
  return (transform % 2);
}

static inline gboolean
gf_monitor_transform_is_flipped (GfMonitorTransform transform)
{
  return (transform >= GF_MONITOR_TRANSFORM_FLIPPED);
}

GfMonitorTransform gf_monitor_transform_from_orientation (GfOrientation      orientation);

GfMonitorTransform gf_monitor_transform_invert           (GfMonitorTransform transform);

GfMonitorTransform gf_monitor_transform_transform        (GfMonitorTransform transform,
                                                          GfMonitorTransform other);

G_END_DECLS

#endif
