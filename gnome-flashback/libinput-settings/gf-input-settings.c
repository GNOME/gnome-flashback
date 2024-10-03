/*
 * Copyright (C) 2014 Red Hat, Inc.
 * Copyright (C) 2016-2019 Alberts Muktupāvels
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

#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdesktop-enums.h>
#include <string.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>

#include "backends/gf-logical-monitor-private.h"
#include "backends/gf-monitor-manager-private.h"
#include "backends/gf-monitor-private.h"
#include "gf-input-settings.h"

typedef void (* ConfigBoolFunc)   (GfInputSettings *settings,
                                   GdkDevice       *device,
                                   gboolean         value);

typedef void (* ConfigDoubleFunc) (GfInputSettings *settings,
                                   GdkDevice       *device,
                                   gdouble          value);

typedef void (* ConfigUintFunc)   (GfInputSettings *settings,
                                   GdkDevice       *device,
                                   guint            value);

typedef struct
{
  GdkDevice       *device;
  GSettings       *gsettings;
  GfInputSettings *settings;
} DeviceMappingInfo;

struct _GfInputSettings
{
  GObject           parent;

  Display          *xdisplay;

  int               xkb_event_base;
  int               xkb_error_base;

  GdkSeat          *seat;

  GfMonitorManager *monitor_manager;
  gulong            monitors_changed_id;

  GSettings        *mouse;
  GSettings        *touchpad;
  GSettings        *trackball;
  GSettings        *keyboard;

  GHashTable       *mappable_devices;
};

G_DEFINE_TYPE (GfInputSettings, gf_input_settings, G_TYPE_OBJECT)

enum
{
  SCROLL_METHOD_FIELD_2FG,
  SCROLL_METHOD_FIELD_EDGE,
  SCROLL_METHOD_FIELD_BUTTON,

  SCROLL_METHOD_NUM_FIELDS
};

static void *
get_property (GfInputSettings *settings,
              GdkDevice       *device,
              const gchar     *property,
              Atom             type,
              gint             format,
              gulong           nitems)
{
  Atom property_atom;
  gint device_id;
  GdkDisplay *display;
  gint rc;
  Atom type_ret;
  gint format_ret;
  gulong nitems_ret;
  gulong bytes_after_ret;
  guchar *data_ret;

  property_atom = XInternAtom (settings->xdisplay, property, True);
  if (!property_atom)
    return NULL;

  device_id = gdk_x11_device_get_id (device);
  data_ret = NULL;

  display = gdk_display_get_default ();
  gdk_x11_display_error_trap_push (display);

  rc = XIGetProperty (settings->xdisplay, device_id, property_atom,
                      0, 10, False, type, &type_ret, &format_ret,
                      &nitems_ret, &bytes_after_ret, &data_ret);

  gdk_x11_display_error_trap_pop_ignored (display);

  if (rc == Success && type_ret == type && format_ret == format && nitems_ret >= nitems)
    return data_ret;

  if (data_ret)
    XFree (data_ret);

  return NULL;
}

static void
change_property (GfInputSettings *settings,
                 GdkDevice       *device,
                 const gchar     *property,
                 Atom             type,
                 gint             format,
                 void            *data,
                 gulong           nitems)
{
  Atom property_atom;
  gint device_id;
  guchar *data_ret;

  property_atom = XInternAtom (settings->xdisplay, property, True);
  if (!property_atom)
    return;

  device_id = gdk_x11_device_get_id (device);
  data_ret = NULL;

  data_ret = get_property (settings, device, property, type, format, nitems);
  if (!data_ret)
    return;

  XIChangeProperty (settings->xdisplay, device_id, property_atom, type,
                    format, XIPropModeReplace, data, nitems);
  XFree (data_ret);
}

static GList *
get_devices (GfInputSettings *settings,
             GdkInputSource   source)
{
  const GList *devices;
  const GList *l;
  GList *list;

  devices = gdk_seat_get_slaves (settings->seat, GDK_SEAT_CAPABILITY_ALL);
  list = NULL;

  for (l = devices; l; l = l->next)
    {
      GdkDevice *device;

      device = GDK_DEVICE (l->data);

      if (gdk_device_get_source (device) != source ||
          gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER)
        continue;

      list = g_list_prepend (list, device);
    }

  return list;
}

static void
device_set_bool_setting (GfInputSettings *settings,
                         GdkDevice       *device,
                         ConfigBoolFunc   func,
                         gboolean         value)
{
  func (settings, device, value);
}

static void
set_bool_setting (GfInputSettings *settings,
                  GdkInputSource   source,
                  ConfigBoolFunc   func,
                  gboolean         value)
{
  GList *devices;
  GList *d;

  devices = get_devices (settings, source);

  for (d = devices; d; d = d->next)
    device_set_bool_setting (settings, d->data, func, value);

  g_list_free (devices);
}

static void
device_set_double_setting (GfInputSettings  *settings,
                           GdkDevice        *device,
                           ConfigDoubleFunc  func,
                           gdouble           value)
{
  func (settings, device, value);
}

static void
set_double_setting (GfInputSettings  *settings,
                    GdkInputSource    source,
                    ConfigDoubleFunc  func,
                    gdouble           value)
{
  GList *devices;
  GList *d;

  devices = get_devices (settings, source);

  for (d = devices; d; d = d->next)
    device_set_double_setting (settings, d->data, func, value);

  g_list_free (devices);
}

static void
device_set_uint_setting (GfInputSettings *settings,
                         GdkDevice       *device,
                         ConfigUintFunc   func,
                         guint            value)
{
  func (settings, device, value);
}

static void
set_uint_setting (GfInputSettings *settings,
                  GdkInputSource   source,
                  ConfigUintFunc   func,
                  guint            value)
{
  GList *devices;
  GList *d;

  devices = get_devices (settings, source);

  for (d = devices; d; d = d->next)
    device_set_uint_setting (settings, d->data, func, value);

  g_list_free (devices);
}

static void
set_speed (GfInputSettings *settings,
           GdkDevice       *device,
           gdouble          speed)
{
  gfloat value = speed;

  change_property (settings, device, "libinput Accel Speed",
                   XInternAtom (settings->xdisplay, "FLOAT", False),
                   32, &value, 1);
}

static void
set_left_handed (GfInputSettings *settings,
                 GdkDevice       *device,
                 gboolean         enabled)
{
  guchar value = (enabled) ? 1 : 0;

  change_property (settings, device, "libinput Left Handed Enabled",
                   XA_INTEGER, 8, &value, 1);
}

static void
set_invert_scroll (GfInputSettings *settings,
                   GdkDevice       *device,
                   gboolean         inverted)
{
  guchar value = (inverted) ? 1 : 0;

  change_property (settings, device, "libinput Natural Scrolling Enabled",
                   XA_INTEGER, 8, &value, 1);
}

static void
set_accel_profile (GfInputSettings             *settings,
                   GdkDevice                   *device,
                   GDesktopPointerAccelProfile  profile)
{
  guchar *available;
  guchar *defaults;
  guchar values[2] = { 0 }; /* adaptive, flat */

  available = get_property (settings, device,
                            "libinput Accel Profiles Available",
                            XA_INTEGER, 8, 2);

  defaults = get_property (settings, device,
                           "libinput Accel Profile Enabled Default",
                           XA_INTEGER, 8, 2);

  if (!available || !defaults)
    {
      if (available)
        XFree (available);

      if (defaults)
        XFree (defaults);

      return;
    }

  memcpy (values, defaults, 2);

  switch (profile)
    {
      case G_DESKTOP_POINTER_ACCEL_PROFILE_FLAT:
        values[0] = 0;
        values[1] = 1;
        break;
      case G_DESKTOP_POINTER_ACCEL_PROFILE_ADAPTIVE:
        values[0] = 1;
        values[1] = 0;
        break;
      case G_DESKTOP_POINTER_ACCEL_PROFILE_DEFAULT:
        break;
      default:
        g_warn_if_reached ();
        break;
    }

  change_property (settings, device, "libinput Accel Profile Enabled",
                   XA_INTEGER, 8, &values, 2);

  if (available)
    XFree (available);

  if (defaults)
    XFree (defaults);
}

static void
set_tap_enabled (GfInputSettings *settings,
                 GdkDevice       *device,
                 gboolean         enabled)
{
  guchar value = (enabled) ? 1 : 0;

  change_property (settings, device, "libinput Tapping Enabled",
                   XA_INTEGER, 8, &value, 1);
}

static void
set_disable_while_typing (GfInputSettings *settings,
                          GdkDevice       *device,
                          gboolean         enabled)
{
  guchar value = (enabled) ? 1 : 0;

  change_property (settings, device, "libinput Disable While Typing Enabled",
                   XA_INTEGER, 8, &value, 1);
}

static void
set_send_events (GfInputSettings          *settings,
                 GdkDevice                *device,
                 GDesktopDeviceSendEvents  mode)
{
  guchar *available;
  guchar values[2] = { 0 }; /* disabled, disabled-on-external-mouse */

  available = get_property (settings, device,
                            "libinput Send Events Modes Available",
                            XA_INTEGER, 8, 2);

  if (!available)
    return;

  switch (mode)
    {
      case G_DESKTOP_DEVICE_SEND_EVENTS_DISABLED:
        values[0] = 1;
        break;
      case G_DESKTOP_DEVICE_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE:
        values[1] = 1;
        break;
      case G_DESKTOP_DEVICE_SEND_EVENTS_ENABLED:
      default:
        break;
    }

  if ((values[0] && !available[0]) || (values[1] && !available[1]))
    g_warning ("Device '%s' does not support sendevents mode %d",
               gdk_device_get_name (device), mode);
  else
    change_property (settings, device, "libinput Send Events Mode Enabled",
                     XA_INTEGER, 8, &values, 2);

  XFree (available);
}

static void
set_edge_scroll (GfInputSettings *settings,
                 GdkDevice       *device,
                 gboolean         edge_scroll_enabled)
{
  guchar values[SCROLL_METHOD_NUM_FIELDS] = { 0 }; /* 2fg, edge, button. The last value is unused */
  guchar *available = NULL;
  guchar *current = NULL;

  available = get_property (settings, device,
                            "libinput Scroll Methods Available",
                            XA_INTEGER, 8, SCROLL_METHOD_NUM_FIELDS);

  if (!available || !available[SCROLL_METHOD_FIELD_EDGE])
    {
      if (available)
        XFree (available);

      return;
    }

  current = get_property (settings, device,
                          "libinput Scroll Method Enabled",
                          XA_INTEGER, 8, SCROLL_METHOD_NUM_FIELDS);

  if (!current)
    {
      XFree (available);

      return;
    }

  memcpy (values, current, SCROLL_METHOD_NUM_FIELDS);

  values[SCROLL_METHOD_FIELD_EDGE] = !!edge_scroll_enabled;
  change_property (settings, device, "libinput Scroll Method Enabled",
                   XA_INTEGER, 8, &values, SCROLL_METHOD_NUM_FIELDS);

  XFree (available);
  XFree (current);
}

static void
set_two_finger_scroll (GfInputSettings *settings,
                       GdkDevice       *device,
                       gboolean         two_finger_scroll_enabled)
{
  guchar values[SCROLL_METHOD_NUM_FIELDS] = { 0 }; /* 2fg, edge, button. The last value is unused */
  guchar *available = NULL;
  guchar *current = NULL;

  available = get_property (settings, device,
                            "libinput Scroll Methods Available",
                            XA_INTEGER, 8, SCROLL_METHOD_NUM_FIELDS);

  if (!available || !available[SCROLL_METHOD_FIELD_2FG])
    {
      if (available)
        XFree (available);

      return;
    }

  current = get_property (settings, device,
                          "libinput Scroll Method Enabled",
                          XA_INTEGER, 8, SCROLL_METHOD_NUM_FIELDS);

  if (!current)
    {
      XFree (available);

      return;
    }

  memcpy (values, current, SCROLL_METHOD_NUM_FIELDS);

  values[SCROLL_METHOD_FIELD_2FG] = !!two_finger_scroll_enabled;
  change_property (settings, device, "libinput Scroll Method Enabled",
                   XA_INTEGER, 8, &values, SCROLL_METHOD_NUM_FIELDS);

  XFree (available);
  XFree (current);
}

static void
set_click_method (GfInputSettings             *settings,
                  GdkDevice                   *device,
                  GDesktopTouchpadClickMethod  mode)
{
  guchar *available;
  guchar *defaults;
  guchar values[2] = { 0 }; /* buttonareas, clickfinger */

  available = get_property (settings, device,
                            "libinput Click Methods Available",
                            XA_INTEGER, 8, 2);

  if (!available)
    return;

  switch (mode)
    {
      case G_DESKTOP_TOUCHPAD_CLICK_METHOD_DEFAULT:
        defaults = get_property (settings, device,
                                 "libinput Click Method Enabled Default",
                                 XA_INTEGER, 8, 2);

        if (!defaults)
          break;

        memcpy (values, defaults, 2);
        XFree (defaults);
        break;
      case G_DESKTOP_TOUCHPAD_CLICK_METHOD_NONE:
        break;
      case G_DESKTOP_TOUCHPAD_CLICK_METHOD_AREAS:
        values[0] = 1;
        break;
      case G_DESKTOP_TOUCHPAD_CLICK_METHOD_FINGERS:
        values[1] = 1;
        break;
      default:
        g_assert_not_reached ();
        return;
  }

  if ((values[0] && !available[0]) || (values[1] && !available[1]))
    g_warning ("Device '%s' does not support click method %d",
               gdk_device_get_name (device), mode);
  else
    change_property (settings, device, "libinput Click Method Enabled",
                     XA_INTEGER, 8, &values, 2);

  XFree (available);
}

static void
set_scroll_button (GfInputSettings *settings,
                   GdkDevice       *device,
                   guint            button)
{
  change_property (settings, device, "libinput Button Scrolling Button",
                   XA_INTEGER, 32, &button, 1);
}

static gboolean
device_is_trackball (GdkDevice *device)
{
  gboolean is_trackball;
  gchar *name;

  if (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER)
    return FALSE;

  name = g_ascii_strdown (gdk_device_get_name (device), -1);
  is_trackball = strstr (name, "trackball") != NULL;
  g_free (name);

  return is_trackball;
}

static void
update_touchpad_left_handed (GfInputSettings *settings,
                             GdkDevice       *device)
{
  GDesktopTouchpadHandedness handedness;
  gboolean enabled;

  if (device && gdk_device_get_source (device) != GDK_SOURCE_TOUCHPAD)
    return;

  handedness = g_settings_get_enum (settings->touchpad, "left-handed");
  enabled = FALSE;

  switch (handedness)
    {
      case G_DESKTOP_TOUCHPAD_HANDEDNESS_RIGHT:
        enabled = FALSE;
        break;
      case G_DESKTOP_TOUCHPAD_HANDEDNESS_LEFT:
        enabled = TRUE;
        break;
      case G_DESKTOP_TOUCHPAD_HANDEDNESS_MOUSE:
        enabled = g_settings_get_boolean (settings->mouse, "left-handed");
        break;
      default:
        g_assert_not_reached ();
        break;
    }

  if (device)
    {
      device_set_bool_setting (settings, device,
                               set_left_handed, enabled);
    }
  else
    {
      set_bool_setting (settings, GDK_SOURCE_TOUCHPAD,
                        set_left_handed, enabled);
    }
}

static void
update_mouse_left_handed (GfInputSettings *settings,
                          GdkDevice       *device)
{
  gboolean enabled;

  if (device && gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
    return;

  enabled = g_settings_get_boolean (settings->mouse, "left-handed");

  if (device)
    {
      device_set_bool_setting (settings, device, set_left_handed, enabled);
    }
  else
    {
      GDesktopTouchpadHandedness handedness;

      set_bool_setting (settings, GDK_SOURCE_MOUSE, set_left_handed, enabled);

      handedness = g_settings_get_enum (settings->touchpad, "left-handed");

      /* Also update touchpads if they're following mouse settings */
      if (handedness == G_DESKTOP_TOUCHPAD_HANDEDNESS_MOUSE)
        update_touchpad_left_handed (settings, NULL);
    }
}

static void
update_mouse_speed (GfInputSettings *settings,
                    GdkDevice       *device)
{
  gdouble value;

  if (device &&
      gdk_device_get_source (device) != GDK_SOURCE_MOUSE &&
      gdk_device_get_source (device) != GDK_SOURCE_TRACKPOINT)
    return;

  value = g_settings_get_double (settings->mouse, "speed");

  if (device)
    {
      device_set_double_setting (settings, device, set_speed, value);
    }
  else
    {
      set_double_setting (settings, GDK_SOURCE_MOUSE, set_speed, value);
      set_double_setting (settings, GDK_SOURCE_TRACKPOINT, set_speed, value);
    }
}

static void
update_mouse_natural_scroll (GfInputSettings *settings,
                             GdkDevice       *device)
{
  gboolean value;

  if (device &&
      gdk_device_get_source (device) != GDK_SOURCE_MOUSE &&
      gdk_device_get_source (device) != GDK_SOURCE_TRACKPOINT)
    return;

  value = g_settings_get_boolean (settings->mouse, "natural-scroll");

  if (device)
    {
      device_set_bool_setting (settings, device, set_invert_scroll, value);
    }
  else
    {
      set_bool_setting (settings, GDK_SOURCE_MOUSE,
                        set_invert_scroll, value);
      set_bool_setting (settings, GDK_SOURCE_TRACKPOINT,
                        set_invert_scroll, value);
    }
}

static void
update_mouse_accel_profile (GfInputSettings *settings,
                            GdkDevice       *device)
{
  GDesktopPointerAccelProfile profile;

  if (device && gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
    return;

  profile = g_settings_get_enum (settings->mouse, "accel-profile");

  if (device)
    {
      device_set_uint_setting (settings, device, set_accel_profile, profile);
    }
  else
    {
      set_uint_setting (settings, GDK_SOURCE_MOUSE, set_accel_profile, profile);
    }
}

static void
update_touchpad_speed (GfInputSettings *settings,
                       GdkDevice       *device)
{
  gdouble value;

  if (device && gdk_device_get_source (device) != GDK_SOURCE_TOUCHPAD)
    return;

  value = g_settings_get_double (settings->touchpad, "speed");

  if (device)
    {
      device_set_double_setting (settings, device, set_speed, value);
    }
  else
    {
      set_double_setting (settings, GDK_SOURCE_TOUCHPAD, set_speed, value);
    }
}

static void
update_touchpad_natural_scroll (GfInputSettings *settings,
                                GdkDevice       *device)
{
  gboolean value;

  if (device && gdk_device_get_source (device) != GDK_SOURCE_TOUCHPAD)
    return;

  value = g_settings_get_boolean (settings->touchpad, "natural-scroll");

  if (device)
    {
      device_set_bool_setting (settings, device, set_invert_scroll, value);
    }
  else
    {
      set_bool_setting (settings, GDK_SOURCE_TOUCHPAD,
                        set_invert_scroll, value);
    }
}

static void
update_touchpad_tap_enabled (GfInputSettings *settings,
                             GdkDevice       *device)
{
  gboolean enabled;

  if (device && gdk_device_get_source (device) != GDK_SOURCE_TOUCHPAD)
    return;

  enabled = g_settings_get_boolean (settings->touchpad, "tap-to-click");

  if (device)
    {
      device_set_bool_setting (settings, device,
                               set_tap_enabled, enabled);
    }
  else
    {
      set_bool_setting (settings, GDK_SOURCE_TOUCHPAD,
                        set_tap_enabled, enabled);
    }
}

static void
update_touchpad_disable_while_typing (GfInputSettings *settings,
                                      GdkDevice       *device)
{
  gboolean enabled;

  if (device && gdk_device_get_source (device) != GDK_SOURCE_TOUCHPAD)
    return;

  enabled = g_settings_get_boolean (settings->touchpad, "disable-while-typing");

  if (device)
    {
      device_set_bool_setting (settings, device,
                               set_disable_while_typing, enabled);
    }
  else
    {
      set_bool_setting (settings, GDK_SOURCE_TOUCHPAD,
                        set_disable_while_typing, enabled);
    }
}

static void
update_touchpad_send_events (GfInputSettings *settings,
                             GdkDevice       *device)
{
  GDesktopDeviceSendEvents mode;

  if (device && gdk_device_get_source (device) != GDK_SOURCE_TOUCHPAD)
    return;

  mode = g_settings_get_enum (settings->touchpad, "send-events");

  if (device)
    {
      device_set_uint_setting (settings, device,
                               set_send_events, mode);
    }
  else
    {
      set_uint_setting (settings, GDK_SOURCE_TOUCHPAD,
                        set_send_events, mode);
    }
}

static void
update_touchpad_edge_scroll (GfInputSettings *settings,
                             GdkDevice       *device)
{
  gboolean edge_scroll_enabled;
  gboolean two_finger_scroll_enabled;

  if (device && gdk_device_get_source (device) != GDK_SOURCE_TOUCHPAD)
    return;

  edge_scroll_enabled = g_settings_get_boolean (settings->touchpad,
                                                "edge-scrolling-enabled");

  two_finger_scroll_enabled = g_settings_get_boolean (settings->touchpad,
                                                      "two-finger-scrolling-enabled");

  /* If both are enabled we prefer two finger. */
  if (edge_scroll_enabled && two_finger_scroll_enabled)
    edge_scroll_enabled = FALSE;

  if (device)
    {
      device_set_bool_setting (settings, device,
                               set_edge_scroll, edge_scroll_enabled);
    }
  else
    {
      set_bool_setting (settings, GDK_SOURCE_TOUCHPAD,
                        set_edge_scroll, edge_scroll_enabled);
    }
}

static void
update_touchpad_two_finger_scroll (GfInputSettings *settings,
                                   GdkDevice       *device)
{
  gboolean two_finger_scroll_enabled;

  if (device && gdk_device_get_source (device) != GDK_SOURCE_TOUCHPAD)
    return;

  two_finger_scroll_enabled = g_settings_get_boolean (settings->touchpad,
                                                      "two-finger-scrolling-enabled");

  /* Disable edge since they can't both be set. */
  if (two_finger_scroll_enabled)
    update_touchpad_edge_scroll (settings, device);

  if (device)
    {
      device_set_bool_setting (settings, device,
                               set_two_finger_scroll, two_finger_scroll_enabled);
    }
  else
    {
      set_bool_setting (settings, GDK_SOURCE_TOUCHPAD,
                        set_two_finger_scroll, two_finger_scroll_enabled);
    }

  /* Edge might have been disabled because two finger was on. */
  if (!two_finger_scroll_enabled)
    update_touchpad_edge_scroll (settings, device);
}

static void
update_touchpad_click_method (GfInputSettings *settings,
                              GdkDevice       *device)
{
  GDesktopTouchpadClickMethod method;

  if (device && gdk_device_get_source (device) != GDK_SOURCE_TOUCHPAD)
    return;

  method = g_settings_get_enum (settings->touchpad, "click-method");

  if (device)
    {
      device_set_uint_setting (settings, device,
                               set_click_method, method);
    }
  else
    {
      set_uint_setting (settings, GDK_SOURCE_TOUCHPAD,
                        set_click_method, method);
    }
}

static void
update_trackball_scroll_button (GfInputSettings *settings,
                                GdkDevice       *device)
{
  guint button;

  if (device && !device_is_trackball (device))
    return;

  /* This key is 'i' in the schema but it also specifies a minimum
   * range of 0 so the cast here is safe.
   */
  button = (guint) g_settings_get_int (settings->trackball,
                                       "scroll-wheel-emulation-button");

  if (device)
    {
      device_set_uint_setting (settings, device, set_scroll_button, button);
    }
  else
    {
      const GList *devices;
      const GList *l;

      devices = gdk_seat_get_slaves (settings->seat, GDK_SEAT_CAPABILITY_ALL);

      for (l = devices; l; l = l->next)
        {
          device = GDK_DEVICE (l->data);

          if (!device_is_trackball (device))
            continue;

          device_set_uint_setting (settings, device, set_scroll_button, button);
        }
    }
}

static void
update_trackball_accel_profile (GfInputSettings *settings,
                                GdkDevice       *device)
{
  GDesktopPointerAccelProfile profile;

  if (device && !device_is_trackball (device))
    return;

  profile = g_settings_get_enum (settings->trackball, "accel-profile");

  if (device)
    {
      device_set_uint_setting (settings, device, set_accel_profile, profile);
    }
  else
    {
      const GList *devices;
      const GList *l;

      devices = gdk_seat_get_slaves (settings->seat, GDK_SEAT_CAPABILITY_ALL);

      for (l = devices; l; l = l->next)
        {
          device = GDK_DEVICE (l->data);

          if (!device_is_trackball (device))
            continue;

          device_set_uint_setting (settings, device, set_accel_profile, profile);
        }
    }
}

static void
update_keyboard_repeat (GfInputSettings *settings)
{
  gboolean repeat;
  guint delay;
  guint interval;

  repeat = g_settings_get_boolean (settings->keyboard, "repeat");
  delay = g_settings_get_uint (settings->keyboard, "delay");
  interval = g_settings_get_uint (settings->keyboard, "repeat-interval");

  if (repeat)
    {
      XAutoRepeatOn (settings->xdisplay);
      XkbSetAutoRepeatRate (settings->xdisplay, XkbUseCoreKbd, delay, interval);
    }
  else
    {
      XAutoRepeatOff (settings->xdisplay);
    }
}

static void
maybe_save_numlock_state (GfInputSettings *self,
                          gboolean         numlock_state)
{
  if (!g_settings_get_boolean (self->keyboard, "remember-numlock-state"))
    return;

  if (g_settings_get_boolean (self->keyboard, "numlock-state") == numlock_state)
    return;

  g_settings_set_boolean (self->keyboard, "numlock-state", numlock_state);
}

static void
maybe_restore_numlock_state (GfInputSettings *self)
{
  unsigned int numlock_mask;
  unsigned int affect;
  unsigned int values;

  if (self->xkb_event_base == -1)
    return;

  if (!g_settings_get_boolean (self->keyboard, "remember-numlock-state"))
    return;

  numlock_mask = XkbKeysymToModifiers (self->xdisplay, XK_Num_Lock);
  affect = values = 0;

  affect = numlock_mask;
  if (g_settings_get_boolean (self->keyboard, "numlock-state"))
    values |= numlock_mask;

  XkbLockModifiers (self->xdisplay, XkbUseCoreKbd, affect, values);
}

static GdkFilterReturn
xkb_event_filter_cb (GdkXEvent *gdk_x_event,
                     GdkEvent  *gdk_event,
                     void      *user_data)
{
  GfInputSettings *self;
  XEvent *x_event;
  XkbEvent *xkb_event;

  self = GF_INPUT_SETTINGS (user_data);
  x_event = (XEvent *) gdk_x_event;

  if (x_event->type != self->xkb_event_base)
    return GDK_FILTER_CONTINUE;

  xkb_event = (XkbEvent *) gdk_x_event;

  if (xkb_event->any.xkb_type != XkbStateNotify)
    return GDK_FILTER_CONTINUE;

  if (xkb_event->state.changed & XkbModifierLockMask)
    {
      unsigned int numlock_mask;
      gboolean numlock_state;

      numlock_mask = XkbKeysymToModifiers (self->xdisplay, XK_Num_Lock);
      numlock_state = (xkb_event->state.locked_mods & numlock_mask) == numlock_mask;

      maybe_save_numlock_state (self, numlock_state);
    }

  return GDK_FILTER_CONTINUE;
}

static void
settings_changed_cb (GSettings       *gsettings,
                     const gchar     *key,
                     GfInputSettings *settings)
{
  if (gsettings == settings->mouse)
    {
      if (strcmp (key, "left-handed") == 0)
        update_mouse_left_handed (settings, NULL);
      else if (strcmp (key, "speed") == 0)
        update_mouse_speed (settings, NULL);
      else if (strcmp (key, "natural-scroll") == 0)
        update_mouse_natural_scroll (settings, NULL);
      else if (strcmp (key, "accel-profile") == 0)
        update_mouse_accel_profile (settings, NULL);
    }
  else if (gsettings == settings->touchpad)
    {
      if (strcmp (key, "left-handed") == 0)
        update_touchpad_left_handed (settings, NULL);
      else if (strcmp (key, "speed") == 0)
        update_touchpad_speed (settings, NULL);
      else if (strcmp (key, "natural-scroll") == 0)
        update_touchpad_natural_scroll (settings, NULL);
      else if (strcmp (key, "tap-to-click") == 0)
        update_touchpad_tap_enabled (settings, NULL);
      else if (strcmp (key, "disable-while-typing") == 0)
        update_touchpad_disable_while_typing (settings, NULL);
      else if (strcmp (key, "send-events") == 0)
        update_touchpad_send_events (settings, NULL);
      else if (strcmp (key, "edge-scrolling-enabled") == 0)
        update_touchpad_edge_scroll (settings, NULL);
      else if (strcmp (key, "two-finger-scrolling-enabled") == 0)
        update_touchpad_two_finger_scroll (settings, NULL);
      else if (strcmp (key, "click-method") == 0)
        update_touchpad_click_method (settings, NULL);
    }
  else if (gsettings == settings->trackball)
    {
      if (strcmp (key, "scroll-wheel-emulation-button") == 0)
        update_trackball_scroll_button (settings, NULL);
      else if (strcmp (key, "accel-profile") == 0)
        update_trackball_accel_profile (settings, NULL);
    }
  else if (gsettings == settings->keyboard)
    {
      if (strcmp (key, "repeat") == 0 ||
          strcmp (key, "repeat-interval") == 0 ||
          strcmp (key, "delay") == 0)
        update_keyboard_repeat (settings);
    }
}

static GfMonitor *
logical_monitor_find_monitor (GfLogicalMonitor *logical_monitor,
                              const gchar      *vendor,
                              const gchar      *product,
                              const gchar      *serial)
{
  GList *monitors;
  GList *l;

  monitors = gf_logical_monitor_get_monitors (logical_monitor);

  for (l = monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;

      if (g_strcmp0 (gf_monitor_get_vendor (monitor), vendor) == 0 &&
          g_strcmp0 (gf_monitor_get_product (monitor), product) == 0 &&
          g_strcmp0 (gf_monitor_get_serial (monitor), serial) == 0)
        return monitor;
    }

  return NULL;
}

static void
find_monitor (GfInputSettings   *settings,
              GSettings         *gsettings,
              GdkDevice         *device,
              GfMonitor        **out_monitor,
              GfLogicalMonitor **out_logical_monitor)
{
  gchar **edid;
  guint n_values;
  GfMonitorManager *monitor_manager;
  GList *logical_monitors;
  GfMonitor *monitor;
  GList *l;

  if (!settings->monitor_manager)
    return;

  edid = g_settings_get_strv (gsettings, "output");
  n_values = g_strv_length (edid);

  if (n_values != 3)
    {
      g_warning ("EDID configuration for device '%s' is incorrect, "
                 "must have 3 values", gdk_device_get_name (device));

      g_strfreev (edid);
      return;
    }

  if (!*edid[0] && !*edid[1] && !*edid[2])
    {
      g_strfreev (edid);
      return;
    }

  monitor_manager = settings->monitor_manager;
  logical_monitors = gf_monitor_manager_get_logical_monitors (monitor_manager);

  for (l = logical_monitors; l; l = l->next)
    {
      GfLogicalMonitor *logical_monitor = l->data;

      monitor = logical_monitor_find_monitor (logical_monitor,
                                              edid[0], edid[1], edid[2]);

      if (monitor)
        {
          if (out_monitor)
            *out_monitor = monitor;

          if (out_logical_monitor)
            *out_logical_monitor = logical_monitor;

          break;
        }
    }

  g_strfreev (edid);
}

static void
update_device_display (GfInputSettings *settings,
                       GSettings       *gsettings,
                       GdkDevice       *device)
{
  GdkInputSource source;
  GfMonitor *monitor;
  GfLogicalMonitor *logical_monitor;
  gfloat matrix[6] = { 1, 0, 0, 0, 1, 0 };
  gfloat full_matrix[9];

  source = gdk_device_get_source (device);

  if (/*get_device_type (device) != CLUTTER_TABLET_DEVICE && */
      source != GDK_SOURCE_PEN &&
      source != GDK_SOURCE_ERASER &&
      source != GDK_SOURCE_TOUCHSCREEN)
    return;

  monitor = NULL;
  logical_monitor = NULL;

  /* If mapping is relative, the device can move on all displays */
  if (source == GDK_SOURCE_TOUCHSCREEN /* ||
      get_mapping_mode (device) == CLUTTER_INPUT_DEVICE_MAPPING_ABSOLUTE*/)
    find_monitor (settings, gsettings, device, &monitor, &logical_monitor);

  if (monitor)
    {
      gf_monitor_manager_get_monitor_matrix (settings->monitor_manager,
                                             monitor, logical_monitor, matrix);
    }

  full_matrix[0] = matrix[0];
  full_matrix[1] = matrix[1];
  full_matrix[2] = matrix[2];
  full_matrix[3] = matrix[3];
  full_matrix[4] = matrix[4];
  full_matrix[5] = matrix[5];
  full_matrix[6] = 0;
  full_matrix[7] = 0;
  full_matrix[8] = 1;

  change_property (settings, device, "Coordinate Transformation Matrix",
                   XInternAtom (settings->xdisplay, "FLOAT", False),
                   32, &full_matrix, 9);
}

static void
monitors_changed_cb (GfMonitorManager *monitor_manager,
                     GfInputSettings  *settings)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_hash_table_iter_init (&iter, settings->mappable_devices);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GSettings *gsettings;
      GdkDevice *device;

      gsettings = G_SETTINGS (value);
      device = GDK_DEVICE (key);

      update_device_display (settings, gsettings, device);
    }
}

static GSettings *
lookup_device_settings (GdkDevice *device)
{
  GdkInputSource source;
  const gchar *group;
  const gchar *schema;
  const gchar *vendor;
  const gchar *product;
  gchar *path;
  GSettings *gsettings;

  source = gdk_device_get_source (device);

  if (source == GDK_SOURCE_TOUCHSCREEN)
    {
      group = "touchscreens";
      schema = "org.gnome.desktop.peripherals.touchscreen";
    }
  else if (/* type == CLUTTER_TABLET_DEVICE || */
           source == GDK_SOURCE_PEN ||
           source == GDK_SOURCE_ERASER ||
           source == GDK_SOURCE_CURSOR)
    {
      group = "tablets";
      schema = "org.gnome.desktop.peripherals.tablet";
    }
  else
    {
      return NULL;
    }

  vendor = gdk_device_get_vendor_id (device);
  product = gdk_device_get_product_id (device);

  path = g_strdup_printf ("/org/gnome/desktop/peripherals/%s/%s:%s/",
                          group, vendor, product);

  gsettings = g_settings_new_with_path (schema, path);
  g_free (path);

  return gsettings;
}

static void
mapped_device_changed_cb (GSettings         *gsettings,
                          const gchar       *key,
                          DeviceMappingInfo *info)
{
  if (strcmp (key, "output") == 0)
    update_device_display (info->settings, gsettings, info->device);
}

static void
free_device_mapping_info (gpointer  data,
                          GClosure *closure)
{
  g_free (data);
}

static void
check_add_mappable_device (GfInputSettings *settings,
                           GdkDevice       *device)
{
  GSettings *gsettings;
  DeviceMappingInfo *info;

  gsettings = lookup_device_settings (device);

  if (!gsettings)
    return;

  info = g_new0 (DeviceMappingInfo, 1);

  info->device = device;
  info->gsettings = gsettings;
  info->settings = settings;

  g_signal_connect_data (gsettings, "changed",
                         G_CALLBACK (mapped_device_changed_cb),
                         info, free_device_mapping_info, 0);

  g_hash_table_insert (settings->mappable_devices, device, gsettings);
  update_device_display (settings, gsettings, device);
}

static void
apply_device_settings (GfInputSettings *settings,
                       GdkDevice       *device)
{
  update_mouse_left_handed (settings, device);
  update_mouse_speed (settings, device);
  update_mouse_natural_scroll (settings, device);
  update_mouse_accel_profile (settings, device);

  update_touchpad_left_handed (settings, device);
  update_touchpad_speed (settings, device);
  update_touchpad_natural_scroll (settings, device);
  update_touchpad_tap_enabled (settings, device);
  update_touchpad_disable_while_typing (settings, device);
  update_touchpad_send_events (settings, device);
  update_touchpad_two_finger_scroll (settings, device);
  update_touchpad_edge_scroll (settings, device);
  update_touchpad_click_method (settings, device);

  update_trackball_scroll_button (settings, device);
  update_trackball_accel_profile (settings, device);
}

static void
device_added_cb (GdkSeat         *seat,
                 GdkDevice       *device,
                 GfInputSettings *settings)
{
  if (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER)
    return;

  apply_device_settings (settings, device);
  check_add_mappable_device (settings, device);
}

static void
device_removed_cb (GdkSeat         *seat,
                   GdkDevice       *device,
                   GfInputSettings *settings)
{
  g_hash_table_remove (settings->mappable_devices, device);
}

static void
check_mappable_devices (GfInputSettings *settings)
{
  const GList *devices;
  const GList *l;

  devices = gdk_seat_get_slaves (settings->seat, GDK_SEAT_CAPABILITY_ALL);

  for (l = devices; l; l = l->next)
    {
      GdkDevice *device;

      device = GDK_DEVICE (l->data);

      if (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER)
        continue;

      check_add_mappable_device (settings, device);
    }
}

static gboolean
check_xkb_extension (GfInputSettings *self)
{
  int xkb_opcode;
  int xkb_major;
  int xkb_minor;

  xkb_major = XkbMajorVersion;
  xkb_minor = XkbMinorVersion;

  if (!XkbQueryExtension (self->xdisplay,
                          &xkb_opcode,
                          &self->xkb_event_base,
                          &self->xkb_error_base,
                          &xkb_major,
                          &xkb_minor))
    {
      self->xkb_event_base = -1;
      self->xkb_error_base = -1;

      g_warning ("X server doesn't have the XKB extension, version %d.%d or "
                 "newer", XkbMajorVersion, XkbMinorVersion);

      return FALSE;
    }

  return TRUE;
}

static void
gf_input_settings_constructed (GObject *object)
{
  GfInputSettings *settings;

  settings = GF_INPUT_SETTINGS (object);

  G_OBJECT_CLASS (gf_input_settings_parent_class)->constructed (object);

  apply_device_settings (settings, NULL);
  update_keyboard_repeat (settings);
  maybe_restore_numlock_state (settings);
  check_mappable_devices (settings);
}

static void
gf_input_settings_dispose (GObject *object)
{
  GfInputSettings *settings;

  settings = GF_INPUT_SETTINGS (object);

  if (settings->monitors_changed_id != 0 && settings->monitor_manager)
    {
      g_signal_handler_disconnect (settings->monitor_manager, settings->monitors_changed_id);
      settings->monitors_changed_id = 0;
    }

  g_clear_object (&settings->mouse);
  g_clear_object (&settings->touchpad);
  g_clear_object (&settings->trackball);
  g_clear_object (&settings->keyboard);

  g_clear_pointer (&settings->mappable_devices, g_hash_table_destroy);

  G_OBJECT_CLASS (gf_input_settings_parent_class)->dispose (object);
}

static void
gf_input_settings_finalize (GObject *object)
{
  GfInputSettings *self;

  self = GF_INPUT_SETTINGS (object);

  gdk_window_remove_filter (NULL, xkb_event_filter_cb, self);

  G_OBJECT_CLASS (gf_input_settings_parent_class)->finalize (object);
}

static void
gf_input_settings_class_init (GfInputSettingsClass *settings_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (settings_class);

  object_class->constructed = gf_input_settings_constructed;
  object_class->dispose = gf_input_settings_dispose;
  object_class->finalize = gf_input_settings_finalize;
}

static void
gf_input_settings_init (GfInputSettings *settings)
{
  GdkDisplay *display;

  display = gdk_display_get_default ();

  settings->xdisplay = gdk_x11_display_get_xdisplay (display);

  if (check_xkb_extension (settings))
    {
      XkbSelectEventDetails (settings->xdisplay,
                             XkbUseCoreKbd,
                             XkbStateNotify,
                             XkbModifierLockMask,
                             XkbModifierLockMask);
    }

  settings->seat = gdk_display_get_default_seat (display);
  g_signal_connect (settings->seat, "device-added",
                    G_CALLBACK (device_added_cb), settings);
  g_signal_connect (settings->seat, "device-removed",
                    G_CALLBACK (device_removed_cb), settings);

  settings->mouse = g_settings_new ("org.gnome.desktop.peripherals.mouse");
  g_signal_connect (settings->mouse, "changed",
                    G_CALLBACK (settings_changed_cb), settings);

  settings->touchpad = g_settings_new ("org.gnome.desktop.peripherals.touchpad");
  g_signal_connect (settings->touchpad, "changed",
                    G_CALLBACK (settings_changed_cb), settings);

  settings->trackball = g_settings_new ("org.gnome.desktop.peripherals.trackball");
  g_signal_connect (settings->trackball, "changed",
                    G_CALLBACK (settings_changed_cb), settings);

  settings->keyboard = g_settings_new ("org.gnome.desktop.peripherals.keyboard");
  g_signal_connect (settings->keyboard, "changed",
                    G_CALLBACK (settings_changed_cb), settings);

  settings->mappable_devices = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

  gdk_window_add_filter (NULL, xkb_event_filter_cb, settings);
}

GfInputSettings *
gf_input_settings_new (void)
{
  return g_object_new (GF_TYPE_INPUT_SETTINGS, NULL);
}

void
gf_input_settings_set_monitor_manager (GfInputSettings  *settings,
                                       GfMonitorManager *monitor_manager)
{
  if (settings->monitors_changed_id != 0 && settings->monitor_manager)
    {
      g_signal_handler_disconnect (settings->monitor_manager, settings->monitors_changed_id);
      settings->monitors_changed_id = 0;
    }

  settings->monitor_manager = monitor_manager;

  settings->monitors_changed_id =
    g_signal_connect (settings->monitor_manager, "monitors-changed",
                      G_CALLBACK (monitors_changed_cb), settings);

  monitors_changed_cb (monitor_manager, settings);
}
