/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013-2017 Red Hat Inc.
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

#include "config.h"
#include "gf-gpu-xrandr-private.h"

#include <X11/extensions/dpms.h>
#include <X11/Xlibint.h>

#include "gf-crtc-xrandr-private.h"
#include "gf-monitor-manager-xrandr-private.h"
#include "gf-output-private.h"
#include "gf-output-xrandr-private.h"

struct _GfGpuXrandr
{
  GfGpu               parent;

  XRRScreenResources *resources;

  int                 max_screen_width;
  int                 max_screen_height;
};

G_DEFINE_TYPE (GfGpuXrandr, gf_gpu_xrandr, GF_TYPE_GPU)

static gint
compare_outputs (const void *one,
                 const void *two)
{
  const GfOutput *o_one = one, *o_two = two;

  return strcmp (o_one->name, o_two->name);
}

static char *
get_xmode_name (XRRModeInfo *xmode)
{
  int width = xmode->width;
  int height = xmode->height;

  return g_strdup_printf ("%dx%d", width, height);
}

static void
gf_gpu_xrandr_finalize (GObject *object)
{
  GfGpuXrandr *gpu_xrandr;

  gpu_xrandr = GF_GPU_XRANDR (object);

  g_clear_pointer (&gpu_xrandr->resources, XRRFreeScreenResources);

  G_OBJECT_CLASS (gf_gpu_xrandr_parent_class)->finalize (object);
}

static gboolean
gf_gpu_xrandr_read_current (GfGpu   *gpu,
                            GError **error)
{
  GfGpuXrandr *gpu_xrandr;
  GfMonitorManager *monitor_manager;
  GfMonitorManagerXrandr *monitor_manager_xrandr;
  Display *xdisplay;
  XRRScreenResources *resources;
  CARD16 dpms_state;
  BOOL dpms_enabled;
  gint min_width;
  gint min_height;
  Screen *screen;
  GList *outputs;
  GList *modes;
  GList *crtcs;
  guint i, j;
  GList *l;
  RROutput primary_output;

  gpu_xrandr = GF_GPU_XRANDR (gpu);
  monitor_manager = gf_gpu_get_monitor_manager (gpu);
  monitor_manager_xrandr = GF_MONITOR_MANAGER_XRANDR (monitor_manager);
  xdisplay = gf_monitor_manager_xrandr_get_xdisplay (monitor_manager_xrandr);

  g_clear_pointer (&gpu_xrandr->resources, XRRFreeScreenResources);

  if (DPMSCapable (xdisplay) &&
      DPMSInfo (xdisplay, &dpms_state, &dpms_enabled) &&
      dpms_enabled)
    {
      switch (dpms_state)
        {
          case DPMSModeOn:
            monitor_manager->power_save_mode = GF_POWER_SAVE_ON;
            break;

          case DPMSModeStandby:
            monitor_manager->power_save_mode = GF_POWER_SAVE_STANDBY;
            break;

          case DPMSModeSuspend:
            monitor_manager->power_save_mode = GF_POWER_SAVE_SUSPEND;
            break;

          case DPMSModeOff:
            monitor_manager->power_save_mode = GF_POWER_SAVE_OFF;
            break;

          default:
            monitor_manager->power_save_mode = GF_POWER_SAVE_UNSUPPORTED;
            break;
        }
    }
  else
    {
      monitor_manager->power_save_mode = GF_POWER_SAVE_UNSUPPORTED;
    }

  XRRGetScreenSizeRange (xdisplay, DefaultRootWindow (xdisplay),
                         &min_width, &min_height,
                         &gpu_xrandr->max_screen_width,
                         &gpu_xrandr->max_screen_height);

  screen = ScreenOfDisplay (xdisplay, DefaultScreen (xdisplay));
  /* This is updated because we called RRUpdateConfiguration below */
  monitor_manager->screen_width = WidthOfScreen (screen);
  monitor_manager->screen_height = HeightOfScreen (screen);

  resources = XRRGetScreenResourcesCurrent (xdisplay, DefaultRootWindow (xdisplay));

  if (!resources)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to retrieve Xrandr screen resources");
      return FALSE;
    }

  gpu_xrandr->resources = resources;

  outputs = NULL;
  modes = NULL;
  crtcs = NULL;

  for (i = 0; i < (guint) resources->nmode; i++)
    {
      XRRModeInfo *xmode;
      GfCrtcMode *mode;

      xmode = &resources->modes[i];
      mode = g_object_new (GF_TYPE_CRTC_MODE, NULL);

      mode->mode_id = xmode->id;
      mode->width = xmode->width;
      mode->height = xmode->height;
      mode->refresh_rate = (xmode->dotClock / ((gfloat) xmode->hTotal * xmode->vTotal));
      mode->flags = xmode->modeFlags;
      mode->name = get_xmode_name (xmode);

      modes = g_list_append (modes, mode);
    }

  gf_gpu_take_modes (gpu, modes);

  for (i = 0; i < (guint) resources->ncrtc; i++)
    {
      XRRCrtcInfo *xrandr_crtc;
      RRCrtc crtc_id;
      GfCrtc *crtc;

      crtc_id = resources->crtcs[i];
      xrandr_crtc = XRRGetCrtcInfo (xdisplay, resources, crtc_id);
      crtc = gf_create_xrandr_crtc (gpu_xrandr, xrandr_crtc, crtc_id, resources);

      crtcs = g_list_append (crtcs, crtc);
      XRRFreeCrtcInfo (xrandr_crtc);
    }

  gf_gpu_take_crtcs (gpu, crtcs);

  primary_output = XRRGetOutputPrimary (xdisplay, DefaultRootWindow (xdisplay));

  for (i = 0; i < (guint) resources->noutput; i++)
    {
      RROutput output_id;
      XRROutputInfo *xrandr_output;

      output_id = resources->outputs[i];
      xrandr_output = XRRGetOutputInfo (xdisplay, resources, output_id);

      if (!xrandr_output)
        continue;

      if (xrandr_output->connection != RR_Disconnected)
        {
          GfOutput *output;

          output = gf_create_xrandr_output (gpu_xrandr,
                                            xrandr_output,
                                            output_id,
                                            primary_output);

          if (output)
            outputs = g_list_prepend (outputs, output);
        }

      XRRFreeOutputInfo (xrandr_output);
    }

  /* Sort the outputs for easier handling in GfMonitorConfig */
  outputs = g_list_sort (outputs, compare_outputs);

  gf_gpu_take_outputs (gpu, outputs);

  /* Now fix the clones */
  for (l = outputs; l; l = l->next)
    {
      GfOutput *output;
      GList *k;

      output = l->data;

      for (j = 0; j < output->n_possible_clones; j++)
        {
          RROutput clone = GPOINTER_TO_INT (output->possible_clones[j]);

          for (k = outputs; k; k = k->next)
            {
              GfOutput *possible_clone = k->data;

              if (clone == (XID) possible_clone->winsys_id)
                {
                  output->possible_clones[j] = possible_clone;
                  break;
                }
            }
        }
    }

  return TRUE;
}

static void
gf_gpu_xrandr_class_init (GfGpuXrandrClass *gpu_xrandr_class)
{
  GObjectClass *object_class;
  GfGpuClass *gpu_class;

  object_class = G_OBJECT_CLASS (gpu_xrandr_class);
  gpu_class = GF_GPU_CLASS (gpu_xrandr_class);

  object_class->finalize = gf_gpu_xrandr_finalize;

  gpu_class->read_current = gf_gpu_xrandr_read_current;
}

static void
gf_gpu_xrandr_init (GfGpuXrandr *gpu_xrandr)
{
}

GfGpuXrandr *
gf_gpu_xrandr_new (GfMonitorManagerXrandr *monitor_manager_xrandr)
{
  return g_object_new (GF_TYPE_GPU_XRANDR,
                       "monitor-manager", monitor_manager_xrandr,
                       NULL);
}

XRRScreenResources *
gf_gpu_xrandr_get_resources (GfGpuXrandr *gpu_xrandr)
{
  return gpu_xrandr->resources;
}

void
gf_gpu_xrandr_get_max_screen_size (GfGpuXrandr *gpu_xrandr,
                                   int         *max_width,
                                   int         *max_height)
{
  *max_width = gpu_xrandr->max_screen_width;
  *max_height = gpu_xrandr->max_screen_height;
}
