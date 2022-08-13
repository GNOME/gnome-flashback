/*
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

#include "config.h"
#include "gf-monitor-transform.h"

static GfMonitorTransform
gf_monitor_transform_flip (GfMonitorTransform transform)
{
  switch (transform)
    {
      case GF_MONITOR_TRANSFORM_NORMAL:
        return GF_MONITOR_TRANSFORM_FLIPPED;
      case GF_MONITOR_TRANSFORM_90:
        return GF_MONITOR_TRANSFORM_FLIPPED_270;
      case GF_MONITOR_TRANSFORM_180:
        return GF_MONITOR_TRANSFORM_FLIPPED_180;
      case GF_MONITOR_TRANSFORM_270:
        return GF_MONITOR_TRANSFORM_FLIPPED_90;
      case GF_MONITOR_TRANSFORM_FLIPPED:
        return GF_MONITOR_TRANSFORM_NORMAL;
      case GF_MONITOR_TRANSFORM_FLIPPED_90:
        return GF_MONITOR_TRANSFORM_270;
      case GF_MONITOR_TRANSFORM_FLIPPED_180:
        return GF_MONITOR_TRANSFORM_180;
      case GF_MONITOR_TRANSFORM_FLIPPED_270:
        return GF_MONITOR_TRANSFORM_90;

      default:
        break;
    }

  g_assert_not_reached ();
  return 0;
}

GfMonitorTransform
gf_monitor_transform_from_orientation (GfOrientation orientation)
{
  switch (orientation)
    {
      case GF_ORIENTATION_BOTTOM_UP:
        return GF_MONITOR_TRANSFORM_180;

      case GF_ORIENTATION_LEFT_UP:
        return GF_MONITOR_TRANSFORM_90;

      case GF_ORIENTATION_RIGHT_UP:
        return GF_MONITOR_TRANSFORM_270;

      case GF_ORIENTATION_UNDEFINED:
      case GF_ORIENTATION_NORMAL:
      default:
        break;
    }

  return GF_MONITOR_TRANSFORM_NORMAL;
}

GfMonitorTransform
gf_monitor_transform_invert (GfMonitorTransform transform)
{
  GfMonitorTransform inverted_transform;

  switch (transform)
    {
      case GF_MONITOR_TRANSFORM_90:
        inverted_transform = GF_MONITOR_TRANSFORM_270;
        break;

      case GF_MONITOR_TRANSFORM_270:
        inverted_transform = GF_MONITOR_TRANSFORM_90;
        break;

      case GF_MONITOR_TRANSFORM_NORMAL:
      case GF_MONITOR_TRANSFORM_180:
      case GF_MONITOR_TRANSFORM_FLIPPED:
      case GF_MONITOR_TRANSFORM_FLIPPED_90:
      case GF_MONITOR_TRANSFORM_FLIPPED_180:
      case GF_MONITOR_TRANSFORM_FLIPPED_270:
        inverted_transform = transform;
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  return inverted_transform;
}

GfMonitorTransform
gf_monitor_transform_transform (GfMonitorTransform transform,
                                GfMonitorTransform other)
{
  GfMonitorTransform new_transform;
  gboolean needs_flip;

  if (gf_monitor_transform_is_flipped (other))
    new_transform = gf_monitor_transform_flip (transform);
  else
    new_transform = transform;

  needs_flip = FALSE;
  if (gf_monitor_transform_is_flipped (new_transform))
    needs_flip = TRUE;

  new_transform += other;
  new_transform %= GF_MONITOR_TRANSFORM_FLIPPED;

  if (needs_flip)
    new_transform += GF_MONITOR_TRANSFORM_FLIPPED;

  return new_transform;
}
