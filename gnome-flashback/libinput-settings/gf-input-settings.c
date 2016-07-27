/*
 * Copyright (C) 2014 Red Hat, Inc.
 * Copyright (C) 2016 Alberts Muktupāvels
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
#include <gsettings-desktop-schemas/gdesktop-enums.h>
#include <libdisplay-config/flashback-display-config.h>
#include <libdisplay-config/flashback-monitor-manager.h>
#include <string.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>

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
  GObject                  parent;

  Display                 *xdisplay;

  GdkSeat                 *seat;

  FlashbackMonitorManager *monitor_manager;
  gulong                   monitors_changed_id;

  GSettings               *mouse;
  GSettings               *touchpad;
  GSettings               *trackball;
  GSettings               *keyboard;

  GHashTable              *mappable_devices;
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

  rc = XIGetProperty (settings->xdisplay, device_id, property_atom,
                      0, 10, False, type, &type_ret, &format_ret,
                      &nitems_ret, &bytes_after_ret, &data_ret);

  if (rc == Success && type_ret == type && format_ret == format && nitems_ret >= nitems)
    {
      if (nitems_ret > nitems)
        g_warning ("Property '%s' for device '%s' returned %lu items, expected %lu",
                   property, gdk_device_get_name (device), nitems_ret, nitems);

      return data_ret;
    }

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

static GSettings *
get_settings_for_source (GfInputSettings *settings,
                         GdkInputSource   source)
{
  if (source == GDK_SOURCE_MOUSE)
    return settings->mouse;
  else if (source == GDK_SOURCE_TOUCHPAD)
    return settings->touchpad;

  return NULL;
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
set_tap_enabled (GfInputSettings *settings,
                 GdkDevice       *device,
                 gboolean         enabled)
{
  guchar value = (enabled) ? 1 : 0;

  change_property (settings, device, "libinput Tapping Enabled",
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
  guchar *available;
  guchar *defaults;
  guchar values[SCROLL_METHOD_NUM_FIELDS] = { 0 }; /* 2fg, edge, button. The last value is unused */

  available = get_property (settings, device,
                            "libinput Scroll Methods Available",
                            XA_INTEGER, 8, SCROLL_METHOD_NUM_FIELDS);

  defaults = get_property (settings, device,
                           "libinput Scroll Method Enabled",
                           XA_INTEGER, 8, SCROLL_METHOD_NUM_FIELDS);

  if (!available || !defaults)
    {
      if (available)
        XFree (available);

      if (defaults)
        XFree (defaults);

      return;
    }

  memcpy (values, defaults, SCROLL_METHOD_NUM_FIELDS);

  /* Don't set edge scrolling if two-finger scrolling is enabled and available */
  if (available[SCROLL_METHOD_FIELD_EDGE] &&
      !(available[SCROLL_METHOD_FIELD_2FG] && values[SCROLL_METHOD_FIELD_2FG]))
    {
      values[1] = !!edge_scroll_enabled;

      change_property (settings, device, "libinput Scroll Method Enabled",
                       XA_INTEGER, 8, &values, SCROLL_METHOD_NUM_FIELDS);
    }

  if (available)
    XFree (available);

  if (defaults)
    XFree (defaults);
}

static void
set_two_finger_scroll (GfInputSettings *settings,
                       GdkDevice       *device,
                       gboolean         two_finger_scroll_enabled)
{
  guchar *available;
  gboolean changed;
  guchar *defaults;
  guchar values[SCROLL_METHOD_NUM_FIELDS] = { 0 }; /* 2fg, edge, button. The last value is unused */

  available = get_property (settings, device,
                            "libinput Scroll Methods Available",
                            XA_INTEGER, 8, SCROLL_METHOD_NUM_FIELDS);

  defaults = get_property (settings, device,
                           "libinput Scroll Method Enabled",
                           XA_INTEGER, 8, SCROLL_METHOD_NUM_FIELDS);

  if (!available || !defaults)
    {
      if (available)
        XFree (available);

      if (defaults)
        XFree (defaults);

      return;
    }

  memcpy (values, defaults, SCROLL_METHOD_NUM_FIELDS);
  changed = FALSE;

  if (available[SCROLL_METHOD_FIELD_2FG])
    {
      values[SCROLL_METHOD_FIELD_2FG] = !!two_finger_scroll_enabled;
      changed = TRUE;
    }

  /* Disable edge scrolling when two-finger scrolling is enabled */
  if (values[SCROLL_METHOD_FIELD_2FG] && values[SCROLL_METHOD_FIELD_EDGE])
    {
      values[SCROLL_METHOD_FIELD_EDGE] = 0;
      changed = TRUE;
    }

  if (changed)
    {
      change_property (settings, device, "libinput Scroll Method Enabled",
                       XA_INTEGER, 8, &values, SCROLL_METHOD_NUM_FIELDS);
    }

  if (available)
    XFree (available);

  if (defaults)
    XFree (defaults);
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
update_device_speed (GfInputSettings *settings,
                     GdkDevice       *device)
{
  GSettings *gsettings;
  const gchar *key;
  gdouble value;

  key = "speed";

  if (device)
    {
      gsettings = get_settings_for_source (settings,
                                           gdk_device_get_source (device));

      if (!gsettings)
        return;

      value = g_settings_get_double (gsettings, key);
      device_set_double_setting (settings, device, set_speed, value);
    }
  else
    {
      gsettings = get_settings_for_source (settings, GDK_SOURCE_MOUSE);
      value = g_settings_get_double (gsettings, key);
      set_double_setting (settings, GDK_SOURCE_MOUSE, set_speed, value);

      gsettings = get_settings_for_source (settings, GDK_SOURCE_TOUCHPAD);
      value = g_settings_get_double (gsettings, key);
      set_double_setting (settings, GDK_SOURCE_TOUCHPAD, set_speed, value);
    }
}

static void
update_device_natural_scroll (GfInputSettings *settings,
                              GdkDevice       *device)
{
  GSettings *gsettings;
  const gchar *key;
  gboolean value;

  key = "natural-scroll";

  if (device)
    {
      gsettings = get_settings_for_source (settings,
                                           gdk_device_get_source (device));

      if (!gsettings)
        return;

      value = g_settings_get_boolean (gsettings, key);
      device_set_bool_setting (settings, device, set_invert_scroll, value);
    }
  else
    {
      gsettings = get_settings_for_source (settings, GDK_SOURCE_MOUSE);
      value = g_settings_get_boolean (gsettings, key);

      set_bool_setting (settings, GDK_SOURCE_MOUSE,
                        set_invert_scroll, value);

      gsettings = get_settings_for_source (settings, GDK_SOURCE_TOUCHPAD);
      value = g_settings_get_boolean (gsettings, key);

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

  if (device && gdk_device_get_source (device) != GDK_SOURCE_TOUCHPAD)
    return;

  edge_scroll_enabled = g_settings_get_boolean (settings->touchpad,
                                                "edge-scrolling-enabled");

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
      change_property (settings, device, "libinput Button Scrolling Button",
                       XA_INTEGER, 32, &button, 1);
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

          change_property (settings, device, "libinput Button Scrolling Button",
                           XA_INTEGER, 32, &button, 1);
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
settings_changed_cb (GSettings       *gsettings,
                     const gchar     *key,
                     GfInputSettings *settings)
{
  if (gsettings == settings->mouse)
    {
      if (strcmp (key, "left-handed") == 0)
        update_mouse_left_handed (settings, NULL);
      else if (strcmp (key, "speed") == 0)
        update_device_speed (settings, NULL);
      else if (strcmp (key, "natural-scroll") == 0)
        update_device_natural_scroll (settings, NULL);
    }
  else if (gsettings == settings->touchpad)
    {
      if (strcmp (key, "left-handed") == 0)
        update_touchpad_left_handed (settings, NULL);
      else if (strcmp (key, "speed") == 0)
        update_device_speed (settings, NULL);
      else if (strcmp (key, "natural-scroll") == 0)
        update_device_natural_scroll (settings, NULL);
      else if (strcmp (key, "tap-to-click") == 0)
        update_touchpad_tap_enabled (settings, NULL);
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
    }
  else if (gsettings == settings->keyboard)
    {
      if (strcmp (key, "repeat") == 0 ||
          strcmp (key, "repeat-interval") == 0 ||
          strcmp (key, "delay") == 0)
        update_keyboard_repeat (settings);
    }
}

static MetaOutput *
find_output (GfInputSettings *settings,
             GSettings       *gsettings,
             GdkDevice       *device)
{
  gchar **edid;
  guint n_values;
  MetaOutput *outputs;
  guint n_outputs;
  guint i;

  edid = g_settings_get_strv (gsettings, "display");
  n_values = g_strv_length (edid);

  if (n_values != 3)
    {
      g_warning ("EDID configuration for device '%s' is incorrect, "
                 "must have 3 values", gdk_device_get_name (device));

      return NULL;
    }

  if (!*edid[0] && !*edid[1] && !*edid[2])
    return NULL;

  outputs = flashback_monitor_manager_get_outputs (settings->monitor_manager,
                                                   &n_outputs);

  for (i = 0; i < n_outputs; i++)
    {
      if (g_strcmp0 (outputs[i].vendor, edid[0]) == 0 &&
          g_strcmp0 (outputs[i].product, edid[1]) == 0 &&
          g_strcmp0 (outputs[i].serial, edid[2]) == 0)
        return &outputs[i];
    }

  return NULL;
}

static void
update_device_display (GfInputSettings *settings,
                       GSettings       *gsettings,
                       GdkDevice       *device)
{
  MetaOutput *output;
  gfloat matrix[6] = { 1, 0, 0, 0, 1, 0 };
  gfloat full_matrix[9];

  output = find_output (settings, gsettings, device);

  if (output)
    {
      flashback_monitor_manager_get_monitor_matrix (settings->monitor_manager,
                                                    output, matrix);
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
monitors_changed_cb (FlashbackMonitorManager *monitor_manager,
                     GfInputSettings         *settings)
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
  if (strcmp (key, "display") == 0)
    update_device_display (info->settings, gsettings, info->device);
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
                         info, (GClosureNotify) g_free, 0);

  g_hash_table_insert (settings->mappable_devices, device, gsettings);
  update_device_display (settings, gsettings, device);
}

static void
apply_device_settings (GfInputSettings *settings,
                       GdkDevice       *device)
{
  update_mouse_left_handed (settings, device);
  update_device_speed (settings, device);
  update_device_natural_scroll (settings, device);

  update_touchpad_left_handed (settings, device);
#if 0
  update_device_speed (settings, device);
  update_device_natural_scroll (settings, device);
#endif
  update_touchpad_tap_enabled (settings, device);
  update_touchpad_send_events (settings, device);
  update_touchpad_edge_scroll (settings, device);
  update_touchpad_two_finger_scroll (settings, device);
  update_touchpad_click_method (settings, device);

  update_trackball_scroll_button (settings, device);
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

static void
clear_monitor_manager (GfInputSettings *settings)
{
  gulong changed_id;

  changed_id = settings->monitors_changed_id;

  if (changed_id > 0 && settings->monitor_manager)
    {
      g_signal_handler_disconnect (settings->monitor_manager, changed_id);
      settings->monitors_changed_id = 0;
    }

  g_clear_object (&settings->monitor_manager);
}

static void
gf_input_settings_constructed (GObject *object)
{
  GfInputSettings *settings;

  settings = GF_INPUT_SETTINGS (object);

  apply_device_settings (settings, NULL);
  update_keyboard_repeat (settings);
  check_mappable_devices (settings);
}

static void
gf_input_settings_dispose (GObject *object)
{
  GfInputSettings *settings;

  settings = GF_INPUT_SETTINGS (object);

  clear_monitor_manager (settings);

  g_clear_object (&settings->mouse);
  g_clear_object (&settings->touchpad);
  g_clear_object (&settings->trackball);
  g_clear_object (&settings->keyboard);

  g_clear_pointer (&settings->mappable_devices, g_hash_table_destroy);

  G_OBJECT_CLASS (gf_input_settings_parent_class)->dispose (object);
}

static void
gf_input_settings_class_init (GfInputSettingsClass *settings_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (settings_class);

  object_class->constructed = gf_input_settings_constructed;
  object_class->dispose = gf_input_settings_dispose;
}

static void
gf_input_settings_init (GfInputSettings *settings)
{
  GdkDisplay *display;

  display = gdk_display_get_default ();

  settings->xdisplay = gdk_x11_display_get_xdisplay (display);

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
}

GfInputSettings *
gf_input_settings_new (void)
{
  return g_object_new (GF_TYPE_INPUT_SETTINGS, NULL);
}

void
gf_input_settings_set_display_config (GfInputSettings        *settings,
                                      FlashbackDisplayConfig *config)
{
  FlashbackMonitorManager *monitor_manager;

  monitor_manager = flashback_display_config_get_monitor_manager (config);

  clear_monitor_manager (settings);
  if (monitor_manager == NULL)
    return;

  settings->monitor_manager = g_object_ref (monitor_manager);
  g_signal_connect (settings->monitor_manager, "monitors-changed",
                    G_CALLBACK (monitors_changed_cb), settings);
}
