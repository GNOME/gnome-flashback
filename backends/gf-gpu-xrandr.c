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

#include <gio/gio.h>
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
  GfOutput *o_one;
  GfOutput *o_two;
  const GfOutputInfo *output_info_one;
  const GfOutputInfo *output_info_two;

  o_one = (GfOutput *) one;
  o_two = (GfOutput *) two;

  output_info_one = gf_output_get_info (o_one);
  output_info_two = gf_output_get_info (o_two);

  return strcmp (output_info_one->name, output_info_two->name);
}

static char *
get_xmode_name (XRRModeInfo *xmode)
{
  int width = xmode->width;
  int height = xmode->height;

  return g_strdup_printf ("%dx%d", width, height);
}

static float
calculate_refresh_rate (XRRModeInfo *xmode)
{
  float h_total;
  float v_total;

  h_total = (float) xmode->hTotal;
  v_total = (float) xmode->vTotal;

  if (h_total == 0.0f || v_total == 0.0f)
    return 0.0;

  if (xmode->modeFlags & RR_DoubleScan)
    v_total *= 2.0f;

  if (xmode->modeFlags & RR_Interlace)
    v_total /= 2.0f;

  return xmode->dotClock / (h_total * v_total);
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
  GfBackend *backend;
  GfMonitorManager *monitor_manager;
  GfMonitorManagerXrandr *monitor_manager_xrandr;
  Display *xdisplay;
  XRRScreenResources *resources;
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

  backend = gf_gpu_get_backend (gpu);
  monitor_manager = gf_backend_get_monitor_manager (backend);
  monitor_manager_xrandr = GF_MONITOR_MANAGER_XRANDR (monitor_manager);
  xdisplay = gf_monitor_manager_xrandr_get_xdisplay (monitor_manager_xrandr);

  g_clear_pointer (&gpu_xrandr->resources, XRRFreeScreenResources);

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
      GfCrtcModeInfo *crtc_mode_info;
      char *crtc_mode_name;
      GfCrtcMode *mode;

      xmode = &resources->modes[i];

      crtc_mode_info = gf_crtc_mode_info_new ();
      crtc_mode_name = get_xmode_name (xmode);

      crtc_mode_info->width = xmode->width;
      crtc_mode_info->height = xmode->height;
      crtc_mode_info->refresh_rate = calculate_refresh_rate (xmode);
      crtc_mode_info->flags = xmode->modeFlags;

      mode = g_object_new (GF_TYPE_CRTC_MODE,
                           "id", (uint64_t) xmode->id,
                           "name", crtc_mode_name,
                           "info", crtc_mode_info,
                           NULL);

      modes = g_list_append (modes, mode);

      gf_crtc_mode_info_unref (crtc_mode_info);
      g_free (crtc_mode_name);
    }

  gf_gpu_take_modes (gpu, modes);

  for (i = 0; i < (guint) resources->ncrtc; i++)
    {
      XRRCrtcInfo *xrandr_crtc;
      RRCrtc crtc_id;
      GfCrtcXrandr *crtc_xrandr;

      crtc_id = resources->crtcs[i];
      xrandr_crtc = XRRGetCrtcInfo (xdisplay, resources, crtc_id);
      crtc_xrandr = gf_crtc_xrandr_new (gpu_xrandr, xrandr_crtc, crtc_id, resources);

      crtcs = g_list_append (crtcs, crtc_xrandr);
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
          GfOutputXrandr *output_xrandr;

          output_xrandr = gf_output_xrandr_new (gpu_xrandr,
                                                xrandr_output,
                                                output_id,
                                                primary_output);

          if (output_xrandr)
            outputs = g_list_prepend (outputs, output_xrandr);
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
      const GfOutputInfo *output_info;
      GList *k;

      output = l->data;
      output_info = gf_output_get_info (output);

      for (j = 0; j < output_info->n_possible_clones; j++)
        {
          RROutput clone = GPOINTER_TO_INT (output_info->possible_clones[j]);

          for (k = outputs; k; k = k->next)
            {
              GfOutput *possible_clone = k->data;

              if (clone == (XID) gf_output_get_id (possible_clone))
                {
                  output_info->possible_clones[j] = possible_clone;
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
gf_gpu_xrandr_new (GfBackendX11 *backend_x11)
{
  return g_object_new (GF_TYPE_GPU_XRANDR,
                       "backend", backend_x11,
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
