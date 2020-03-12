/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
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

#include "config.h"
#include "gf-crtc-xrandr-private.h"

#include <X11/Xlib-xcb.h>

#include "gf-output-private.h"

#define ALL_ROTATIONS (RR_Rotate_0 | RR_Rotate_90 | RR_Rotate_180 | RR_Rotate_270)
#define ALL_TRANSFORMS ((1 << (GF_MONITOR_TRANSFORM_FLIPPED_270 + 1)) - 1)

typedef struct
{
  GfRectangle         rect;
  GfMonitorTransform  transform;

  GfCrtcMode         *current_mode;
} GfCrtcXrandr;

static void
gf_crtc_destroy_notify (GfCrtc *crtc)
{
  g_free (crtc->driver_private);
}

static GfMonitorTransform
gf_monitor_transform_from_xrandr (Rotation rotation)
{
  static const GfMonitorTransform y_reflected_map[4] = {
    GF_MONITOR_TRANSFORM_FLIPPED_180,
    GF_MONITOR_TRANSFORM_FLIPPED_90,
    GF_MONITOR_TRANSFORM_FLIPPED,
    GF_MONITOR_TRANSFORM_FLIPPED_270
  };
  GfMonitorTransform ret;

  switch (rotation & 0x7F)
    {
      default:
      case RR_Rotate_0:
        ret = GF_MONITOR_TRANSFORM_NORMAL;
        break;

      case RR_Rotate_90:
        ret = GF_MONITOR_TRANSFORM_90;
        break;

      case RR_Rotate_180:
        ret = GF_MONITOR_TRANSFORM_180;
        break;

      case RR_Rotate_270:
        ret = GF_MONITOR_TRANSFORM_270;
        break;
    }

  if (rotation & RR_Reflect_X)
    return ret + 4;
  else if (rotation & RR_Reflect_Y)
    return y_reflected_map[ret];
  else
    return ret;
}

static GfMonitorTransform
gf_monitor_transform_from_xrandr_all (Rotation rotation)
{
  GfMonitorTransform ret;

  /* Handle the common cases first (none or all) */
  if (rotation == 0 || rotation == RR_Rotate_0)
    return (1 << GF_MONITOR_TRANSFORM_NORMAL);

  /* All rotations and one reflection -> all of them by composition */
  if ((rotation & ALL_ROTATIONS) &&
      ((rotation & RR_Reflect_X) || (rotation & RR_Reflect_Y)))
    return ALL_TRANSFORMS;

  ret = 1 << GF_MONITOR_TRANSFORM_NORMAL;
  if (rotation & RR_Rotate_90)
    ret |= 1 << GF_MONITOR_TRANSFORM_90;
  if (rotation & RR_Rotate_180)
    ret |= 1 << GF_MONITOR_TRANSFORM_180;
  if (rotation & RR_Rotate_270)
    ret |= 1 << GF_MONITOR_TRANSFORM_270;
  if (rotation & (RR_Rotate_0 | RR_Reflect_X))
    ret |= 1 << GF_MONITOR_TRANSFORM_FLIPPED;
  if (rotation & (RR_Rotate_90 | RR_Reflect_X))
    ret |= 1 << GF_MONITOR_TRANSFORM_FLIPPED_90;
  if (rotation & (RR_Rotate_180 | RR_Reflect_X))
    ret |= 1 << GF_MONITOR_TRANSFORM_FLIPPED_180;
  if (rotation & (RR_Rotate_270 | RR_Reflect_X))
    ret |= 1 << GF_MONITOR_TRANSFORM_FLIPPED_270;

  return ret;
}

GfCrtc *
gf_create_xrandr_crtc (GfGpuXrandr        *gpu_xrandr,
                       XRRCrtcInfo        *xrandr_crtc,
                       RRCrtc              crtc_id,
                       XRRScreenResources *resources)

{
  GfCrtc *crtc;
  GfCrtcXrandr *crtc_xrandr;
  unsigned int i;
  GList *modes;

  crtc = g_object_new (GF_TYPE_CRTC, NULL);

  crtc_xrandr = g_new0 (GfCrtcXrandr, 1);
  crtc_xrandr->rect = (GfRectangle) {
    .x = xrandr_crtc->x,
    .y = xrandr_crtc->y,
    .width = xrandr_crtc->width,
    .height = xrandr_crtc->height,
  };
  crtc_xrandr->transform = gf_monitor_transform_from_xrandr (xrandr_crtc->rotation);

  crtc->driver_private = crtc_xrandr;
  crtc->driver_notify = (GDestroyNotify) gf_crtc_destroy_notify;

  crtc->gpu = GF_GPU (gpu_xrandr);
  crtc->crtc_id = crtc_id;
  crtc->is_dirty = FALSE;
  crtc->all_transforms = gf_monitor_transform_from_xrandr_all (xrandr_crtc->rotations);

  modes = gf_gpu_get_modes (crtc->gpu);
  for (i = 0; i < (guint) resources->nmode; i++)
    {
      if (resources->modes[i].id == xrandr_crtc->mode)
        {
          crtc_xrandr->current_mode = g_list_nth_data (modes, i);
          break;
        }
    }

  if (crtc_xrandr->current_mode)
    {
      gf_crtc_set_config (crtc,
                          &crtc_xrandr->rect,
                          crtc_xrandr->current_mode,
                          crtc_xrandr->transform);
    }

  return crtc;
}

gboolean
gf_crtc_xrandr_set_config (GfCrtc               *crtc,
                           xcb_randr_crtc_t      xrandr_crtc,
                           xcb_timestamp_t       timestamp,
                           int                   x,
                           int                   y,
                           xcb_randr_mode_t      mode,
                           xcb_randr_rotation_t  rotation,
                           xcb_randr_output_t   *outputs,
                           int                   n_outputs,
                           xcb_timestamp_t      *out_timestamp)
{
  GfGpu *gpu;
  GfGpuXrandr *gpu_xrandr;
  GfMonitorManager *monitor_manager;
  GfMonitorManagerXrandr *monitor_manager_xrandr;
  Display *xdisplay;
  XRRScreenResources *resources;
  xcb_connection_t *xcb_conn;
  xcb_timestamp_t config_timestamp;
  xcb_randr_set_crtc_config_cookie_t cookie;
  xcb_randr_set_crtc_config_reply_t *reply;
  xcb_generic_error_t *xcb_error;

  gpu = gf_crtc_get_gpu (crtc);
  gpu_xrandr = GF_GPU_XRANDR (gpu);

  monitor_manager = gf_gpu_get_monitor_manager (gpu);
  monitor_manager_xrandr = GF_MONITOR_MANAGER_XRANDR (monitor_manager);

  xdisplay = gf_monitor_manager_xrandr_get_xdisplay (monitor_manager_xrandr);
  resources = gf_gpu_xrandr_get_resources (gpu_xrandr);
  xcb_conn = XGetXCBConnection (xdisplay);

  config_timestamp = resources->configTimestamp;
  cookie = xcb_randr_set_crtc_config (xcb_conn, xrandr_crtc,
                                      timestamp, config_timestamp,
                                      x, y, mode, rotation,
                                      n_outputs, outputs);

  xcb_error = NULL;
  reply = xcb_randr_set_crtc_config_reply (xcb_conn, cookie, &xcb_error);
  if (xcb_error || !reply)
    {
      g_free (xcb_error);
      g_free (reply);

      return FALSE;
    }

  *out_timestamp = reply->timestamp;
  g_free (reply);

  return TRUE;
}

gboolean
gf_crtc_xrandr_is_assignment_changed (GfCrtc     *crtc,
                                      GfCrtcInfo *crtc_info)
{
  GfCrtcXrandr *crtc_xrandr;
  unsigned int i;

  crtc_xrandr = crtc->driver_private;

  if (crtc_xrandr->current_mode != crtc_info->mode)
    return TRUE;

  if (crtc_xrandr->rect.x != crtc_info->layout.x)
    return TRUE;

  if (crtc_xrandr->rect.y != crtc_info->layout.y)
    return TRUE;

  if (crtc_xrandr->transform != crtc_info->transform)
    return TRUE;

  for (i = 0; i < crtc_info->outputs->len; i++)
    {
      GfOutput *output;
      GfCrtc *assigned_crtc;

      output = ((GfOutput **) crtc_info->outputs->pdata)[i];
      assigned_crtc = gf_output_get_assigned_crtc (output);

      if (assigned_crtc != crtc)
        return TRUE;
    }

  return FALSE;
}

GfCrtcMode *
gf_crtc_xrandr_get_current_mode (GfCrtc *crtc)
{
  GfCrtcXrandr *crtc_xrandr;

  crtc_xrandr = crtc->driver_private;

  return crtc_xrandr->current_mode;
}
