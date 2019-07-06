/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2017 Alberts Muktupāvels
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
 * - src/backends/x11/meta-monitor-manager-xrandr.c
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/dpms.h>
#include <X11/Xlib-xcb.h>
#include <xcb/randr.h>

#include "gf-backend-x11-private.h"
#include "gf-crtc-xrandr-private.h"
#include "gf-monitor-config-manager-private.h"
#include "gf-monitor-manager-xrandr-private.h"
#include "gf-monitor-private.h"
#include "gf-monitor-tiled-private.h"
#include "gf-output-private.h"

/* Look for DPI_FALLBACK in:
 * http://git.gnome.org/browse/gnome-settings-daemon/tree/plugins/xsettings/gsd-xsettings-manager.c
 * for the reasoning
 */
#define DPI_FALLBACK 96.0

struct _GfMonitorManagerXrandr
{
  GfMonitorManager    parent;

  Display            *xdisplay;
  Window              xroot;

  gint                rr_event_base;
  gint                rr_error_base;

  gboolean            has_randr15;
  GHashTable         *tiled_monitor_atoms;

  XRRScreenResources *resources;

  Time                last_xrandr_set_timestamp;

  gint                max_screen_width;
  gint                max_screen_height;

  gfloat             *supported_scales;
  gint                n_supported_scales;
};

typedef struct
{
  Atom xrandr_name;
} GfMonitorData;

G_DEFINE_TYPE (GfMonitorManagerXrandr, gf_monitor_manager_xrandr, GF_TYPE_MONITOR_MANAGER)

static void
add_supported_scale (GArray *supported_scales,
                     gfloat  scale)
{
  guint i;

  for (i = 0; i < supported_scales->len; i++)
    {
      gfloat supported_scale;

      supported_scale = g_array_index (supported_scales, gfloat, i);

      if (scale == supported_scale)
        return;
    }

  g_array_append_val (supported_scales, scale);
}

static gint
compare_scales (gconstpointer a,
                gconstpointer b)
{
  gfloat f = *(gfloat *) a - *(gfloat *) b;

  if (f < 0)
    return -1;

  if (f > 0)
    return 1;

  return 0;
}

static void
ensure_supported_monitor_scales (GfMonitorManager *manager)
{
  GfMonitorManagerXrandr *xrandr;
  GfMonitorScalesConstraint constraints;
  GArray *supported_scales;
  GList *l;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  if (xrandr->supported_scales)
    return;

  constraints = GF_MONITOR_SCALES_CONSTRAINT_NO_FRAC;
  supported_scales = g_array_new (FALSE, FALSE, sizeof (gfloat));

  for (l = manager->monitors; l; l = l->next)
    {
      GfMonitor *monitor;
      GfMonitorMode *monitor_mode;
      gfloat *monitor_scales;
      gint n_monitor_scales;
      gint i;

      monitor = l->data;
      monitor_mode = gf_monitor_get_preferred_mode (monitor);
      monitor_scales = gf_monitor_calculate_supported_scales (monitor,
                                                              monitor_mode,
                                                              constraints,
                                                              &n_monitor_scales);

      for (i = 0; i < n_monitor_scales; i++)
        {
          add_supported_scale (supported_scales, monitor_scales[i]);
        }

      g_array_sort (supported_scales, compare_scales);
      g_free (monitor_scales);
    }

  xrandr->supported_scales = (gfloat *) supported_scales->data;
  xrandr->n_supported_scales = supported_scales->len;
  g_array_free (supported_scales, FALSE);
}

static void
gf_monitor_manager_xrandr_rebuild_derived (GfMonitorManager *manager,
                                           GfMonitorsConfig *config)
{
  GfMonitorManagerXrandr *xrandr;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  g_clear_pointer (&xrandr->supported_scales, g_free);
  gf_monitor_manager_rebuild_derived (manager, config);
}

static gboolean
xrandr_set_crtc_config (GfMonitorManagerXrandr *xrandr,
                        GfCrtc                 *crtc,
                        gboolean                save_timestamp,
                        xcb_randr_crtc_t        xrandr_crtc,
                        xcb_timestamp_t         timestamp,
                        gint                    x,
                        gint                    y,
                        xcb_randr_mode_t        mode,
                        xcb_randr_rotation_t    rotation,
                        xcb_randr_output_t     *outputs,
                        gint                    n_outputs)
{
  xcb_timestamp_t new_timestamp;

  if (!gf_crtc_xrandr_set_config (crtc, xrandr_crtc, timestamp,
                                  x, y, mode, rotation,
                                  outputs, n_outputs,
                                  &new_timestamp))
    return FALSE;

  if (save_timestamp)
    xrandr->last_xrandr_set_timestamp = new_timestamp;

  return TRUE;
}

static void
output_set_presentation_xrandr (GfMonitorManagerXrandr *xrandr,
                                GfOutput               *output,
                                gboolean                presentation)
{
  Atom atom;
  gint value;

  atom = XInternAtom (xrandr->xdisplay, "_GNOME_FLASHBACK_PRESENTATION_OUTPUT", False);
  value= presentation;

  xcb_randr_change_output_property (XGetXCBConnection (xrandr->xdisplay),
                                    (XID) output->winsys_id,
                                    atom, XCB_ATOM_CARDINAL, 32,
                                    XCB_PROP_MODE_REPLACE,
                                    1, &value);
}

static void
output_set_underscanning_xrandr (GfMonitorManagerXrandr *xrandr,
                                 GfOutput               *output,
                                 gboolean                underscanning)
{
  Atom prop, valueatom;
  const gchar *value;

  prop = XInternAtom (xrandr->xdisplay, "underscan", False);

  value = underscanning ? "on" : "off";
  valueatom = XInternAtom (xrandr->xdisplay, value, False);

  xcb_randr_change_output_property (XGetXCBConnection (xrandr->xdisplay),
                                    (XID) output->winsys_id,
                                    prop, XCB_ATOM_ATOM, 32,
                                    XCB_PROP_MODE_REPLACE,
                                    1, &valueatom);

  /* Configure the border at the same time. Currently, we use a
   * 5% of the width/height of the mode. In the future, we should
   * make the border configurable.
   */
  if (underscanning)
    {
      uint32_t border_value;

      prop = XInternAtom (xrandr->xdisplay, "underscan hborder", False);
      border_value = output->crtc->current_mode->width * 0.05;

      xcb_randr_change_output_property (XGetXCBConnection (xrandr->xdisplay),
                                        (XID) output->winsys_id,
                                        prop, XCB_ATOM_INTEGER, 32,
                                        XCB_PROP_MODE_REPLACE,
                                        1, &border_value);

      prop = XInternAtom (xrandr->xdisplay, "underscan vborder", False);
      border_value = output->crtc->current_mode->height * 0.05;

      xcb_randr_change_output_property (XGetXCBConnection (xrandr->xdisplay),
                                        (XID) output->winsys_id,
                                        prop, XCB_ATOM_INTEGER, 32,
                                        XCB_PROP_MODE_REPLACE,
                                        1, &border_value);
    }
}

static gboolean
is_crtc_assignment_changed (GfCrtc      *crtc,
                            GfCrtcInfo **crtc_infos,
                            guint        n_crtc_infos)
{
  guint i;

  for (i = 0; i < n_crtc_infos; i++)
    {
      GfCrtcInfo *crtc_info = crtc_infos[i];
      guint j;

      if (crtc_info->crtc != crtc)
        continue;

      if (crtc->current_mode != crtc_info->mode)
        return TRUE;

      if (crtc->rect.x != crtc_info->x)
        return TRUE;

      if (crtc->rect.y != crtc_info->y)
        return TRUE;

      if (crtc->transform != crtc_info->transform)
        return TRUE;

      for (j = 0; j < crtc_info->outputs->len; j++)
        {
          GfOutput *output = ((GfOutput**) crtc_info->outputs->pdata)[j];

          if (output->crtc != crtc)
            return TRUE;
        }

      return FALSE;
    }

  return crtc->current_mode != NULL;
}

static gboolean
is_output_assignment_changed (GfOutput      *output,
                              GfCrtcInfo   **crtc_infos,
                              guint          n_crtc_infos,
                              GfOutputInfo **output_infos,
                              guint          n_output_infos)
{
  gboolean output_is_found = FALSE;
  guint i;

  for (i = 0; i < n_output_infos; i++)
    {
      GfOutputInfo *output_info = output_infos[i];

      if (output_info->output != output)
        continue;

      if (output->is_primary != output_info->is_primary)
        return TRUE;

      if (output->is_presentation != output_info->is_presentation)
        return TRUE;

      if (output->is_underscanning != output_info->is_underscanning)
        return TRUE;

      output_is_found = TRUE;
    }

  if (!output_is_found)
    return output->crtc != NULL;

  for (i = 0; i < n_crtc_infos; i++)
    {
      GfCrtcInfo *crtc_info = crtc_infos[i];
      guint j;

      for (j = 0; j < crtc_info->outputs->len; j++)
        {
          GfOutput *crtc_info_output;

          crtc_info_output = ((GfOutput**) crtc_info->outputs->pdata)[j];

          if (crtc_info_output == output &&
              crtc_info->crtc == output->crtc)
            return FALSE;
        }
    }

  return TRUE;
}

static gboolean
is_assignments_changed (GfMonitorManager  *manager,
                        GfCrtcInfo       **crtc_infos,
                        guint              n_crtc_infos,
                        GfOutputInfo     **output_infos,
                        guint              n_output_infos)
{
  GList *l;

  for (l = manager->crtcs; l; l = l->next)
    {
      GfCrtc *crtc = l->data;

      if (is_crtc_assignment_changed (crtc, crtc_infos, n_crtc_infos))
        return TRUE;
    }

  for (l = manager->outputs; l; l = l->next)
    {
      GfOutput *output = l->data;

      if (is_output_assignment_changed (output,
                                        crtc_infos,
                                        n_crtc_infos,
                                        output_infos,
                                        n_output_infos))
        return TRUE;
    }

  return FALSE;
}

static guint8 *
get_edid_property (Display  *xdisplay,
                   RROutput  output,
                   Atom      atom,
                   gsize    *len)
{
  guchar *prop;
  gint actual_format;
  gulong nitems, bytes_after;
  Atom actual_type;
  guint8 *result;

  XRRGetOutputProperty (xdisplay, output, atom,
                        0, 100, False, False,
                        AnyPropertyType,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &prop);

  if (actual_type == XA_INTEGER && actual_format == 8)
    {
      result = g_memdup (prop, nitems);
      if (len)
        *len = nitems;
    }
  else
    {
      result = NULL;
    }

  if (prop)
    XFree (prop);

  return result;
}

static GBytes *
read_output_edid (GfMonitorManagerXrandr *xrandr,
                  XID                     winsys_id)
{
  Atom edid_atom;
  guint8 *result;
  gsize len;

  edid_atom = XInternAtom (xrandr->xdisplay, "EDID", FALSE);
  result = get_edid_property (xrandr->xdisplay, winsys_id, edid_atom, &len);

  if (!result)
    {
      edid_atom = XInternAtom (xrandr->xdisplay, "EDID_DATA", FALSE);
      result = get_edid_property (xrandr->xdisplay, winsys_id, edid_atom, &len);
    }

  if (result)
    {
      if (len > 0 && len % 128 == 0)
        return g_bytes_new_take (result, len);
      else
        g_free (result);
    }

  return NULL;
}

static xcb_randr_rotation_t
gf_monitor_transform_to_xrandr (GfMonitorTransform transform)
{
  xcb_randr_rotation_t rotation;

  rotation = XCB_RANDR_ROTATION_ROTATE_0;

  switch (transform)
    {
      case GF_MONITOR_TRANSFORM_NORMAL:
        rotation = XCB_RANDR_ROTATION_ROTATE_0;
        break;

      case GF_MONITOR_TRANSFORM_90:
        rotation = XCB_RANDR_ROTATION_ROTATE_90;
        break;

      case GF_MONITOR_TRANSFORM_180:
        rotation = XCB_RANDR_ROTATION_ROTATE_180;
        break;

      case GF_MONITOR_TRANSFORM_270:
        rotation = XCB_RANDR_ROTATION_ROTATE_270;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED:
        rotation = XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_ROTATE_0;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED_90:
        rotation = XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_ROTATE_90;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED_180:
        rotation = XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_ROTATE_180;
        break;

      case GF_MONITOR_TRANSFORM_FLIPPED_270:
        rotation = XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_ROTATE_270;
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  return rotation;
}

static gboolean
output_get_property_exists (GfMonitorManagerXrandr *xrandr,
                            GfOutput               *output,
                            const gchar            *propname)
{
  gboolean exists;
  Atom atom, actual_type;
  gint actual_format;
  gulong nitems, bytes_after;
  guchar *buffer;

  atom = XInternAtom (xrandr->xdisplay, propname, False);
  XRRGetOutputProperty (xrandr->xdisplay, (XID) output->winsys_id, atom,
                        0, G_MAXLONG, False, False, AnyPropertyType,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  exists = (actual_type != None);

  if (buffer)
    XFree (buffer);

  return exists;
}

static gboolean
output_get_hotplug_mode_update (GfMonitorManagerXrandr *xrandr,
                                GfOutput               *output)
{
  return output_get_property_exists (xrandr, output, "hotplug_mode_update");
}

static gboolean
output_get_integer_property (GfMonitorManagerXrandr *xrandr,
                             GfOutput               *output,
                             const gchar            *propname,
                             gint                   *value)
{
  gboolean exists;
  Atom atom, actual_type;
  gint actual_format;
  gulong nitems, bytes_after;
  guchar *buffer;

  atom = XInternAtom (xrandr->xdisplay, propname, False);
  XRRGetOutputProperty (xrandr->xdisplay, (XID) output->winsys_id, atom,
                        0, G_MAXLONG, False, False, XA_INTEGER,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  exists = (actual_type == XA_INTEGER && actual_format == 32 && nitems == 1);

  if (exists && value != NULL)
    *value = ((gint*) buffer)[0];

  if (buffer)
    XFree (buffer);

  return exists;
}

static gint
output_get_suggested_x (GfMonitorManagerXrandr *xrandr,
                        GfOutput               *output)
{
  gint val;

  if (output_get_integer_property (xrandr, output, "suggested X", &val))
    return val;

  return -1;
}

static gint
output_get_suggested_y (GfMonitorManagerXrandr *xrandr,
                        GfOutput               *output)
{
  gint val;

  if (output_get_integer_property (xrandr, output, "suggested Y", &val))
    return val;

  return -1;
}

static GfConnectorType
connector_type_from_atom (GfMonitorManagerXrandr *xrandr,
                          Atom                    atom)
{
  Display *xdisplay;

  xdisplay = xrandr->xdisplay;

  if (atom == XInternAtom (xdisplay, "HDMI", True))
    return GF_CONNECTOR_TYPE_HDMIA;
  if (atom == XInternAtom (xdisplay, "VGA", True))
    return GF_CONNECTOR_TYPE_VGA;
  /* Doesn't have a DRM equivalent, but means an internal panel.
   * We could pick either LVDS or eDP here. */
  if (atom == XInternAtom (xdisplay, "Panel", True))
    return GF_CONNECTOR_TYPE_LVDS;
  if (atom == XInternAtom (xdisplay, "DVI", True) ||
      atom == XInternAtom (xdisplay, "DVI-I", True))
    return GF_CONNECTOR_TYPE_DVII;
  if (atom == XInternAtom (xdisplay, "DVI-A", True))
    return GF_CONNECTOR_TYPE_DVIA;
  if (atom == XInternAtom (xdisplay, "DVI-D", True))
    return GF_CONNECTOR_TYPE_DVID;
  if (atom == XInternAtom (xdisplay, "DisplayPort", True))
    return GF_CONNECTOR_TYPE_DisplayPort;
  if (atom == XInternAtom (xdisplay, "TV", True))
    return GF_CONNECTOR_TYPE_TV;
  if (atom == XInternAtom (xdisplay, "TV-Composite", True))
    return GF_CONNECTOR_TYPE_Composite;
  if (atom == XInternAtom (xdisplay, "TV-SVideo", True))
    return GF_CONNECTOR_TYPE_SVIDEO;
  /* Another set of mismatches. */
  if (atom == XInternAtom (xdisplay, "TV-SCART", True))
    return GF_CONNECTOR_TYPE_TV;
  if (atom == XInternAtom (xdisplay, "TV-C4", True))
    return GF_CONNECTOR_TYPE_TV;

  return GF_CONNECTOR_TYPE_Unknown;
}

static GfConnectorType
output_get_connector_type_from_prop (GfMonitorManagerXrandr *xrandr,
                                     GfOutput               *output)
{
  Atom atom, actual_type, connector_type_atom;
  gint actual_format;
  gulong nitems, bytes_after;
  guchar *buffer;
  GfConnectorType ret;

  atom = XInternAtom (xrandr->xdisplay, "ConnectorType", False);
  XRRGetOutputProperty (xrandr->xdisplay, (XID) output->winsys_id, atom,
                        0, G_MAXLONG, False, False, XA_ATOM,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_ATOM || actual_format != 32 || nitems < 1)
    {
      if (buffer)
        XFree (buffer);

      return GF_CONNECTOR_TYPE_Unknown;
    }

  connector_type_atom = ((Atom *) buffer)[0];
  ret = connector_type_from_atom (xrandr, connector_type_atom);
  XFree (buffer);

  return ret;
}

static GfConnectorType
output_get_connector_type_from_name (GfMonitorManagerXrandr *xrandr,
                                     GfOutput               *output)
{
  const gchar *name;

  name = output->name;

  /* drmmode_display.c, which was copy/pasted across all the FOSS
   * xf86-video-* drivers, seems to name its outputs based on the
   * connector type, so look for that....
   *
   * SNA has its own naming scheme, because what else did you expect
   * from SNA, but it's not too different, so we can thankfully use
   * that with minor changes.
   *
   * http://cgit.freedesktop.org/xorg/xserver/tree/hw/xfree86/drivers/modesetting/drmmode_display.c#n953
   * http://cgit.freedesktop.org/xorg/driver/xf86-video-intel/tree/src/sna/sna_display.c#n3486
   */

  if (g_str_has_prefix (name, "DVI"))
    return GF_CONNECTOR_TYPE_DVII;
  if (g_str_has_prefix (name, "LVDS"))
    return GF_CONNECTOR_TYPE_LVDS;
  if (g_str_has_prefix (name, "HDMI"))
    return GF_CONNECTOR_TYPE_HDMIA;
  if (g_str_has_prefix (name, "VGA"))
    return GF_CONNECTOR_TYPE_VGA;
  /* SNA uses DP, not DisplayPort. Test for both. */
  if (g_str_has_prefix (name, "DP") || g_str_has_prefix (name, "DisplayPort"))
    return GF_CONNECTOR_TYPE_DisplayPort;
  if (g_str_has_prefix (name, "eDP"))
    return GF_CONNECTOR_TYPE_eDP;
  if (g_str_has_prefix (name, "Virtual"))
    return GF_CONNECTOR_TYPE_VIRTUAL;
  if (g_str_has_prefix (name, "Composite"))
    return GF_CONNECTOR_TYPE_Composite;
  if (g_str_has_prefix (name, "S-video"))
    return GF_CONNECTOR_TYPE_SVIDEO;
  if (g_str_has_prefix (name, "TV"))
    return GF_CONNECTOR_TYPE_TV;
  if (g_str_has_prefix (name, "CTV"))
    return GF_CONNECTOR_TYPE_Composite;
  if (g_str_has_prefix (name, "DSI"))
    return GF_CONNECTOR_TYPE_DSI;
  if (g_str_has_prefix (name, "DIN"))
    return GF_CONNECTOR_TYPE_9PinDIN;

  return GF_CONNECTOR_TYPE_Unknown;
}

static GfConnectorType
output_get_connector_type (GfMonitorManagerXrandr *xrandr,
                           GfOutput               *output)
{
  GfConnectorType ret;

  /* The "ConnectorType" property is considered mandatory since RandR 1.3,
   * but none of the FOSS drivers support it, because we're a bunch of
   * professional software developers.
   *
   * Try poking it first, without any expectations that it will work.
   * If it's not there, we thankfully have other bonghits to try next.
   */
  ret = output_get_connector_type_from_prop (xrandr, output);
  if (ret != GF_CONNECTOR_TYPE_Unknown)
    return ret;

  /* Fall back to heuristics based on the output name. */
  ret = output_get_connector_type_from_name (xrandr, output);
  if (ret != GF_CONNECTOR_TYPE_Unknown)
    return ret;

  return GF_CONNECTOR_TYPE_Unknown;
}

static void
output_get_tile_info (GfMonitorManagerXrandr *xrandr,
                      GfOutput               *output)
{
  Atom tile_atom;
  guchar *prop;
  gulong nitems, bytes_after;
  gint actual_format;
  Atom actual_type;

  if (xrandr->has_randr15 == FALSE)
    return;

  tile_atom = XInternAtom (xrandr->xdisplay, "TILE", FALSE);
  XRRGetOutputProperty (xrandr->xdisplay, output->winsys_id,
                        tile_atom, 0, 100, False,
                        False, AnyPropertyType,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &prop);

  if (actual_type == XA_INTEGER && actual_format == 32 && nitems == 8)
    {
      glong *values = (glong *) prop;
      output->tile_info.group_id = values[0];
      output->tile_info.flags = values[1];
      output->tile_info.max_h_tiles = values[2];
      output->tile_info.max_v_tiles = values[3];
      output->tile_info.loc_h_tile = values[4];
      output->tile_info.loc_v_tile = values[5];
      output->tile_info.tile_w = values[6];
      output->tile_info.tile_h = values[7];
    }

  if (prop)
    XFree (prop);
}

static void
output_get_modes (GfMonitorManager *manager,
                  GfOutput         *output,
                  XRROutputInfo    *xrandr_output)
{
  guint j;
  guint n_actual_modes;

  output->modes = g_new0 (GfCrtcMode *, xrandr_output->nmode);

  n_actual_modes = 0;
  for (j = 0; j < (guint) xrandr_output->nmode; j++)
    {
      GList *l;

      for (l = manager->modes; l; l = l->next)
        {
          GfCrtcMode *mode = l->data;

          if (xrandr_output->modes[j] == (XID) mode->mode_id)
            {
              output->modes[n_actual_modes] = mode;
              n_actual_modes += 1;
              break;
            }
        }
    }

  output->n_modes = n_actual_modes;
  if (n_actual_modes > 0)
    output->preferred_mode = output->modes[0];
}

static void
output_get_crtcs (GfMonitorManager *manager,
                  GfOutput         *output,
                  XRROutputInfo    *xrandr_output)
{
  guint j;
  guint n_actual_crtcs;
  GList *l;

  output->possible_crtcs = g_new0 (GfCrtc *, xrandr_output->ncrtc);

  n_actual_crtcs = 0;
  for (j = 0; j < (guint) xrandr_output->ncrtc; j++)
    {
      for (l = manager->crtcs; l; l = l->next)
        {
          GfCrtc *crtc = l->data;

          if ((XID) crtc->crtc_id == xrandr_output->crtcs[j])
            {
              output->possible_crtcs[n_actual_crtcs] = crtc;
              n_actual_crtcs += 1;
              break;
            }
        }
    }
  output->n_possible_crtcs = n_actual_crtcs;

  output->crtc = NULL;
  for (l = manager->crtcs; l; l = l->next)
    {
      GfCrtc *crtc = l->data;

      if ((XID) crtc->crtc_id == xrandr_output->crtc)
        {
          output->crtc = crtc;
          break;
        }
    }
}

static gboolean
output_get_boolean_property (GfMonitorManagerXrandr *xrandr,
                             GfOutput               *output,
                             const gchar            *propname)
{
  Atom atom, actual_type;
  gint actual_format;
  gulong nitems, bytes_after;
  guchar *buffer;
  gboolean value;

  atom = XInternAtom (xrandr->xdisplay, propname, False);
  XRRGetOutputProperty (xrandr->xdisplay, (XID) output->winsys_id, atom,
                        0, G_MAXLONG, False, False, XA_CARDINAL,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_CARDINAL || actual_format != 32 || nitems < 1)
    {
      if (buffer)
        XFree (buffer);

      return FALSE;
    }

  value = ((gint*) buffer)[0];
  XFree (buffer);

  return value;
}

static gboolean
output_get_presentation_xrandr (GfMonitorManagerXrandr *xrandr,
                                GfOutput               *output)
{
  return output_get_boolean_property (xrandr, output, "_GNOME_FLASHBACK_PRESENTATION_OUTPUT");
}

static gboolean
output_get_underscanning_xrandr (GfMonitorManagerXrandr *xrandr,
                                 GfOutput               *output)
{
  Atom atom, actual_type;
  gint actual_format;
  gulong nitems, bytes_after;
  guchar *buffer;
  gchar *str;
  gboolean value;

  atom = XInternAtom (xrandr->xdisplay, "underscan", False);
  XRRGetOutputProperty (xrandr->xdisplay, (XID) output->winsys_id, atom,
                        0, G_MAXLONG, False, False, XA_ATOM,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_ATOM || actual_format != 32 || nitems < 1)
    {
      if (buffer)
        XFree (buffer);

      return FALSE;
    }

  str = XGetAtomName (xrandr->xdisplay, *(Atom *)buffer);
  XFree (buffer);

  value = !strcmp (str, "on");
  XFree (str);

  return value;
}

static gboolean
output_get_supports_underscanning_xrandr (GfMonitorManagerXrandr *xrandr,
                                          GfOutput               *output)
{
  Atom atom, actual_type;
  gint actual_format, i;
  gulong nitems, bytes_after;
  guchar *buffer;
  XRRPropertyInfo *property_info;
  Atom *values;
  gboolean supports_underscanning = FALSE;

  atom = XInternAtom (xrandr->xdisplay, "underscan", False);
  XRRGetOutputProperty (xrandr->xdisplay, (XID) output->winsys_id, atom,
                        0, G_MAXLONG, False, False, XA_ATOM,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_ATOM || actual_format != 32 || nitems < 1)
    {
      if (buffer)
        XFree (buffer);

      return FALSE;
    }

  property_info = XRRQueryOutputProperty (xrandr->xdisplay,
                                          (XID) output->winsys_id,
                                          atom);
  values = (Atom *) property_info->values;

  for (i = 0; i < property_info->num_values; i++)
    {
      /* The output supports underscanning if "on" is a valid value
       * for the underscan property.
       */
      gchar *name = XGetAtomName (xrandr->xdisplay, values[i]);
      if (strcmp (name, "on") == 0)
        supports_underscanning = TRUE;

      XFree (name);
    }

  XFree (property_info);

  return supports_underscanning;
}

static int
normalize_backlight (GfOutput *output,
                     gint      hw_value)
{
  return round ((gdouble) (hw_value - output->backlight_min) /
                (output->backlight_max - output->backlight_min) * 100.0);
}

static gint
output_get_backlight_xrandr (GfMonitorManagerXrandr *xrandr,
                             GfOutput               *output)
{
  gint value = -1;
  Atom atom, actual_type;
  gint actual_format;
  gulong nitems, bytes_after;
  guchar *buffer;

  atom = XInternAtom (xrandr->xdisplay, "Backlight", False);
  XRRGetOutputProperty (xrandr->xdisplay, (XID) output->winsys_id, atom,
                        0, G_MAXLONG, False, False, XA_INTEGER,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_INTEGER || actual_format != 32 || nitems < 1)
    {
      if (buffer)
        XFree (buffer);

      return FALSE;
    }

  value = ((gint*) buffer)[0];
  XFree (buffer);

  if (value > 0)
    return normalize_backlight (output, value);
  else
    return -1;
}

static void
output_get_backlight_limits_xrandr (GfMonitorManagerXrandr *xrandr,
                                    GfOutput               *output)
{
  Atom atom;
  xcb_connection_t *xcb_conn;
  xcb_randr_query_output_property_cookie_t cookie;
  xcb_randr_query_output_property_reply_t *reply;
  int32_t *values;

  atom = XInternAtom (xrandr->xdisplay, "Backlight", False);

  xcb_conn = XGetXCBConnection (xrandr->xdisplay);
  cookie = xcb_randr_query_output_property (xcb_conn,
                                            (xcb_randr_output_t) output->winsys_id,
                                            (xcb_atom_t) atom);

  reply = xcb_randr_query_output_property_reply (xcb_conn, cookie, NULL);

  /* This can happen on systems without backlights. */
  if (reply == NULL)
    return;

  if (!reply->range || reply->length != 2)
    {
      g_warning ("backlight %s was not range\n", output->name);
      g_free (reply);
      return;
    }

  values = xcb_randr_query_output_property_valid_values (reply);

  output->backlight_min = values[0];
  output->backlight_max = values[1];

  g_free (reply);
}

static gint
compare_outputs (const void *one,
                 const void *two)
{
  const GfOutput *o_one = one, *o_two = two;

  return strcmp (o_one->name, o_two->name);
}

static void
apply_crtc_assignments (GfMonitorManager  *manager,
                        gboolean           save_timestamp,
                        GfCrtcInfo       **crtcs,
                        guint              n_crtcs,
                        GfOutputInfo     **outputs,
                        guint              n_outputs)
{
  GfMonitorManagerXrandr *xrandr;
  gint width, height, width_mm, height_mm;
  guint i;
  GList *l;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  XGrabServer (xrandr->xdisplay);

  /* First compute the new size of the screen (framebuffer) */
  width = 0; height = 0;
  for (i = 0; i < n_crtcs; i++)
    {
      GfCrtcInfo *crtc_info = crtcs[i];
      GfCrtc *crtc = crtc_info->crtc;

      crtc->is_dirty = TRUE;

      if (crtc_info->mode == NULL)
        continue;

      if (gf_monitor_transform_is_rotated (crtc_info->transform))
        {
          width = MAX (width, crtc_info->x + crtc_info->mode->height);
          height = MAX (height, crtc_info->y + crtc_info->mode->width);
        }
      else
        {
          width = MAX (width, crtc_info->x + crtc_info->mode->width);
          height = MAX (height, crtc_info->y + crtc_info->mode->height);
        }
    }

  /* Second disable all newly disabled CRTCs, or CRTCs that in the previous
   * configuration would be outside the new framebuffer (otherwise X complains
   * loudly when resizing)
   * CRTC will be enabled again after resizing the FB
   */
  for (i = 0; i < n_crtcs; i++)
    {
      GfCrtcInfo *crtc_info = crtcs[i];
      GfCrtc *crtc = crtc_info->crtc;

      if (crtc_info->mode == NULL ||
          crtc->rect.x + crtc->rect.width > width ||
          crtc->rect.y + crtc->rect.height > height)
        {
          xrandr_set_crtc_config (xrandr,
                                  crtc,
                                  save_timestamp,
                                  (xcb_randr_crtc_t) crtc->crtc_id,
                                  XCB_CURRENT_TIME,
                                  0, 0, XCB_NONE,
                                  XCB_RANDR_ROTATION_ROTATE_0,
                                  NULL, 0);

          crtc->rect.x = 0;
          crtc->rect.y = 0;
          crtc->rect.width = 0;
          crtc->rect.height = 0;
          crtc->current_mode = NULL;
        }
    }

  /* Disable CRTCs not mentioned in the list */
  for (l = manager->crtcs; l; l = l->next)
    {
      GfCrtc *crtc = l->data;

      if (crtc->is_dirty)
        {
          crtc->is_dirty = FALSE;
          continue;
        }

      if (crtc->current_mode == NULL)
        continue;

      xrandr_set_crtc_config (xrandr,
                              crtc,
                              save_timestamp,
                              (xcb_randr_crtc_t) crtc->crtc_id,
                              XCB_CURRENT_TIME,
                              0, 0, XCB_NONE,
                              XCB_RANDR_ROTATION_ROTATE_0,
                              NULL, 0);

      crtc->rect.x = 0;
      crtc->rect.y = 0;
      crtc->rect.width = 0;
      crtc->rect.height = 0;
      crtc->current_mode = NULL;
    }

  g_assert (width > 0 && height > 0);
  /* The 'physical size' of an X screen is meaningless if that screen
   * can consist of many monitors. So just pick a size that make the
   * dpi 96.
   *
   * Firefox and Evince apparently believe what X tells them.
   */
  width_mm = (width / DPI_FALLBACK) * 25.4 + 0.5;
  height_mm = (height / DPI_FALLBACK) * 25.4 + 0.5;
  XRRSetScreenSize (xrandr->xdisplay, xrandr->xroot,
                    width, height, width_mm, height_mm);

  for (i = 0; i < n_crtcs; i++)
    {
      GfCrtcInfo *crtc_info = crtcs[i];
      GfCrtc *crtc = crtc_info->crtc;

      if (crtc_info->mode != NULL)
        {
          GfCrtcMode *mode;
          xcb_randr_output_t *output_ids;
          guint j, n_output_ids;
          xcb_randr_rotation_t rotation;

          mode = crtc_info->mode;

          n_output_ids = crtc_info->outputs->len;
          output_ids = g_new0 (xcb_randr_output_t, n_output_ids);

          for (j = 0; j < n_output_ids; j++)
            {
              GfOutput *output;

              output = ((GfOutput**) crtc_info->outputs->pdata)[j];

              output->is_dirty = TRUE;
              output->crtc = crtc;

              output_ids[j] = output->winsys_id;
            }

          rotation = gf_monitor_transform_to_xrandr (crtc_info->transform);
          if (!xrandr_set_crtc_config (xrandr,
                                       crtc,
                                       save_timestamp,
                                       (xcb_randr_crtc_t) crtc->crtc_id,
                                       XCB_CURRENT_TIME,
                                       crtc_info->x, crtc_info->y,
                                       (xcb_randr_mode_t) mode->mode_id,
                                       rotation,
                                       output_ids, n_output_ids))
            {
              g_warning ("Configuring CRTC %d with mode %d (%d x %d @ %f) at position %d, %d and transform %u failed\n",
                         (guint) (crtc->crtc_id), (guint) (mode->mode_id),
                         mode->width, mode->height, (gdouble) mode->refresh_rate,
                         crtc_info->x, crtc_info->y, crtc_info->transform);

              g_free (output_ids);
              continue;
            }

          if (gf_monitor_transform_is_rotated (crtc_info->transform))
            {
              width = mode->height;
              height = mode->width;
            }
          else
            {
              width = mode->width;
              height = mode->height;
            }

          crtc->rect.x = crtc_info->x;
          crtc->rect.y = crtc_info->y;
          crtc->rect.width = width;
          crtc->rect.height = height;
          crtc->current_mode = mode;
          crtc->transform = crtc_info->transform;

          g_free (output_ids);
        }
    }

  for (i = 0; i < n_outputs; i++)
    {
      GfOutputInfo *output_info = outputs[i];
      GfOutput *output = output_info->output;

      if (output_info->is_primary)
        {
          XRRSetOutputPrimary (xrandr->xdisplay, xrandr->xroot,
                               (XID)output_info->output->winsys_id);
        }

      output_set_presentation_xrandr (xrandr, output_info->output,
                                      output_info->is_presentation);

      if (output_get_supports_underscanning_xrandr (xrandr, output_info->output))
        output_set_underscanning_xrandr (xrandr, output_info->output,
                                         output_info->is_underscanning);

      output->is_primary = output_info->is_primary;
      output->is_presentation = output_info->is_presentation;
      output->is_underscanning = output_info->is_underscanning;
    }

  /* Disable outputs not mentioned in the list */
  for (l = manager->outputs; l; l = l->next)
    {
      GfOutput *output = l->data;

      if (output->is_dirty)
        {
          output->is_dirty = FALSE;
          continue;
        }

      output->crtc = NULL;
      output->is_primary = FALSE;
    }

  XUngrabServer (xrandr->xdisplay);
  XFlush (xrandr->xdisplay);
}

static GQuark
gf_monitor_data_quark (void)
{
  static GQuark quark;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("gf-monitor-data-quark");

  return quark;
}

static GfMonitorData *
data_from_monitor (GfMonitor *monitor)
{
  GfMonitorData *data;
  GQuark quark;

  quark = gf_monitor_data_quark ();
  data = g_object_get_qdata (G_OBJECT (monitor), quark);

  if (data)
    return data;

  data = g_new0 (GfMonitorData, 1);
  g_object_set_qdata_full (G_OBJECT (monitor), quark, data, g_free);

  return data;
}

static void
increase_monitor_count (GfMonitorManagerXrandr *xrandr,
                        Atom                    name_atom)
{
  GHashTable *atoms;
  gpointer key;
  gint count;

  atoms = xrandr->tiled_monitor_atoms;
  key = GSIZE_TO_POINTER (name_atom);

  count = GPOINTER_TO_INT (g_hash_table_lookup (atoms, key));
  count++;

  g_hash_table_insert (atoms, key, GINT_TO_POINTER (count));
}

static gint
decrease_monitor_count (GfMonitorManagerXrandr *xrandr,
                        Atom                    name_atom)
{
  GHashTable *atoms;
  gpointer key;
  gint count;

  atoms = xrandr->tiled_monitor_atoms;
  key = GSIZE_TO_POINTER (name_atom);

  count = GPOINTER_TO_SIZE (g_hash_table_lookup (atoms, key));
  count--;

  g_hash_table_insert (atoms, key, GINT_TO_POINTER (count));

  return count;
}

static void
init_monitors (GfMonitorManagerXrandr *xrandr)
{
  XRRMonitorInfo *m;
  gint n, i;

  if (xrandr->has_randr15 == FALSE)
    return;

  /* Delete any tiled monitors setup, as gnome-fashback will want to
   * recreate things in its image.
   */
  m = XRRGetMonitors (xrandr->xdisplay, xrandr->xroot, FALSE, &n);

  if (n == -1)
    return;

  for (i = 0; i < n; i++)
    {
      if (m[i].noutput > 1)
        {
          XRRDeleteMonitor (xrandr->xdisplay, xrandr->xroot, m[i].name);
        }
    }

  XRRFreeMonitors (m);
}

static void
gf_monitor_manager_xrandr_constructed (GObject *object)
{
  GfMonitorManagerXrandr *xrandr;
  GfBackend *backend;
  gint rr_event_base;
  gint rr_error_base;

  xrandr = GF_MONITOR_MANAGER_XRANDR (object);
  backend = gf_monitor_manager_get_backend (GF_MONITOR_MANAGER (xrandr));

  xrandr->xdisplay = gf_backend_x11_get_xdisplay (GF_BACKEND_X11 (backend));
  xrandr->xroot = DefaultRootWindow (xrandr->xdisplay);

  if (XRRQueryExtension (xrandr->xdisplay, &rr_event_base, &rr_error_base))
    {
      gint major_version;
      gint minor_version;

      xrandr->rr_event_base = rr_event_base;
      xrandr->rr_error_base = rr_error_base;

      /* We only use ScreenChangeNotify, but GDK uses the others,
       * and we don't want to step on its toes.
       */
      XRRSelectInput (xrandr->xdisplay, xrandr->xroot,
                      RRScreenChangeNotifyMask |
                      RRCrtcChangeNotifyMask |
                      RROutputPropertyNotifyMask);

      XRRQueryVersion (xrandr->xdisplay, &major_version, &minor_version);

      xrandr->has_randr15 = FALSE;
      if (major_version > 1 || (major_version == 1 && minor_version >= 5))
        {
          xrandr->has_randr15 = TRUE;
          xrandr->tiled_monitor_atoms = g_hash_table_new (NULL, NULL);
        }

      init_monitors (xrandr);
    }

  G_OBJECT_CLASS (gf_monitor_manager_xrandr_parent_class)->constructed (object);
}

static void
gf_monitor_manager_xrandr_dispose (GObject *object)
{
  GfMonitorManagerXrandr *xrandr;

  xrandr = GF_MONITOR_MANAGER_XRANDR (object);

  g_clear_pointer (&xrandr->tiled_monitor_atoms, g_hash_table_destroy);

  G_OBJECT_CLASS (gf_monitor_manager_xrandr_parent_class)->dispose (object);
}

static void
gf_monitor_manager_xrandr_finalize (GObject *object)
{
  GfMonitorManagerXrandr *xrandr;

  xrandr = GF_MONITOR_MANAGER_XRANDR (object);

  g_clear_pointer (&xrandr->resources, XRRFreeScreenResources);
  g_clear_pointer (&xrandr->supported_scales, g_free);

  G_OBJECT_CLASS (gf_monitor_manager_xrandr_parent_class)->finalize (object);
}

static void
gf_monitor_manager_xrandr_read_current (GfMonitorManager *manager)
{
  GfMonitorManagerXrandr *xrandr;
  XRRScreenResources *resources;
  CARD16 dpms_state;
  BOOL dpms_enabled;
  gint min_width;
  gint min_height;
  Screen *screen;
  guint i, j;
  GList *l;
  RROutput primary_output;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  if (DPMSCapable (xrandr->xdisplay) &&
      DPMSInfo (xrandr->xdisplay, &dpms_state, &dpms_enabled) &&
      dpms_enabled)
    {
      switch (dpms_state)
        {
          case DPMSModeOn:
            manager->power_save_mode = GF_POWER_SAVE_ON;
            break;

          case DPMSModeStandby:
            manager->power_save_mode = GF_POWER_SAVE_STANDBY;
            break;

          case DPMSModeSuspend:
            manager->power_save_mode = GF_POWER_SAVE_SUSPEND;
            break;

          case DPMSModeOff:
            manager->power_save_mode = GF_POWER_SAVE_OFF;
            break;

          default:
            manager->power_save_mode = GF_POWER_SAVE_UNSUPPORTED;
            break;
        }
    }
  else
    {
      manager->power_save_mode = GF_POWER_SAVE_UNSUPPORTED;
    }

  XRRGetScreenSizeRange (xrandr->xdisplay, xrandr->xroot,
                         &min_width, &min_height,
                         &xrandr->max_screen_width,
                         &xrandr->max_screen_height);

  /* This is updated because we called RRUpdateConfiguration below */
  screen = ScreenOfDisplay (xrandr->xdisplay, DefaultScreen (xrandr->xdisplay));
  manager->screen_width = WidthOfScreen (screen);
  manager->screen_height = HeightOfScreen (screen);

  g_clear_pointer (&xrandr->resources, XRRFreeScreenResources);
  resources = XRRGetScreenResourcesCurrent (xrandr->xdisplay, xrandr->xroot);

  if (!resources)
    return;

  xrandr->resources = resources;
  manager->outputs = NULL;
  manager->modes = NULL;
  manager->crtcs = NULL;

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
      mode->name = g_strdup_printf ("%dx%d", xmode->width, xmode->height);

      manager->modes = g_list_append (manager->modes, mode);
    }

  for (i = 0; i < (guint) resources->ncrtc; i++)
    {
      XRRCrtcInfo *xrandr_crtc;
      RRCrtc crtc_id;
      GfCrtc *crtc;

      crtc_id = resources->crtcs[i];
      xrandr_crtc = XRRGetCrtcInfo (xrandr->xdisplay, resources, crtc_id);
      crtc = gf_create_xrandr_crtc (manager, xrandr_crtc, crtc_id, resources);

      manager->crtcs = g_list_append (manager->crtcs, crtc);
      XRRFreeCrtcInfo (xrandr_crtc);
    }

  primary_output = XRRGetOutputPrimary (xrandr->xdisplay, xrandr->xroot);

  for (i = 0; i < (guint) resources->noutput; i++)
    {
      XRROutputInfo *xrandr_output;
      GfOutput *output;

      xrandr_output = XRRGetOutputInfo (xrandr->xdisplay, resources,
                                        resources->outputs[i]);

      if (!xrandr_output)
        continue;

      if (xrandr_output->connection != RR_Disconnected)
        {
          GBytes *edid;

          output = g_object_new (GF_TYPE_OUTPUT, NULL);
          output->monitor_manager = manager;

          output->winsys_id = resources->outputs[i];
          output->name = g_strdup (xrandr_output->name);

          edid = read_output_edid (xrandr, output->winsys_id);
          gf_output_parse_edid (output, edid);
          g_bytes_unref (edid);

          output->width_mm = xrandr_output->mm_width;
          output->height_mm = xrandr_output->mm_height;
          output->hotplug_mode_update = output_get_hotplug_mode_update (xrandr, output);
          output->suggested_x = output_get_suggested_x (xrandr, output);
          output->suggested_y = output_get_suggested_y (xrandr, output);
          output->connector_type = output_get_connector_type (xrandr, output);

          output_get_tile_info (xrandr, output);
          output_get_modes (manager, output, xrandr_output);
          output_get_crtcs (manager, output, xrandr_output);

          output->n_possible_clones = xrandr_output->nclone;
          output->possible_clones = g_new0 (GfOutput *, output->n_possible_clones);

          /* We can build the list of clones now, because we don't have
           * the list of outputs yet, so temporarily set the pointers to
           * the bare XIDs, and then we'll fix them in a second pass
           */
          for (j = 0; j < (guint) xrandr_output->nclone; j++)
            {
              output->possible_clones[j] = GINT_TO_POINTER (xrandr_output->clones[j]);
            }

          output->is_primary = ((XID) output->winsys_id == primary_output);
          output->is_presentation = output_get_presentation_xrandr (xrandr, output);
          output->is_underscanning = output_get_underscanning_xrandr (xrandr, output);
          output->supports_underscanning = output_get_supports_underscanning_xrandr (xrandr, output);

          output_get_backlight_limits_xrandr (xrandr, output);

          if (!(output->backlight_min == 0 && output->backlight_max == 0))
            output->backlight = output_get_backlight_xrandr (xrandr, output);
          else
            output->backlight = -1;

          if (output->n_modes == 0 || output->n_possible_crtcs == 0)
            g_object_unref (output);
          else
            manager->outputs = g_list_prepend (manager->outputs, output);
        }

      XRRFreeOutputInfo (xrandr_output);
    }

  /* Sort the outputs for easier handling in GfMonitorConfig */
  manager->outputs = g_list_sort (manager->outputs, compare_outputs);

  /* Now fix the clones */
  for (l = manager->outputs; l; l = l->next)
    {
      GfOutput *output;
      GList *k;

      output = l->data;

      for (j = 0; j < output->n_possible_clones; j++)
        {
          RROutput clone = GPOINTER_TO_INT (output->possible_clones[j]);

          for (k = manager->outputs; k; k = k->next)
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
}

static GBytes *
gf_monitor_manager_xrandr_read_edid (GfMonitorManager *manager,
                                     GfOutput         *output)
{
  GfMonitorManagerXrandr *xrandr;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  return read_output_edid (xrandr, output->winsys_id);
}

static void
gf_monitor_manager_xrandr_ensure_initial_config (GfMonitorManager *manager)
{
  GfMonitorConfigManager *config_manager;
  GfMonitorsConfig *config;

  gf_monitor_manager_ensure_configured (manager);

  /* Normally we don't rebuild our data structures until we see the
   * RRScreenNotify event, but at least at startup we want to have the
   * right configuration immediately.
   */
  gf_monitor_manager_read_current_state (manager);

  config_manager = gf_monitor_manager_get_config_manager (manager);
  config = gf_monitor_config_manager_get_current (config_manager);

  gf_monitor_manager_update_logical_state_derived (manager, config);
}

static gboolean
gf_monitor_manager_xrandr_apply_monitors_config (GfMonitorManager        *manager,
                                                 GfMonitorsConfig        *config,
                                                 GfMonitorsConfigMethod   method,
                                                 GError                 **error)
{
  GPtrArray *crtc_infos;
  GPtrArray *output_infos;

  if (!config)
    {
      gf_monitor_manager_xrandr_rebuild_derived (manager, NULL);
      return TRUE;
    }

  if (!gf_monitor_config_manager_assign (manager, config,
                                         &crtc_infos, &output_infos,
                                         error))
    return FALSE;

  if (method != GF_MONITORS_CONFIG_METHOD_VERIFY)
    {
      /*
       * If the assignment has not changed, we won't get any notification about
       * any new configuration from the X server; but we still need to update
       * our own configuration, as something not applicable in Xrandr might
       * have changed locally, such as the logical monitors scale. This means we
       * must check that our new assignment actually changes anything, otherwise
       * just update the logical state.
       */
      if (is_assignments_changed (manager,
                                  (GfCrtcInfo **) crtc_infos->pdata,
                                  crtc_infos->len,
                                  (GfOutputInfo **) output_infos->pdata,
                                  output_infos->len))
        {
          apply_crtc_assignments (manager,
                                  TRUE,
                                  (GfCrtcInfo **) crtc_infos->pdata,
                                  crtc_infos->len,
                                  (GfOutputInfo **) output_infos->pdata,
                                  output_infos->len);
        }
      else
        {
          gf_monitor_manager_xrandr_rebuild_derived (manager, config);
        }
    }

  g_ptr_array_free (crtc_infos, TRUE);
  g_ptr_array_free (output_infos, TRUE);

  return TRUE;
}

static void
gf_monitor_manager_xrandr_set_power_save_mode (GfMonitorManager *manager,
                                               GfPowerSave       mode)
{
  GfMonitorManagerXrandr *xrandr;
  CARD16 state;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  switch (mode)
    {
      case GF_POWER_SAVE_ON:
        state = DPMSModeOn;
        break;

      case GF_POWER_SAVE_STANDBY:
        state = DPMSModeStandby;
        break;

      case GF_POWER_SAVE_SUSPEND:
        state = DPMSModeSuspend;
        break;

      case GF_POWER_SAVE_OFF:
        state = DPMSModeOff;
        break;

      case GF_POWER_SAVE_UNSUPPORTED:
      default:
        return;
    }

  DPMSForceLevel (xrandr->xdisplay, state);
  DPMSSetTimeouts (xrandr->xdisplay, 0, 0, 0);
}

static void
gf_monitor_manager_xrandr_change_backlight (GfMonitorManager *manager,
                                            GfOutput         *output,
                                            gint              value)
{
  GfMonitorManagerXrandr *xrandr;
  gint hw_value;
  Atom atom;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  hw_value = round ((gdouble) value / 100.0 * output->backlight_max + output->backlight_min);
  atom = XInternAtom (xrandr->xdisplay, "Backlight", False);

  xcb_randr_change_output_property (XGetXCBConnection (xrandr->xdisplay),
                                    (XID)output->winsys_id,
                                    atom, XCB_ATOM_INTEGER, 32,
                                    XCB_PROP_MODE_REPLACE,
                                    1, &hw_value);

  /* We're not selecting for property notifies, so update the value immediately */
  output->backlight = normalize_backlight (output, hw_value);
}

static void
gf_monitor_manager_xrandr_get_crtc_gamma (GfMonitorManager  *manager,
                                          GfCrtc            *crtc,
                                          gsize             *size,
                                          gushort          **red,
                                          gushort          **green,
                                          gushort          **blue)
{
  GfMonitorManagerXrandr *xrandr;
  XRRCrtcGamma *gamma;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);
  gamma = XRRGetCrtcGamma (xrandr->xdisplay, (XID) crtc->crtc_id);

  *size = gamma->size;
  *red = g_memdup (gamma->red, sizeof (gushort) * gamma->size);
  *green = g_memdup (gamma->green, sizeof (gushort) * gamma->size);
  *blue = g_memdup (gamma->blue, sizeof (gushort) * gamma->size);

  XRRFreeGamma (gamma);
}

static void
gf_monitor_manager_xrandr_set_crtc_gamma (GfMonitorManager *manager,
                                          GfCrtc           *crtc,
                                          gsize             size,
                                          gushort          *red,
                                          gushort          *green,
                                          gushort          *blue)
{
  GfMonitorManagerXrandr *xrandr;
  XRRCrtcGamma *gamma;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  gamma = XRRAllocGamma (size);
  memcpy (gamma->red, red, sizeof (gushort) * size);
  memcpy (gamma->green, green, sizeof (gushort) * size);
  memcpy (gamma->blue, blue, sizeof (gushort) * size);

  XRRSetCrtcGamma (xrandr->xdisplay, (XID) crtc->crtc_id, gamma);
  XRRFreeGamma (gamma);
}

static void
gf_monitor_manager_xrandr_tiled_monitor_added (GfMonitorManager *manager,
                                               GfMonitor        *monitor)
{
  GfMonitorManagerXrandr *xrandr;
  GfMonitorTiled *monitor_tiled;
  const gchar *product;
  uint32_t tile_group_id;
  gchar *name;
  Atom name_atom;
  GfMonitorData *data;
  GList *outputs;
  XRRMonitorInfo *monitor_info;
  GList *l;
  gint i;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  if (xrandr->has_randr15 == FALSE)
    return;

  monitor_tiled = GF_MONITOR_TILED (monitor);
  product = gf_monitor_get_product (monitor);
  tile_group_id = gf_monitor_tiled_get_tile_group_id (monitor_tiled);

  if (product)
    name = g_strdup_printf ("%s-%d", product, tile_group_id);
  else
    name = g_strdup_printf ("Tiled-%d", tile_group_id);

  name_atom = XInternAtom (xrandr->xdisplay, name, False);
  g_free (name);

  data = data_from_monitor (monitor);
  data->xrandr_name = name_atom;

  increase_monitor_count (xrandr, name_atom);

  outputs = gf_monitor_get_outputs (monitor);
  monitor_info = XRRAllocateMonitor (xrandr->xdisplay, g_list_length (outputs));

  monitor_info->name = name_atom;
  monitor_info->primary = gf_monitor_is_primary (monitor);
  monitor_info->automatic = True;

  for (l = outputs, i = 0; l; l = l->next, i++)
    {
      GfOutput *output = l->data;

      monitor_info->outputs[i] = output->winsys_id;
    }

  XRRSetMonitor (xrandr->xdisplay, xrandr->xroot, monitor_info);
  XRRFreeMonitors (monitor_info);
}

static void
gf_monitor_manager_xrandr_tiled_monitor_removed (GfMonitorManager *manager,
                                                 GfMonitor        *monitor)
{
  GfMonitorManagerXrandr *xrandr;
  GfMonitorData *data;
  gint monitor_count;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  if (xrandr->has_randr15 == FALSE)
    return;

  data = data_from_monitor (monitor);
  monitor_count = decrease_monitor_count (xrandr, data->xrandr_name);

  if (monitor_count == 0)
    {
      XRRDeleteMonitor (xrandr->xdisplay, xrandr->xroot, data->xrandr_name);
    }
}

static gboolean
gf_monitor_manager_xrandr_is_transform_handled (GfMonitorManager   *manager,
                                                GfCrtc             *crtc,
                                                GfMonitorTransform  transform)
{
  g_warn_if_fail ((crtc->all_transforms & transform) == transform);

  return TRUE;
}

static gfloat
gf_monitor_manager_xrandr_calculate_monitor_mode_scale (GfMonitorManager *manager,
                                                        GfMonitor        *monitor,
                                                        GfMonitorMode    *monitor_mode)
{
  return gf_monitor_calculate_mode_scale (monitor, monitor_mode);
}

static gfloat *
gf_monitor_manager_xrandr_calculate_supported_scales (GfMonitorManager           *manager,
                                                      GfLogicalMonitorLayoutMode  layout_mode,
                                                      GfMonitor                  *monitor,
                                                      GfMonitorMode              *monitor_mode,
                                                      gint                       *n_supported_scales)
{
  GfMonitorManagerXrandr *xrandr;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  ensure_supported_monitor_scales (manager);

  *n_supported_scales = xrandr->n_supported_scales;
  return g_memdup (xrandr->supported_scales,
                   xrandr->n_supported_scales * sizeof (gfloat));
}

static GfMonitorManagerCapability
gf_monitor_manager_xrandr_get_capabilities (GfMonitorManager *manager)
{
  GfMonitorManagerCapability capabilities;

  capabilities = GF_MONITOR_MANAGER_CAPABILITY_NONE;

  capabilities |= GF_MONITOR_MANAGER_CAPABILITY_MIRRORING;
  capabilities |= GF_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED;

  return capabilities;
}

static gboolean
gf_monitor_manager_xrandr_get_max_screen_size (GfMonitorManager *manager,
                                               gint             *max_width,
                                               gint             *max_height)
{
  GfMonitorManagerXrandr *xrandr;

  xrandr = GF_MONITOR_MANAGER_XRANDR (manager);

  *max_width = xrandr->max_screen_width;
  *max_height = xrandr->max_screen_height;

  return TRUE;
}

static GfLogicalMonitorLayoutMode
gf_monitor_manager_xrandr_get_default_layout_mode (GfMonitorManager *manager)
{
  return GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;
}

static void
gf_monitor_manager_xrandr_class_init (GfMonitorManagerXrandrClass *xrandr_class)
{
  GObjectClass *object_class;
  GfMonitorManagerClass *manager_class;

  object_class = G_OBJECT_CLASS (xrandr_class);
  manager_class = GF_MONITOR_MANAGER_CLASS (xrandr_class);

  object_class->constructed = gf_monitor_manager_xrandr_constructed;
  object_class->dispose = gf_monitor_manager_xrandr_dispose;
  object_class->finalize = gf_monitor_manager_xrandr_finalize;

  manager_class->read_current = gf_monitor_manager_xrandr_read_current;
  manager_class->read_edid = gf_monitor_manager_xrandr_read_edid;
  manager_class->ensure_initial_config = gf_monitor_manager_xrandr_ensure_initial_config;
  manager_class->apply_monitors_config = gf_monitor_manager_xrandr_apply_monitors_config;
  manager_class->set_power_save_mode = gf_monitor_manager_xrandr_set_power_save_mode;
  manager_class->change_backlight = gf_monitor_manager_xrandr_change_backlight;
  manager_class->get_crtc_gamma = gf_monitor_manager_xrandr_get_crtc_gamma;
  manager_class->set_crtc_gamma = gf_monitor_manager_xrandr_set_crtc_gamma;
  manager_class->tiled_monitor_added = gf_monitor_manager_xrandr_tiled_monitor_added;
  manager_class->tiled_monitor_removed = gf_monitor_manager_xrandr_tiled_monitor_removed;
  manager_class->is_transform_handled = gf_monitor_manager_xrandr_is_transform_handled;
  manager_class->calculate_monitor_mode_scale = gf_monitor_manager_xrandr_calculate_monitor_mode_scale;
  manager_class->calculate_supported_scales = gf_monitor_manager_xrandr_calculate_supported_scales;
  manager_class->get_capabilities = gf_monitor_manager_xrandr_get_capabilities;
  manager_class->get_max_screen_size = gf_monitor_manager_xrandr_get_max_screen_size;
  manager_class->get_default_layout_mode = gf_monitor_manager_xrandr_get_default_layout_mode;
}

static void
gf_monitor_manager_xrandr_init (GfMonitorManagerXrandr *xrandr)
{
}

Display *
gf_monitor_manager_xrandr_get_xdisplay (GfMonitorManagerXrandr *xrandr)
{
  return xrandr->xdisplay;
}

XRRScreenResources *
gf_monitor_manager_xrandr_get_resources (GfMonitorManagerXrandr *xrandr)
{
  return xrandr->resources;
}

gboolean
gf_monitor_manager_xrandr_handle_xevent (GfMonitorManagerXrandr *xrandr,
                                         XEvent                 *event)
{
  GfMonitorManager *manager;
  XRRScreenResources *resources;

  manager = GF_MONITOR_MANAGER (xrandr);

  if ((event->type - xrandr->rr_event_base) != RRScreenChangeNotify)
    return FALSE;

  XRRUpdateConfiguration (event);
  gf_monitor_manager_read_current_state (manager);

  resources = xrandr->resources;
  if (!resources)
    return TRUE;

  if (resources->timestamp < resources->configTimestamp)
    {
      gf_monitor_manager_on_hotplug (manager);
    }
  else
    {
      GfMonitorsConfig *config;

      config = NULL;

      if (resources->timestamp == xrandr->last_xrandr_set_timestamp)
        {
          GfMonitorConfigManager *config_manager;

          config_manager = gf_monitor_manager_get_config_manager (manager);
          config = gf_monitor_config_manager_get_current (config_manager);
        }

      gf_monitor_manager_xrandr_rebuild_derived (manager, config);
    }

  return TRUE;
}
