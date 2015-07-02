/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2015 Alberts Muktupāvels
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
 * Adapted from mutter 3.16.0:
 * - /src/backends/meta-monitor-manager.c
 * - /src/backends/x11/meta-monitor-manager-xrandr.c
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkx.h>
#include <math.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/dpms.h>
#include <X11/Xlib-xcb.h>
#include <xcb/randr.h>
#include "edid.h"
#include "flashback-monitor-config.h"
#include "flashback-monitor-manager.h"

#define ALL_TRANSFORMS ((1 << (META_MONITOR_TRANSFORM_FLIPPED_270 + 1)) - 1)

/* Look for DPI_FALLBACK in:
 * http://git.gnome.org/browse/gnome-settings-daemon/tree/plugins/xsettings/gsd-xsettings-manager.c
 * for the reasoning */
#define DPI_FALLBACK 96.0

struct _FlashbackMonitorManagerPrivate
{
  Display               *xdisplay;

  XRRScreenResources    *resources;

  int                    rr_event_base;
  int                    rr_error_base;

  gboolean               has_randr15;

  MetaDBusDisplayConfig *display_config;
};

G_DEFINE_TYPE_WITH_PRIVATE (FlashbackMonitorManager, flashback_monitor_manager, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_DISPLAY_CONFIG,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

static void
add_monitor (FlashbackMonitorManager *manager,
             MetaMonitorInfo         *monitor)
{
#ifdef HAVE_XRANDR15
  XRRMonitorInfo *m;
  int o;
  Atom name;
  char name_buf[40];

  if (manager->priv->has_randr15 == FALSE)
    return;

  if (monitor->n_outputs <= 1)
    return;

  if (monitor->outputs[0]->product)
    snprintf (name_buf, 40, "%s-%d", monitor->outputs[0]->product, monitor->outputs[0]->tile_info.group_id);
  else
    snprintf (name_buf, 40, "Tiled-%d", monitor->outputs[0]->tile_info.group_id);

  name = XInternAtom (manager->priv->xdisplay, name_buf, False);
  monitor->monitor_winsys_xid = name;
  m = XRRAllocateMonitor (manager->priv->xdisplay, monitor->n_outputs);
  if (!m)
    return;

  m->name = name;
  m->primary = monitor->is_primary;
  m->automatic = True;

  for (o = 0; o < monitor->n_outputs; o++)
    {
      MetaOutput *output = monitor->outputs[o];
      m->outputs[o] = output->winsys_id;
    }

  XRRSetMonitor (manager->priv->xdisplay,
                 DefaultRootWindow (manager->priv->xdisplay),
                 m);

  XRRFreeMonitors (m);
#endif
}

static void
remove_monitor (FlashbackMonitorManager *manager,
                int                      monitor_winsys_xid)
{
#ifdef HAVE_XRANDR15
  if (manager->priv->has_randr15 == FALSE)
    return;

  XRRDeleteMonitor (manager->priv->xdisplay,
                    DefaultRootWindow (manager->priv->xdisplay),
                    monitor_winsys_xid);
#endif
}

static void
init_monitors (FlashbackMonitorManager *manager)
{
#ifdef HAVE_XRANDR15
  XRRMonitorInfo *m;
  int n, i;

  if (manager->priv->has_randr15 == FALSE)
    return;

  /* delete any tiled monitors setup, as mutter will want to recreate
     things in its image */
  m = XRRGetMonitors (manager->priv->xdisplay,
                      DefaultRootWindow (manager->priv->xdisplay),
                      FALSE, &n);

  if (n == -1)
    return;

  for (i = 0; i < n; i++)
    {
      if (m[i].noutput > 1)
        XRRDeleteMonitor (manager->priv->xdisplay,
                          DefaultRootWindow (manager->priv->xdisplay),
                          m[i].name);
    }

  XRRFreeMonitors (m);
#endif
}

static GdkFilterReturn
filter_func (GdkXEvent *xevent,
             GdkEvent  *event,
             gpointer   user_data)
{
  FlashbackMonitorManager *manager;
  XEvent *xev;

  manager = FLASHBACK_MONITOR_MANAGER (user_data);
  xev = (XEvent *) xevent;

  if ((xev->type - manager->priv->rr_event_base) == RRScreenChangeNotify)
    {
      XRRUpdateConfiguration (xev);

      flashback_monitor_manager_read_current_config (manager);

      if (manager->priv->resources->timestamp < manager->priv->resources->configTimestamp)
        {
          /* This is a hotplug event, so go ahead and build a new configuration. */
          flashback_monitor_manager_on_hotplug (manager);
        }
      else
        {
          /* Something else changed -- tell the world about it. */
          flashback_monitor_manager_rebuild_derived (manager);
        }
    }

  return GDK_FILTER_CONTINUE;
}

static gboolean
rectangle_contains_rect (const GdkRectangle *outer_rect,
                         const GdkRectangle *inner_rect)
{
  return
    inner_rect->x                      >= outer_rect->x &&
    inner_rect->y                      >= outer_rect->y &&
    inner_rect->x + inner_rect->width  <= outer_rect->x + outer_rect->width &&
    inner_rect->y + inner_rect->height <= outer_rect->y + outer_rect->height;
}

static Rotation
meta_monitor_transform_to_xrandr (MetaMonitorTransform transform)
{
  switch (transform)
    {
    case META_MONITOR_TRANSFORM_NORMAL:
      return RR_Rotate_0;
    case META_MONITOR_TRANSFORM_90:
      return RR_Rotate_90;
    case META_MONITOR_TRANSFORM_180:
      return RR_Rotate_180;
    case META_MONITOR_TRANSFORM_270:
      return RR_Rotate_270;
    case META_MONITOR_TRANSFORM_FLIPPED:
      return RR_Reflect_X | RR_Rotate_0;
    case META_MONITOR_TRANSFORM_FLIPPED_90:
      return RR_Reflect_X | RR_Rotate_90;
    case META_MONITOR_TRANSFORM_FLIPPED_180:
      return RR_Reflect_X | RR_Rotate_180;
    case META_MONITOR_TRANSFORM_FLIPPED_270:
      return RR_Reflect_X | RR_Rotate_270;
    }

  g_assert_not_reached ();
}

static MetaMonitorTransform
meta_monitor_transform_from_xrandr (Rotation rotation)
{
  static const MetaMonitorTransform y_reflected_map[4] = {
    META_MONITOR_TRANSFORM_FLIPPED_180,
    META_MONITOR_TRANSFORM_FLIPPED_90,
    META_MONITOR_TRANSFORM_FLIPPED,
    META_MONITOR_TRANSFORM_FLIPPED_270
  };
  MetaMonitorTransform ret;

  switch (rotation & 0x7F)
    {
    default:
    case RR_Rotate_0:
      ret = META_MONITOR_TRANSFORM_NORMAL;
      break;
    case RR_Rotate_90:
      ret = META_MONITOR_TRANSFORM_90;
      break;
    case RR_Rotate_180:
      ret = META_MONITOR_TRANSFORM_180;
      break;
    case RR_Rotate_270:
      ret = META_MONITOR_TRANSFORM_270;
      break;
    }

  if (rotation & RR_Reflect_X)
    return ret + 4;
  else if (rotation & RR_Reflect_Y)
    return y_reflected_map[ret];
  else
    return ret;
}

#define ALL_ROTATIONS (RR_Rotate_0 | RR_Rotate_90 | RR_Rotate_180 | RR_Rotate_270)

static MetaMonitorTransform
meta_monitor_transform_from_xrandr_all (Rotation rotation)
{
  unsigned ret;

  /* Handle the common cases first (none or all) */
  if (rotation == 0 || rotation == RR_Rotate_0)
    return (1 << META_MONITOR_TRANSFORM_NORMAL);

  /* All rotations and one reflection -> all of them by composition */
  if ((rotation & ALL_ROTATIONS) &&
      ((rotation & RR_Reflect_X) || (rotation & RR_Reflect_Y)))
    return ALL_TRANSFORMS;

  ret = 1 << META_MONITOR_TRANSFORM_NORMAL;
  if (rotation & RR_Rotate_90)
    ret |= 1 << META_MONITOR_TRANSFORM_90;
  if (rotation & RR_Rotate_180)
    ret |= 1 << META_MONITOR_TRANSFORM_180;
  if (rotation & RR_Rotate_270)
    ret |= 1 << META_MONITOR_TRANSFORM_270;
  if (rotation & (RR_Rotate_0 | RR_Reflect_X))
    ret |= 1 << META_MONITOR_TRANSFORM_FLIPPED;
  if (rotation & (RR_Rotate_90 | RR_Reflect_X))
    ret |= 1 << META_MONITOR_TRANSFORM_FLIPPED_90;
  if (rotation & (RR_Rotate_180 | RR_Reflect_X))
    ret |= 1 << META_MONITOR_TRANSFORM_FLIPPED_180;
  if (rotation & (RR_Rotate_270 | RR_Reflect_X))
    ret |= 1 << META_MONITOR_TRANSFORM_FLIPPED_270;

  return ret;
}

static gboolean
output_get_integer_property (FlashbackMonitorManagerPrivate *priv,
                             MetaOutput                     *output,
                             const char                     *propname,
                             gint                           *value)
{
  gboolean exists = FALSE;
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  unsigned char *buffer;

  atom = XInternAtom (priv->xdisplay, propname, False);
  XRRGetOutputProperty (priv->xdisplay,
                        (XID) output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_INTEGER,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  exists = (actual_type == XA_INTEGER && actual_format == 32 && nitems == 1);

  if (exists && value != NULL)
    *value = ((int*)buffer)[0];

  XFree (buffer);
  return exists;
}

static gboolean
output_get_property_exists (FlashbackMonitorManagerPrivate *priv,
                            MetaOutput                     *output,
                            const char                     *propname)
{
  gboolean exists = FALSE;
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  unsigned char *buffer;

  atom = XInternAtom (priv->xdisplay, propname, False);
  XRRGetOutputProperty (priv->xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, AnyPropertyType,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  exists = (actual_type != None);

  XFree (buffer);
  return exists;
}

static gboolean
output_get_boolean_property (FlashbackMonitorManagerPrivate *priv,
                             MetaOutput                     *output,
                             const char                     *propname)
{
  gboolean value = FALSE;
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  unsigned char *buffer;

  atom = XInternAtom (priv->xdisplay, propname, False);
  XRRGetOutputProperty (priv->xdisplay,
                        (XID) output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_CARDINAL,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_CARDINAL || actual_format != 32 || nitems < 1)
    goto out;

  value = ((int*)buffer)[0];

 out:
  XFree (buffer);
  return value;
}

static gboolean
output_get_presentation_xrandr (FlashbackMonitorManagerPrivate *priv,
                                MetaOutput                     *output)
{
  return output_get_boolean_property (priv, output, "_GNOME_FLASHBACK_PRESENTATION_OUTPUT");
}

static void
output_set_presentation_xrandr (FlashbackMonitorManagerPrivate *priv,
                                MetaOutput                     *output,
                                gboolean                        presentation)
{
  Atom atom;
  int value;

  value = presentation;
  atom = XInternAtom (priv->xdisplay, "_GNOME_FLASHBACK_PRESENTATION_OUTPUT", False);
  XRRChangeOutputProperty (priv->xdisplay,
                           (XID) output->winsys_id,
                           atom,
                           XA_CARDINAL, 32, PropModeReplace,
                           (unsigned char*) &value, 1);
}

static gboolean
output_get_underscanning_xrandr (FlashbackMonitorManagerPrivate *priv,
                                 MetaOutput                     *output)
{
  gboolean value = FALSE;
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *buffer;
  char *str;

  atom = XInternAtom (priv->xdisplay, "underscan", False);
  XRRGetOutputProperty (priv->xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_ATOM,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_ATOM || actual_format != 32 ||
      nitems < 1)
    goto out;

  str = XGetAtomName (priv->xdisplay, *(Atom *)buffer);
  value = !strcmp(str, "on");
  XFree (str);

out:
  XFree (buffer);
  return value;
}

static gboolean
output_get_supports_underscanning_xrandr (FlashbackMonitorManagerPrivate *priv,
                                          MetaOutput                     *output)
{
  Atom atom, actual_type;
  int actual_format, i;
  unsigned long nitems, bytes_after;
  unsigned char *buffer;
  XRRPropertyInfo *property_info;
  Atom *values;
  gboolean supports_underscanning = FALSE;

  atom = XInternAtom (priv->xdisplay, "underscan", False);
  XRRGetOutputProperty (priv->xdisplay,
                        (XID)output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_ATOM,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_ATOM || actual_format != 32 || nitems < 1)
    {
      XFree (buffer);
      return FALSE;
    }

  property_info = XRRQueryOutputProperty (priv->xdisplay,
                                          (XID) output->winsys_id,
                                          atom);
  values = (Atom *) property_info->values;

  for (i = 0; i < property_info->num_values; i++)
    {
      /* The output supports underscanning if "on" is a valid value
       * for the underscan property.
       */
      char *name = XGetAtomName (priv->xdisplay, values[i]);
      if (strcmp (name, "on") == 0)
        supports_underscanning = TRUE;

      XFree (name);
    }

  XFree (property_info);

  return supports_underscanning;
}

static int
normalize_backlight (MetaOutput *output,
                     int         hw_value)
{
  return round ((double)(hw_value - output->backlight_min) /
                (output->backlight_max - output->backlight_min) * 100.0);
}

static int
output_get_backlight_xrandr (FlashbackMonitorManagerPrivate *priv,
                             MetaOutput                     *output)
{
  int value = -1;
  Atom atom, actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  unsigned char *buffer;

  atom = XInternAtom (priv->xdisplay, "Backlight", False);
  XRRGetOutputProperty (priv->xdisplay,
                        (XID) output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_INTEGER,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_INTEGER || actual_format != 32 || nitems < 1)
    goto out;

  value = ((int*)buffer)[0];

 out:
  XFree (buffer);
  if (value > 0)
    return normalize_backlight (output, value);
  else
    return -1;
}

static void
output_get_backlight_limits_xrandr (FlashbackMonitorManagerPrivate *priv,
                                    MetaOutput                     *output)
{
  Atom atom;
  xcb_connection_t *xcb_conn;
  xcb_randr_query_output_property_reply_t *reply;

  atom = XInternAtom (priv->xdisplay, "Backlight", False);

  xcb_conn = XGetXCBConnection (priv->xdisplay);
  reply = xcb_randr_query_output_property_reply (xcb_conn,
                                                 xcb_randr_query_output_property (xcb_conn,
                                                                                  (xcb_randr_output_t) output->winsys_id,
                                                                                  (xcb_atom_t) atom),
                                                 NULL);

  /* This can happen on systems without backlights. */
  if (reply == NULL)
    return;

  if (!reply->range || reply->length != 2)
    {
      g_warning ("backlight %s was not range\n", output->name);
      goto out;
    }

  int32_t *values = xcb_randr_query_output_property_valid_values (reply);
  output->backlight_min = values[0];
  output->backlight_max = values[1];

out:
  free (reply);
}

static void
output_get_tile_info (FlashbackMonitorManagerPrivate *priv,
                      MetaOutput                     *output)
{
  Atom tile_atom;
  unsigned char *prop;
  unsigned long nitems, bytes_after;
  int actual_format;
  Atom actual_type;

  if (priv->has_randr15 == FALSE)
    return;

  tile_atom = XInternAtom (priv->xdisplay, "TILE", FALSE);
  XRRGetOutputProperty (priv->xdisplay, output->winsys_id,
                        tile_atom, 0, 100, False,
                        False, AnyPropertyType,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &prop);

  if (actual_type == XA_INTEGER && actual_format == 32 && nitems == 8)
    {
      long *values = (long *)prop;
      output->tile_info.group_id = values[0];
      output->tile_info.flags = values[1];
      output->tile_info.max_h_tiles = values[2];
      output->tile_info.max_v_tiles = values[3];
      output->tile_info.loc_h_tile = values[4];
      output->tile_info.loc_v_tile = values[5];
      output->tile_info.tile_w = values[6];
      output->tile_info.tile_h = values[7];
    }

  XFree (prop);
}

static gboolean
output_get_hotplug_mode_update (FlashbackMonitorManagerPrivate *priv,
                                MetaOutput                     *output)
{
  return output_get_property_exists (priv, output, "hotplug_mode_update");
}

static gint
output_get_suggested_x (FlashbackMonitorManagerPrivate *priv,
                        MetaOutput                     *output)
{
  gint val;
  if (output_get_integer_property (priv, output, "suggested X", &val))
    return val;

  return -1;
}

static gint
output_get_suggested_y (FlashbackMonitorManagerPrivate *priv,
                        MetaOutput                     *output)
{
  gint val;
  if (output_get_integer_property (priv, output, "suggested Y", &val))
    return val;

  return -1;
}

static MetaConnectorType
connector_type_from_atom (FlashbackMonitorManagerPrivate *priv,
                          Atom                            atom)
{
  Display *xdpy = priv->xdisplay;

  if (atom == XInternAtom (xdpy, "HDMI", True))
    return META_CONNECTOR_TYPE_HDMIA;
  if (atom == XInternAtom (xdpy, "VGA", True))
    return META_CONNECTOR_TYPE_VGA;
  /* Doesn't have a DRM equivalent, but means an internal panel.
   * We could pick either LVDS or eDP here. */
  if (atom == XInternAtom (xdpy, "Panel", True))
    return META_CONNECTOR_TYPE_LVDS;
  if (atom == XInternAtom (xdpy, "DVI", True) || atom == XInternAtom (xdpy, "DVI-I", True))
    return META_CONNECTOR_TYPE_DVII;
  if (atom == XInternAtom (xdpy, "DVI-A", True))
    return META_CONNECTOR_TYPE_DVIA;
  if (atom == XInternAtom (xdpy, "DVI-D", True))
    return META_CONNECTOR_TYPE_DVID;
  if (atom == XInternAtom (xdpy, "DisplayPort", True))
    return META_CONNECTOR_TYPE_DisplayPort;

  if (atom == XInternAtom (xdpy, "TV", True))
    return META_CONNECTOR_TYPE_TV;
  if (atom == XInternAtom (xdpy, "TV-Composite", True))
    return META_CONNECTOR_TYPE_Composite;
  if (atom == XInternAtom (xdpy, "TV-SVideo", True))
    return META_CONNECTOR_TYPE_SVIDEO;
  /* Another set of mismatches. */
  if (atom == XInternAtom (xdpy, "TV-SCART", True))
    return META_CONNECTOR_TYPE_TV;
  if (atom == XInternAtom (xdpy, "TV-C4", True))
    return META_CONNECTOR_TYPE_TV;

  return META_CONNECTOR_TYPE_Unknown;
}

static MetaConnectorType
output_get_connector_type_from_prop (FlashbackMonitorManagerPrivate *priv,
                                     MetaOutput                     *output)
{
  MetaConnectorType ret = META_CONNECTOR_TYPE_Unknown;
  Atom atom, actual_type, connector_type_atom;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *buffer;

  atom = XInternAtom (priv->xdisplay, "ConnectorType", False);
  XRRGetOutputProperty (priv->xdisplay,
                        (XID) output->winsys_id,
                        atom,
                        0, G_MAXLONG, False, False, XA_ATOM,
                        &actual_type, &actual_format,
                        &nitems, &bytes_after, &buffer);

  if (actual_type != XA_ATOM || actual_format != 32 || nitems < 1)
    goto out;

  connector_type_atom = ((Atom *) buffer)[0];
  ret = connector_type_from_atom (priv, connector_type_atom);

 out:
  if (buffer)
    XFree (buffer);
  return ret;
}

static MetaConnectorType
output_get_connector_type_from_name (FlashbackMonitorManagerPrivate *priv,
                                     MetaOutput                     *output)
{
  const char *name = output->name;

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
    return META_CONNECTOR_TYPE_DVII;
  if (g_str_has_prefix (name, "LVDS"))
    return META_CONNECTOR_TYPE_LVDS;
  if (g_str_has_prefix (name, "HDMI"))
    return META_CONNECTOR_TYPE_HDMIA;
  if (g_str_has_prefix (name, "VGA"))
    return META_CONNECTOR_TYPE_VGA;
  /* SNA uses DP, not DisplayPort. Test for both. */
  if (g_str_has_prefix (name, "DP") || g_str_has_prefix (name, "DisplayPort"))
    return META_CONNECTOR_TYPE_DisplayPort;
  if (g_str_has_prefix (name, "eDP"))
    return META_CONNECTOR_TYPE_eDP;
  if (g_str_has_prefix (name, "Virtual"))
    return META_CONNECTOR_TYPE_VIRTUAL;
  if (g_str_has_prefix (name, "Composite"))
    return META_CONNECTOR_TYPE_Composite;
  if (g_str_has_prefix (name, "S-video"))
    return META_CONNECTOR_TYPE_SVIDEO;
  if (g_str_has_prefix (name, "TV"))
    return META_CONNECTOR_TYPE_TV;
  if (g_str_has_prefix (name, "CTV"))
    return META_CONNECTOR_TYPE_Composite;
  if (g_str_has_prefix (name, "DSI"))
    return META_CONNECTOR_TYPE_DSI;
  if (g_str_has_prefix (name, "DIN"))
    return META_CONNECTOR_TYPE_9PinDIN;

  return META_CONNECTOR_TYPE_Unknown;
}

static MetaConnectorType
output_get_connector_type (FlashbackMonitorManagerPrivate *priv,
                           MetaOutput                     *output)
{
  MetaConnectorType ret;

  /* The "ConnectorType" property is considered mandatory since RandR 1.3,
   * but none of the FOSS drivers support it, because we're a bunch of
   * professional software developers.
   *
   * Try poking it first, without any expectations that it will work.
   * If it's not there, we thankfully have other bonghits to try next.
   */
  ret = output_get_connector_type_from_prop (priv, output);
  if (ret != META_CONNECTOR_TYPE_Unknown)
    return ret;

  /* Fall back to heuristics based on the output name. */
  ret = output_get_connector_type_from_name (priv, output);
  if (ret != META_CONNECTOR_TYPE_Unknown)
    return ret;

  return META_CONNECTOR_TYPE_Unknown;
}

static int
compare_outputs (const void *one,
                 const void *two)
{
  const MetaOutput *o_one = one, *o_two = two;

  return strcmp (o_one->name, o_two->name);
}

static char *
get_xmode_name (XRRModeInfo *xmode)
{
  int width = xmode->width;
  int height = xmode->height;

  return g_strdup_printf ("%dx%d", width, height);
}

static gboolean
gdk_rectangle_equal (const GdkRectangle *src1,
                     const GdkRectangle *src2)
{
  return ((src1->x == src2->x) && (src1->y == src2->y) &&
          (src1->width == src2->width) && (src1->height == src2->height));
}

static gboolean
gdk_rectangle_contains_rect (const GdkRectangle *outer_rect,
                             const GdkRectangle *inner_rect)
{
  return inner_rect->x                      >= outer_rect->x &&
         inner_rect->y                      >= outer_rect->y &&
         inner_rect->x + inner_rect->width  <= outer_rect->x + outer_rect->width &&
         inner_rect->y + inner_rect->height <= outer_rect->y + outer_rect->height;
}

/*
 * rules for constructing a tiled monitor
 * 1. find a tile_group_id
 * 2. iterate over all outputs for that tile group id
 * 3. see if output has a crtc and if it is configured for the tile size
 * 4. calculate the total tile size
 * 5. set tile finished size
 * 6. check for more tile_group_id
*/
static void
construct_tile_monitor (FlashbackMonitorManager *manager,
                        GArray                  *monitor_infos,
                        guint32                  tile_group_id)
{
  MetaMonitorInfo info;
  unsigned i;

  for (i = 0; i < monitor_infos->len; i++)
    {
      MetaMonitorInfo *pinfo = &g_array_index (monitor_infos, MetaMonitorInfo, i);

      if (pinfo->tile_group_id == tile_group_id)
        return;
    }

  /* didn't find it */
  info.number = monitor_infos->len;
  info.tile_group_id = tile_group_id;
  info.is_presentation = FALSE;
  info.refresh_rate = 0.0;
  info.width_mm = 0;
  info.height_mm = 0;
  info.is_primary = FALSE;
  info.rect.x = INT_MAX;
  info.rect.y = INT_MAX;
  info.rect.width = 0;
  info.rect.height = 0;
  info.winsys_id = 0;
  info.n_outputs = 0;
  info.monitor_winsys_xid = 0;

  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output = &manager->outputs[i];

      if (!output->tile_info.group_id)
        continue;

      if (output->tile_info.group_id != tile_group_id)
        continue;

      if (!output->crtc)
        continue;

      if (output->crtc->rect.width != (int)output->tile_info.tile_w ||
          output->crtc->rect.height != (int)output->tile_info.tile_h)
        continue;

      if (output->tile_info.loc_h_tile == 0 && output->tile_info.loc_v_tile == 0)
        {
          info.refresh_rate = output->crtc->current_mode->refresh_rate;
          info.width_mm = output->width_mm;
          info.height_mm = output->height_mm;
          info.winsys_id = output->winsys_id;
        }

      /* hack */
      if (output->crtc->rect.x < info.rect.x)
        info.rect.x = output->crtc->rect.x;
      if (output->crtc->rect.y < info.rect.y)
        info.rect.y = output->crtc->rect.y;

      if (output->tile_info.loc_h_tile == 0)
        info.rect.height += output->tile_info.tile_h;

      if (output->tile_info.loc_v_tile == 0)
        info.rect.width += output->tile_info.tile_w;

      if (info.n_outputs > META_MAX_OUTPUTS_PER_MONITOR)
        continue;

      info.outputs[info.n_outputs++] = output;
    }

  /* if we don't have a winsys id, i.e. we haven't found tile 0,0
     don't try and add this to the monitor infos */
  if (!info.winsys_id)
    return;

  g_array_append_val (monitor_infos, info);
}

/*
 * make_logical_config:
 *
 * Turn outputs and CRTCs into logical MetaMonitorInfo,
 * that will be used by the core and API layer (MetaScreen
 * and friends)
 */
static void
make_logical_config (FlashbackMonitorManager *manager)
{
  GArray *monitor_infos;
  unsigned int i, j;

  monitor_infos = g_array_sized_new (FALSE, TRUE, sizeof (MetaMonitorInfo),
                                     manager->n_outputs);

  /* Walk the list of MetaCRTCs, and build a MetaMonitorInfo
     for each of them, unless they reference a rectangle that
     is already there.
  */

  /* for tiling we need to work out how many tiled outputs there are */
  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output = &manager->outputs[i];

      if (output->tile_info.group_id)
        construct_tile_monitor (manager, monitor_infos, output->tile_info.group_id);
    }

  for (i = 0; i < manager->n_crtcs; i++)
    {
      MetaCRTC *crtc = &manager->crtcs[i];

      /* Ignore CRTCs not in use */
      if (crtc->current_mode == NULL)
        continue;

      for (j = 0; j < monitor_infos->len; j++)
        {
          MetaMonitorInfo *info = &g_array_index (monitor_infos, MetaMonitorInfo, j);
          if (gdk_rectangle_contains_rect (&crtc->rect, &info->rect))
            {
              crtc->logical_monitor = info;
              break;
            }
        }

      if (crtc->logical_monitor == NULL)
        {
          MetaMonitorInfo info;

          info.number = monitor_infos->len;
          info.tile_group_id = 0;
          info.rect = crtc->rect;
          info.refresh_rate = crtc->current_mode->refresh_rate;
          info.is_primary = FALSE;
          /* This starts true because we want
             is_presentation only if all outputs are
             marked as such (while for primary it's enough
             that any is marked)
          */
          info.is_presentation = TRUE;
          info.in_fullscreen = -1;
          info.winsys_id = 0;

          info.n_outputs = 0;
          info.monitor_winsys_xid = 0;

          g_array_append_val (monitor_infos, info);

          crtc->logical_monitor = &g_array_index (monitor_infos, MetaMonitorInfo,
                                                  info.number);
        }
    }

  /* Now walk the list of outputs applying extended properties (primary
     and presentation)
  */
  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output;
      MetaMonitorInfo *info;

      output = &manager->outputs[i];

      /* Ignore outputs that are not active */
      if (output->crtc == NULL)
        continue;

      if (output->tile_info.group_id)
        continue;

      /* We must have a logical monitor on every CRTC at this point */
      g_assert (output->crtc->logical_monitor != NULL);

      info = output->crtc->logical_monitor;

      info->is_primary = info->is_primary || output->is_primary;
      info->is_presentation = info->is_presentation && output->is_presentation;

      info->width_mm = output->width_mm;
      info->height_mm = output->height_mm;

      info->outputs[0] = output;
      info->n_outputs = 1;

      if (output->is_primary || info->winsys_id == 0)
        info->winsys_id = output->winsys_id;

      if (info->is_primary)
        manager->primary_monitor_index = info->number;
    }

  manager->n_monitor_infos = monitor_infos->len;
  manager->monitor_infos = (void*)g_array_free (monitor_infos, FALSE);

  for (i = 0; i < manager->n_monitor_infos; i++)
    add_monitor (manager, &manager->monitor_infos[i]);
}

static void
free_output_array (MetaOutput *old_outputs,
                   int         n_old_outputs)
{
  int i;

  for (i = 0; i < n_old_outputs; i++)
    {
      g_free (old_outputs[i].name);
      g_free (old_outputs[i].vendor);
      g_free (old_outputs[i].product);
      g_free (old_outputs[i].serial);
      g_free (old_outputs[i].modes);
      g_free (old_outputs[i].possible_crtcs);
      g_free (old_outputs[i].possible_clones);

      if (old_outputs[i].driver_notify)
        old_outputs[i].driver_notify (&old_outputs[i]);
    }

  g_free (old_outputs);
}

static void
free_mode_array (MetaMonitorMode *old_modes,
                 int              n_old_modes)
{
  int i;

  for (i = 0; i < n_old_modes; i++)
    {
      g_free (old_modes[i].name);

      if (old_modes[i].driver_notify)
        old_modes[i].driver_notify (&old_modes[i]);
    }

  g_free (old_modes);
}

static guint8 *
get_edid_property (Display  *dpy,
                   RROutput  output,
                   Atom      atom,
                   gsize    *len)
{
  unsigned char *prop;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  Atom actual_type;
  guint8 *result;

  XRRGetOutputProperty (dpy, output, atom,
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

  XFree (prop);

  return result;
}

static GBytes *
read_output_edid (FlashbackMonitorManager *manager,
                  XID                      winsys_id)
{
  Atom edid_atom;
  guint8 *result;
  gsize len;

  edid_atom = XInternAtom (manager->priv->xdisplay, "EDID", FALSE);
  result = get_edid_property (manager->priv->xdisplay, winsys_id, edid_atom, &len);

  if (!result)
    {
      edid_atom = XInternAtom (manager->priv->xdisplay, "EDID_DATA", FALSE);
      result = get_edid_property (manager->priv->xdisplay, winsys_id, edid_atom, &len);
    }

  if (!result)
    {
      edid_atom = XInternAtom (manager->priv->xdisplay, "XFree86_DDC_EDID1_RAWDATA", FALSE);
      result = get_edid_property (manager->priv->xdisplay, winsys_id, edid_atom, &len);
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

static void
read_current_config (FlashbackMonitorManager *manager)
{
  FlashbackMonitorManagerPrivate *priv;
  XRRScreenResources *resources;
  RROutput primary_output;
  unsigned int i;
  unsigned int j;
  unsigned int k;
  unsigned int n_actual_outputs;
  int min_width;
  int min_height;
  Screen *screen;
  BOOL dpms_capable;
  BOOL dpms_enabled;
  CARD16 dpms_state;

  priv = manager->priv;

  if (priv->resources)
    XRRFreeScreenResources (priv->resources);
  priv->resources = NULL;

  dpms_capable = DPMSCapable (priv->xdisplay);

  if (dpms_capable &&
      DPMSInfo (priv->xdisplay, &dpms_state, &dpms_enabled) &&
      dpms_enabled)
    {
      switch (dpms_state)
        {
        case DPMSModeOn:
          manager->power_save_mode = META_POWER_SAVE_ON;
          break;
        case DPMSModeStandby:
          manager->power_save_mode = META_POWER_SAVE_STANDBY;
          break;
        case DPMSModeSuspend:
          manager->power_save_mode = META_POWER_SAVE_SUSPEND;
          break;
        case DPMSModeOff:
          manager->power_save_mode = META_POWER_SAVE_OFF;
          break;
        default:
          manager->power_save_mode = META_POWER_SAVE_UNSUPPORTED;
          break;
        }
    }
  else
    {
      manager->power_save_mode = META_POWER_SAVE_UNSUPPORTED;
    }

  XRRGetScreenSizeRange (priv->xdisplay, DefaultRootWindow (priv->xdisplay),
                         &min_width, &min_height,
                         &manager->max_screen_width,
                         &manager->max_screen_height);

  screen = ScreenOfDisplay (priv->xdisplay, DefaultScreen (priv->xdisplay));

  /* This is updated because we called RRUpdateConfiguration below */
  manager->screen_width = WidthOfScreen (screen);
  manager->screen_height = HeightOfScreen (screen);

  resources = XRRGetScreenResourcesCurrent (priv->xdisplay, DefaultRootWindow (priv->xdisplay));
  if (!resources)
    return;

  priv->resources = resources;
  manager->n_outputs = resources->noutput;
  manager->n_crtcs = resources->ncrtc;
  manager->n_modes = resources->nmode;
  manager->outputs = g_new0 (MetaOutput, manager->n_outputs);
  manager->modes = g_new0 (MetaMonitorMode, manager->n_modes);
  manager->crtcs = g_new0 (MetaCRTC, manager->n_crtcs);

  for (i = 0; i < (unsigned) resources->nmode; i++)
    {
      XRRModeInfo *xmode = &resources->modes[i];
      MetaMonitorMode *mode;

      mode = &manager->modes[i];

      mode->mode_id = xmode->id;
      mode->width = xmode->width;
      mode->height = xmode->height;
      mode->refresh_rate = (xmode->dotClock / ((float)xmode->hTotal * xmode->vTotal));
      mode->name = get_xmode_name (xmode);
    }

  for (i = 0; i < (unsigned) resources->ncrtc; i++)
    {
      XRRCrtcInfo *crtc;
      MetaCRTC *meta_crtc;

      crtc = XRRGetCrtcInfo (priv->xdisplay, resources, resources->crtcs[i]);

      meta_crtc = &manager->crtcs[i];

      meta_crtc->crtc_id = resources->crtcs[i];
      meta_crtc->rect.x = crtc->x;
      meta_crtc->rect.y = crtc->y;
      meta_crtc->rect.width = crtc->width;
      meta_crtc->rect.height = crtc->height;
      meta_crtc->is_dirty = FALSE;
      meta_crtc->transform = meta_monitor_transform_from_xrandr (crtc->rotation);
      meta_crtc->all_transforms = meta_monitor_transform_from_xrandr_all (crtc->rotations);

      for (j = 0; j < (unsigned) resources->nmode; j++)
        {
          if (resources->modes[j].id == crtc->mode)
            {
              meta_crtc->current_mode = &manager->modes[j];
              break;
            }
        }

      XRRFreeCrtcInfo (crtc);
    }

  primary_output = XRRGetOutputPrimary (priv->xdisplay, DefaultRootWindow (priv->xdisplay));

  n_actual_outputs = 0;
  for (i = 0; i < (unsigned) resources->noutput; i++)
    {
      XRROutputInfo *output;
      MetaOutput *meta_output;

      output = XRRGetOutputInfo (priv->xdisplay, resources, resources->outputs[i]);

      meta_output = &manager->outputs[n_actual_outputs];

      if (output->connection != RR_Disconnected)
        {
          GBytes *edid;

          meta_output->winsys_id = resources->outputs[i];
          meta_output->name = g_strdup (output->name);

          edid = read_output_edid (manager, meta_output->winsys_id);
          meta_output_parse_edid (meta_output, edid);
          g_bytes_unref (edid);

          meta_output->width_mm = output->mm_width;
          meta_output->height_mm = output->mm_height;
          meta_output->hotplug_mode_update = output_get_hotplug_mode_update (priv, meta_output);
          meta_output->suggested_x = output_get_suggested_x (priv, meta_output);
          meta_output->suggested_y = output_get_suggested_y (priv, meta_output);
          meta_output->connector_type = output_get_connector_type (priv, meta_output);

          output_get_tile_info (priv, meta_output);

          meta_output->n_modes = output->nmode;
          meta_output->modes = g_new0 (MetaMonitorMode *, meta_output->n_modes);

          for (j = 0; j < meta_output->n_modes; j++)
            {
              for (k = 0; k < manager->n_modes; k++)
                {
                  if (output->modes[j] == (XID)manager->modes[k].mode_id)
                    {
                      meta_output->modes[j] = &manager->modes[k];
                      break;
                    }
                }
            }

          meta_output->preferred_mode = meta_output->modes[0];

          meta_output->n_possible_crtcs = output->ncrtc;
          meta_output->possible_crtcs = g_new0 (MetaCRTC *, meta_output->n_possible_crtcs);

          for (j = 0; j < (unsigned)output->ncrtc; j++)
            {
              for (k = 0; k < manager->n_crtcs; k++)
                {
                  if ((XID)manager->crtcs[k].crtc_id == output->crtcs[j])
                    {
                      meta_output->possible_crtcs[j] = &manager->crtcs[k];
                      break;
                    }
                }
            }

          meta_output->crtc = NULL;

          for (j = 0; j < manager->n_crtcs; j++)
            {
              if ((XID)manager->crtcs[j].crtc_id == output->crtc)
                {
                  meta_output->crtc = &manager->crtcs[j];
                  break;
                }
            }

          meta_output->n_possible_clones = output->nclone;
          meta_output->possible_clones = g_new0 (MetaOutput *, meta_output->n_possible_clones);
          /* We can build the list of clones now, because we don't have the list of outputs
             yet, so temporarily set the pointers to the bare XIDs, and then we'll fix them
             in a second pass
          */
          for (j = 0; j < (unsigned)output->nclone; j++)
            {
              meta_output->possible_clones[j] = GINT_TO_POINTER (output->clones[j]);
            }

          meta_output->is_primary = ((XID)meta_output->winsys_id == primary_output);
          meta_output->is_presentation = output_get_presentation_xrandr (priv, meta_output);
          meta_output->is_underscanning = output_get_underscanning_xrandr (priv, meta_output);
          meta_output->supports_underscanning = output_get_supports_underscanning_xrandr (priv, meta_output);
          output_get_backlight_limits_xrandr (priv, meta_output);

        if (!(meta_output->backlight_min == 0 && meta_output->backlight_max == 0))
          meta_output->backlight = output_get_backlight_xrandr (priv, meta_output);
        else
          meta_output->backlight = -1;

        n_actual_outputs++;
      }

      XRRFreeOutputInfo (output);
    }

  manager->n_outputs = n_actual_outputs;

  /* Sort the outputs for easier handling in MetaMonitorConfig */
  qsort (manager->outputs, manager->n_outputs, sizeof (MetaOutput), compare_outputs);

  /* Now fix the clones */
  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *meta_output;

      meta_output = &manager->outputs[i];

      for (j = 0; j < meta_output->n_possible_clones; j++)
        {
          RROutput clone;

          clone = GPOINTER_TO_INT (meta_output->possible_clones[j]);

          for (k = 0; k < manager->n_outputs; k++)
            {
              if (clone == (XID)manager->outputs[k].winsys_id)
                {
                  meta_output->possible_clones[j] = &manager->outputs[k];
                  break;
                }
            }
        }
    }
}

static void
flashback_monitor_manager_constructed (GObject *object)
{
  FlashbackMonitorManager *manager;

  manager = FLASHBACK_MONITOR_MANAGER (object);

  manager->in_init = TRUE;

  manager->monitor_config = flashback_monitor_config_new (manager);

  flashback_monitor_manager_read_current_config (manager);

  if (!flashback_monitor_config_apply_stored (manager->monitor_config))
    flashback_monitor_config_make_default (manager->monitor_config);

  /* Under XRandR, we don't rebuild our data structures until we see
     the RRScreenNotify event, but at least at startup we want to have
     the right configuration immediately.

     The other backends keep the data structures always updated,
     so this is not needed.
  */
  flashback_monitor_manager_read_current_config (manager);

  make_logical_config (manager);

  manager->in_init = FALSE;
}

static void
flashback_monitor_manager_set_property (GObject      *object,
                                        guint         property_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  FlashbackMonitorManager *manager;

  manager = FLASHBACK_MONITOR_MANAGER (object);

  switch (property_id)
    {
      case PROP_DISPLAY_CONFIG:
        if (manager->priv->display_config)
          g_object_unref (manager->priv->display_config);
        manager->priv->display_config = g_value_get_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
flashback_monitor_manager_get_property (GObject    *object,
                                        guint       property_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  FlashbackMonitorManager *manager;

  manager = FLASHBACK_MONITOR_MANAGER (object);

  switch (property_id)
    {
      case PROP_DISPLAY_CONFIG:
        g_value_set_object (value, manager->priv->display_config);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
flashback_monitor_manager_finalize (GObject *object)
{
  FlashbackMonitorManager *manager;

  manager = FLASHBACK_MONITOR_MANAGER (object);

  gdk_window_remove_filter (NULL, filter_func, manager);

  if (manager->priv->resources)
    XRRFreeScreenResources (manager->priv->resources);
  manager->priv->resources = NULL;

  free_output_array (manager->outputs, manager->n_outputs);
  free_mode_array (manager->modes, manager->n_modes);
  g_free (manager->monitor_infos);
  g_free (manager->crtcs);

  G_OBJECT_CLASS (flashback_monitor_manager_parent_class)->finalize (object);
}

static void
flashback_monitor_manager_class_init (FlashbackMonitorManagerClass *manager_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (manager_class);

  object_class->constructed = flashback_monitor_manager_constructed;
  object_class->set_property = flashback_monitor_manager_set_property;
  object_class->get_property = flashback_monitor_manager_get_property;
  object_class->finalize = flashback_monitor_manager_finalize;

  object_properties[PROP_DISPLAY_CONFIG] =
    g_param_spec_object ("display-config",
                         "display-config",
                         "display-config",
                         META_DBUS_TYPE_DISPLAY_CONFIG,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPERTIES,
                                     object_properties);
}

static void
flashback_monitor_manager_init (FlashbackMonitorManager *manager)
{
  FlashbackMonitorManagerPrivate *priv;
  int major_version;
  int minor_version;

  priv = flashback_monitor_manager_get_instance_private (manager);
  manager->priv = priv;

  priv->xdisplay = gdk_x11_display_get_xdisplay (gdk_display_get_default ());

  if (!XRRQueryExtension (priv->xdisplay, &priv->rr_event_base, &priv->rr_error_base))
    return;

  gdk_window_add_filter (NULL, filter_func, manager);

  /* We only use ScreenChangeNotify, but GDK uses the others,
     and we don't want to step on its toes */
  XRRSelectInput (priv->xdisplay, DefaultRootWindow (priv->xdisplay),
                  RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask |
                  RROutputPropertyNotifyMask);

  priv->has_randr15 = FALSE;

  XRRQueryVersion (priv->xdisplay, &major_version, &minor_version);

#ifdef HAVE_XRANDR15
  if (major_version > 1 || (major_version == 1 && minor_version >= 5))
    priv->has_randr15 = TRUE;
#endif

  init_monitors (manager);
}

FlashbackMonitorManager *
flashback_monitor_manager_new (MetaDBusDisplayConfig *display_config)
{
  return g_object_new (FLASHBACK_TYPE_MONITOR_MANAGER,
                       "display-config", display_config,
                       NULL);
}

static void
output_set_underscanning_xrandr (FlashbackMonitorManagerPrivate *priv,
                                 MetaOutput                     *output,
                                 gboolean                        underscanning)
{
  Atom prop, valueatom;
  const char *value;

  prop = XInternAtom (priv->xdisplay, "underscan", False);

  value = underscanning ? "on" : "off";
  valueatom = XInternAtom (priv->xdisplay, value, False);

  XRRChangeOutputProperty (priv->xdisplay,
                           (XID)output->winsys_id,
                           prop,
                           XA_ATOM, 32, PropModeReplace,
                           (unsigned char*) &valueatom, 1);

  /* Configure the border at the same time. Currently, we use a
   * 5% of the width/height of the mode. In the future, we should
   * make the border configurable. */
  if (underscanning)
    {
      uint32_t border_value;

      prop = XInternAtom (priv->xdisplay, "underscan hborder", False);
      border_value = output->crtc->current_mode->width * 0.05;
      XRRChangeOutputProperty (priv->xdisplay,
                               (XID)output->winsys_id,
                               prop,
                               XA_INTEGER, 32, PropModeReplace,
                               (unsigned char *) &border_value, 1);

      prop = XInternAtom (priv->xdisplay, "underscan vborder", False);
      border_value = output->crtc->current_mode->height * 0.05;
      XRRChangeOutputProperty (priv->xdisplay,
                               (XID)output->winsys_id,
                               prop,
                               XA_INTEGER, 32, PropModeReplace,
                               (unsigned char *) &border_value, 1);
    }
}

void
flashback_monitor_manager_apply_configuration (FlashbackMonitorManager  *manager,
                                               MetaCRTCInfo            **crtcs,
                                               unsigned int              n_crtcs,
                                               MetaOutputInfo          **outputs,
                                               unsigned int              n_outputs)
{
  FlashbackMonitorManagerPrivate *priv;
  unsigned i;
  int width;
  int height;
  int width_mm;
  int height_mm;

  priv = manager->priv;

  XGrabServer (priv->xdisplay);

  /* First compute the new size of the screen (framebuffer) */
  width = 0; height = 0;
  for (i = 0; i < n_crtcs; i++)
    {
      MetaCRTCInfo *crtc_info = crtcs[i];
      MetaCRTC *crtc = crtc_info->crtc;
      crtc->is_dirty = TRUE;

      if (crtc_info->mode == NULL)
        continue;

      if (crtc_info->transform % 2)
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
     configuration would be outside the new framebuffer (otherwise X complains
     loudly when resizing)
     CRTC will be enabled again after resizing the FB
  */
  for (i = 0; i < n_crtcs; i++)
    {
      MetaCRTCInfo *crtc_info = crtcs[i];
      MetaCRTC *crtc = crtc_info->crtc;

      if (crtc_info->mode == NULL ||
          crtc->rect.x + crtc->rect.width > width ||
          crtc->rect.y + crtc->rect.height > height)
        {
          XRRSetCrtcConfig (priv->xdisplay,
                            priv->resources,
                            (XID) crtc->crtc_id,
                            CurrentTime,
                            0, 0,
                            None,
                            RR_Rotate_0,
                            NULL, 0);

          crtc->rect.x = 0;
          crtc->rect.y = 0;
          crtc->rect.width = 0;
          crtc->rect.height = 0;
          crtc->current_mode = NULL;
        }
    }

  /* Disable CRTCs not mentioned in the list */
  for (i = 0; i < manager->n_crtcs; i++)
    {
      MetaCRTC *crtc = &manager->crtcs[i];

      if (crtc->is_dirty)
        {
          crtc->is_dirty = FALSE;
          continue;
        }
      if (crtc->current_mode == NULL)
        continue;

      XRRSetCrtcConfig (priv->xdisplay,
                        priv->resources,
                        (XID) crtc->crtc_id,
                        CurrentTime,
                        0, 0,
                        None,
                        RR_Rotate_0,
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
  XRRSetScreenSize (priv->xdisplay, DefaultRootWindow (priv->xdisplay),
                    width, height, width_mm, height_mm);

  for (i = 0; i < n_crtcs; i++)
    {
      MetaCRTCInfo *crtc_info = crtcs[i];
      MetaCRTC *crtc = crtc_info->crtc;

      if (crtc_info->mode != NULL)
        {
          MetaMonitorMode *mode;
          XID *outputs;
          unsigned int j, n_outputs;
          int width, height;
          Status ok;

          mode = crtc_info->mode;

          n_outputs = crtc_info->outputs->len;
          outputs = g_new (XID, n_outputs);

          for (j = 0; j < n_outputs; j++)
            {
              MetaOutput *output;

              output = ((MetaOutput**)crtc_info->outputs->pdata)[j];

              output->is_dirty = TRUE;
              output->crtc = crtc;

              outputs[j] = output->winsys_id;
            }

          ok = XRRSetCrtcConfig (priv->xdisplay,
                                 priv->resources,
                                 (XID) crtc->crtc_id,
                                 CurrentTime,
                                 crtc_info->x, crtc_info->y,
                                 (XID) mode->mode_id,
                                 meta_monitor_transform_to_xrandr (crtc_info->transform),
                                 outputs, n_outputs);

          if (ok != Success)
            {
              g_warning ("Configuring CRTC %d with mode %d (%d x %d @ %f) at position %d, %d and transform %u failed\n",
                         (unsigned) (crtc->crtc_id), (unsigned) (mode->mode_id),
                         mode->width, mode->height, (float) mode->refresh_rate,
                         crtc_info->x, crtc_info->y, crtc_info->transform);
              goto next;
            }

          if (crtc_info->transform % 2)
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

        next:
          g_free (outputs);
        }
    }

  for (i = 0; i < n_outputs; i++)
    {
      MetaOutputInfo *output_info = outputs[i];
      MetaOutput *output = output_info->output;

      if (output_info->is_primary)
        {
          XRRSetOutputPrimary (priv->xdisplay,
                               DefaultRootWindow (priv->xdisplay),
                               (XID)output_info->output->winsys_id);
        }

      output_set_presentation_xrandr (priv,
                                      output_info->output,
                                      output_info->is_presentation);

      if (output_get_supports_underscanning_xrandr (priv, output_info->output))
        output_set_underscanning_xrandr (priv,
                                         output_info->output,
                                         output_info->is_underscanning);

      output->is_primary = output_info->is_primary;
      output->is_presentation = output_info->is_presentation;
      output->is_underscanning = output_info->is_underscanning;
    }

  /* Disable outputs not mentioned in the list */
  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output = &manager->outputs[i];

      if (output->is_dirty)
        {
          output->is_dirty = FALSE;
          continue;
        }

      output->crtc = NULL;
      output->is_primary = FALSE;
    }

  XUngrabServer (priv->xdisplay);
  XFlush (priv->xdisplay);
}

void
flashback_monitor_manager_confirm_configuration (FlashbackMonitorManager *manager,
                                                 gboolean                 ok)
{
  if (!manager->persistent_timeout_id)
    {
      /* too late */
      return;
    }

  g_source_remove (manager->persistent_timeout_id);
  manager->persistent_timeout_id = 0;

  if (ok)
    flashback_monitor_config_make_persistent (manager->monitor_config);
  else
    flashback_monitor_config_restore_previous (manager->monitor_config);
}

void
flashback_monitor_manager_change_backlight (FlashbackMonitorManager *manager,
					                                  MetaOutput              *output,
					                                  gint                     value)
{
  FlashbackMonitorManagerPrivate *priv;
  Atom atom;
  int hw_value;

  priv = manager->priv;
  hw_value = round ((double)value / 100.0 * output->backlight_max + output->backlight_min);

  atom = XInternAtom (priv->xdisplay, "Backlight", False);
  XRRChangeOutputProperty (priv->xdisplay,
                           (XID) output->winsys_id,
                           atom,
                           XA_INTEGER, 32, PropModeReplace,
                           (unsigned char *) &hw_value, 1);

  /* We're not selecting for property notifies, so update the value immediately */
  output->backlight = normalize_backlight (output, hw_value);
}

void
flashback_monitor_manager_get_crtc_gamma (FlashbackMonitorManager  *manager,
                                          MetaCRTC                 *crtc,
                                          gsize                    *size,
                                          unsigned short          **red,
                                          unsigned short          **green,
                                          unsigned short          **blue)
{
  XRRCrtcGamma *gamma;

  gamma = XRRGetCrtcGamma (manager->priv->xdisplay, (XID) crtc->crtc_id);

  *size = gamma->size;
  *red = g_memdup (gamma->red, sizeof (unsigned short) * gamma->size);
  *green = g_memdup (gamma->green, sizeof (unsigned short) * gamma->size);
  *blue = g_memdup (gamma->blue, sizeof (unsigned short) * gamma->size);

  XRRFreeGamma (gamma);
}

void
flashback_monitor_manager_set_crtc_gamma (FlashbackMonitorManager *manager,
                                          MetaCRTC                *crtc,
                                          gsize                    size,
                                          unsigned short          *red,
                                          unsigned short          *green,
                                          unsigned short          *blue)
{
  XRRCrtcGamma *gamma;

  gamma = XRRAllocGamma (size);
  memcpy (gamma->red, red, sizeof (unsigned short) * size);
  memcpy (gamma->green, green, sizeof (unsigned short) * size);
  memcpy (gamma->blue, blue, sizeof (unsigned short) * size);

  XRRSetCrtcGamma (manager->priv->xdisplay, (XID) crtc->crtc_id, gamma);

  XRRFreeGamma (gamma);
}

void
flashback_monitor_manager_set_power_save_mode (FlashbackMonitorManager *manager,
                                               MetaPowerSave            mode)
{
  CARD16 state;

  switch (mode)
  {
    case META_POWER_SAVE_ON:
      state = DPMSModeOn;
      break;
    case META_POWER_SAVE_STANDBY:
      state = DPMSModeStandby;
      break;
    case META_POWER_SAVE_SUSPEND:
      state = DPMSModeSuspend;
      break;
    case META_POWER_SAVE_OFF:
      state = DPMSModeOff;
      break;
    default:
      return;
  }

  DPMSForceLevel (manager->priv->xdisplay, state);
  DPMSSetTimeouts (manager->priv->xdisplay, 0, 0, 0);
}

GBytes *
flashback_monitor_manager_read_edid (FlashbackMonitorManager *manager,
                                     MetaOutput              *output)
{
  return read_output_edid (manager, output->winsys_id);
}

void
flashback_monitor_manager_read_current_config (FlashbackMonitorManager *manager)
{
  MetaOutput *old_outputs;
  MetaCRTC *old_crtcs;
  MetaMonitorMode *old_modes;
  unsigned int n_old_outputs;
  unsigned int n_old_modes;

  /* Some implementations of read_current use the existing information
   * we have available, so don't free the old configuration until after
   * read_current finishes. */
  old_outputs = manager->outputs;
  n_old_outputs = manager->n_outputs;
  old_modes = manager->modes;
  n_old_modes = manager->n_modes;
  old_crtcs = manager->crtcs;

  manager->serial++;

  read_current_config (manager);

  free_output_array (old_outputs, n_old_outputs);
  free_mode_array (old_modes, n_old_modes);
  g_free (old_crtcs);
}

void
meta_output_parse_edid (MetaOutput *meta_output,
                        GBytes     *edid)
{
  MonitorInfo *parsed_edid;
  gsize len;

  if (!edid)
    goto out;

  parsed_edid = decode_edid (g_bytes_get_data (edid, &len));

  if (parsed_edid)
    {
      meta_output->vendor = g_strndup (parsed_edid->manufacturer_code, 4);
      if (parsed_edid->dsc_product_name[0])
        meta_output->product = g_strndup (parsed_edid->dsc_product_name, 14);
      else
        meta_output->product = g_strdup_printf ("0x%04x", (unsigned) parsed_edid->product_code);
      if (parsed_edid->dsc_serial_number[0])
        meta_output->serial = g_strndup (parsed_edid->dsc_serial_number, 14);
      else
        meta_output->serial = g_strdup_printf ("0x%08x", parsed_edid->serial_number);

      g_free (parsed_edid);
    }

 out:
  if (!meta_output->vendor)
    {
      meta_output->vendor = g_strdup ("unknown");
      meta_output->product = g_strdup ("unknown");
      meta_output->serial = g_strdup ("unknown");
    }
}

void
flashback_monitor_manager_on_hotplug (FlashbackMonitorManager *manager)
{
  gboolean applied_config = FALSE;

  /* If the monitor has hotplug_mode_update (which is used by VMs), don't bother
   * applying our stored configuration, because it's likely the user just resizing
   * the window.
   */
  if (!flashback_monitor_manager_has_hotplug_mode_update (manager))
    {
      if (flashback_monitor_config_apply_stored (manager->monitor_config))
        applied_config = TRUE;
    }

  /* If we haven't applied any configuration, apply the default configuration. */
  if (!applied_config)
    flashback_monitor_config_make_default (manager->monitor_config);
}

MetaOutput *
flashback_monitor_manager_get_outputs (FlashbackMonitorManager *manager,
                                       unsigned int            *n_outputs)
{
  *n_outputs = manager->n_outputs;
  return manager->outputs;
}

void
flashback_monitor_manager_get_resources (FlashbackMonitorManager  *manager,
                                         MetaMonitorMode         **modes,
                                         unsigned int             *n_modes,
                                         MetaCRTC                **crtcs,
                                         unsigned int             *n_crtcs,
                                         MetaOutput              **outputs,
                                         unsigned int             *n_outputs)
{
  if (modes)
    {
      *modes = manager->modes;
      *n_modes = manager->n_modes;
    }
  if (crtcs)
    {
      *crtcs = manager->crtcs;
      *n_crtcs = manager->n_crtcs;
    }
  if (outputs)
    {
      *outputs = manager->outputs;
      *n_outputs = manager->n_outputs;
    }
}

void
flashback_monitor_manager_get_screen_limits (FlashbackMonitorManager *manager,
                                             int                     *width,
                                             int                     *height)
{
  *width = manager->max_screen_width;
  *height = manager->max_screen_height;
}

gint
flashback_monitor_manager_get_monitor_for_output (FlashbackMonitorManager *manager,
                                                  guint                    id)
{
  MetaOutput *output;
  guint i;

  g_return_val_if_fail (FLASHBACK_IS_MONITOR_MANAGER (manager), -1);
  g_return_val_if_fail (id < manager->n_outputs, -1);

  output = &manager->outputs[id];
  if (!output || !output->crtc)
    return -1;

  for (i = 0; i < manager->n_monitor_infos; i++)
    if (rectangle_contains_rect (&manager->monitor_infos[i].rect, &output->crtc->rect))
      return i;

  return -1;
}

void
flashback_monitor_manager_rebuild_derived (FlashbackMonitorManager *manager)
{
  MetaMonitorInfo *old_monitor_infos;
  unsigned old_n_monitor_infos;
  unsigned i, j;

  old_monitor_infos = manager->monitor_infos;
  old_n_monitor_infos = manager->n_monitor_infos;

  if (manager->in_init)
    return;

  make_logical_config (manager);

  for (i = 0; i < old_n_monitor_infos; i++)
    {
      gboolean delete_mon = TRUE;

      for (j = 0; j < manager->n_monitor_infos; j++)
        {
          if (manager->monitor_infos[j].monitor_winsys_xid == old_monitor_infos[i].monitor_winsys_xid)
            {
              delete_mon = FALSE;
              break;
            }
        }

      if (delete_mon)
        remove_monitor (manager, old_monitor_infos[i].monitor_winsys_xid);
    }

  g_signal_emit_by_name (manager->priv->display_config, "monitors-changed");

  g_free (old_monitor_infos);
}
