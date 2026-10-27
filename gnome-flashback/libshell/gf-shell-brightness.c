/*
 * Copyright (C) 2026 Carlos Fernandez Sanz
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
 * Based on GNOME Shell's js/misc/brightnessManager.js and on mutter's
 * src/backends/meta-backlight-sysfs.c and src/backends/meta-udev.c.
 */

#include "config.h"
#include "gf-shell-brightness.h"

#include <gdk/gdkx.h>
#include <gudev/gudev.h>
#include <math.h>
#include <X11/Xlib.h>

#include "backends/gf-logical-monitor-private.h"
#include "backends/gf-monitor-manager-private.h"
#include "backends/gf-monitor-private.h"
#include "dbus/gf-brightness-gen.h"
#include "dbus/gf-shell-brightness-gen.h"

#define SHELL_BRIGHTNESS_DBUS_NAME "org.gnome.Shell.Brightness"
#define SHELL_BRIGHTNESS_DBUS_PATH "/org/gnome/Shell/Brightness"

#define LOGIN_DBUS_NAME "org.freedesktop.login1"
#define LOGIN_SESSION_DBUS_PATH "/org/freedesktop/login1/session/auto"
#define LOGIN_SESSION_DBUS_INTERFACE "org.freedesktop.login1.Session"

#define POWER_SCHEMA "org.gnome.settings-daemon.plugins.power"

#define N_STEPS 20
#define VALUE_EPSILON 0.001

typedef struct
{
  char        *connector;

  /* sysfs backlight, set through logind */
  GUdevDevice *device;
  guint        n_pending;

  int          min;
  int          max;
  int          brightness;
  int          n_steps;

  /* relative brightness chosen by the user, [0, 1] */
  double       value;
  /* value relative to the brightest display */
  double       factor;
} GfBacklight;

struct _GfShellBrightness
{
  GObject               parent;

  FlashbackOsd         *osd;
  GfMonitorManager     *monitor_manager;

  GUdevClient          *udev_client;
  GDBusConnection      *system_bus;
  GSettings            *power_settings;
  GCancellable         *cancellable;

  GPtrArray            *backlights;
  double                value;
  int                   n_steps;

  gboolean              dimming;
  double                auto_brightness_target;

  GfShellBrightnessGen *shell_brightness;
  GfBrightnessGen      *brightness;
  gboolean              updating_brightness;
  gint                  bus_name_id;
};

typedef struct
{
  GfShellBrightness *self;
  char              *sysfs_path;
} SetBrightnessData;

G_DEFINE_TYPE (GfShellBrightness, gf_shell_brightness, G_TYPE_OBJECT)

static void
backlight_free (GfBacklight *backlight)
{
  g_free (backlight->connector);
  g_clear_object (&backlight->device);
  g_free (backlight);
}

static double
get_relative_brightness (GfBacklight *backlight,
                         int          brightness)
{
  return (double) (brightness - backlight->min) /
         (backlight->max - backlight->min);
}

static GfMonitor *
find_monitor (GfShellBrightness *self,
              const char        *connector)
{
  GList *l;

  for (l = gf_monitor_manager_get_monitors (self->monitor_manager);
       l != NULL;
       l = l->next)
    {
      GfMonitor *monitor;

      monitor = l->data;

      if (g_strcmp0 (gf_monitor_get_connector (monitor), connector) == 0)
        return monitor;
    }

  return NULL;
}

static GUdevDevice *
find_device_by_type (GList      *devices,
                     const char *type)
{
  GList *l;

  for (l = devices; l != NULL; l = l->next)
    {
      GUdevDevice *device;

      device = l->data;

      if (g_strcmp0 (g_udev_device_get_sysfs_attr (device, "type"), type) == 0)
        return g_object_ref (device);
    }

  return NULL;
}

static GUdevDevice *
find_device_for_connector (GList      *devices,
                           const char *connector)
{
  char *suffix;
  GList *l;

  suffix = g_strdup_printf ("-%s", connector);

  for (l = devices; l != NULL; l = l->next)
    {
      GUdevDevice *device;
      GUdevDevice *parent;
      gboolean match;

      device = l->data;

      if (g_strcmp0 (g_udev_device_get_sysfs_attr (device, "type"), "raw") != 0)
        continue;

      /* Raw backlights registered by a drm driver have the connector as
       * their parent, named card<n>-<connector>.
       */
      parent = g_udev_device_get_parent (device);
      if (parent == NULL)
        continue;

      match = g_strcmp0 (g_udev_device_get_subsystem (parent), "drm") == 0 &&
              g_str_has_suffix (g_udev_device_get_name (parent), suffix) &&
              g_strcmp0 (g_udev_device_get_sysfs_attr (parent, "enabled"),
                         "enabled") == 0;

      g_object_unref (parent);

      if (match)
        {
          g_free (suffix);
          return g_object_ref (device);
        }
    }

  g_free (suffix);

  return NULL;
}

static GUdevDevice *
find_device (GfShellBrightness *self,
             const char        *connector,
             gboolean           is_builtin)
{
  GList *devices;
  GUdevDevice *device;

  devices = g_udev_client_query_by_subsystem (self->udev_client, "backlight");
  device = NULL;

  /* For built-in panels, prefer firmware, then platform, then raw. */
  if (is_builtin)
    device = find_device_by_type (devices, "firmware");

  if (device == NULL && is_builtin)
    device = find_device_by_type (devices, "platform");

  if (device == NULL)
    device = find_device_for_connector (devices, connector);

  if (device == NULL && is_builtin)
    device = find_device_by_type (devices, "raw");

  g_list_free_full (devices, g_object_unref);

  return device;
}

static GfBacklight *
backlight_new_sysfs (GUdevDevice *device,
                     const char  *connector)
{
  GfBacklight *backlight;
  int max;
  int min;

  max = g_udev_device_get_sysfs_attr_as_int_uncached (device, "max_brightness");
  min = MAX (1, max / 100);

  /* If a raw interface has less than 100 values, assume that 0 does not
   * turn the backlight off.
   */
  if (max < 99 &&
      g_strcmp0 (g_udev_device_get_sysfs_attr (device, "type"), "raw") == 0)
    min = 0;

  if (min >= max)
    return NULL;

  backlight = g_new0 (GfBacklight, 1);
  backlight->connector = g_strdup (connector);
  backlight->device = g_object_ref (device);
  backlight->min = min;
  backlight->max = max;

  backlight->brightness =
    g_udev_device_get_sysfs_attr_as_int_uncached (device, "brightness");
  backlight->brightness = CLAMP (backlight->brightness, min, max);

  return backlight;
}

static GfBacklight *
backlight_new_xrandr (GfMonitor  *monitor,
                      const char *connector)
{
  GfBacklight *backlight;
  int value;
  int min;
  int max;

  if (!gf_monitor_get_backlight_info (monitor, &min, &max) ||
      !gf_monitor_get_backlight (monitor, &value) ||
      min >= max)
    return NULL;

  backlight = g_new0 (GfBacklight, 1);
  backlight->connector = g_strdup (connector);
  backlight->min = min;
  backlight->max = max;
  backlight->brightness = CLAMP (value, min, max);

  return backlight;
}

static void
set_brightness_cb (GObject      *object,
                   GAsyncResult *res,
                   gpointer      user_data);

static void
backlight_set_brightness (GfShellBrightness *self,
                          GfBacklight       *backlight,
                          int                brightness)
{
  brightness = CLAMP (brightness, backlight->min, backlight->max);

  if (brightness == backlight->brightness)
    return;

  if (backlight->device != NULL)
    {
      SetBrightnessData *data;

      if (self->system_bus == NULL)
        {
          GError *error;

          error = NULL;
          self->system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);

          if (self->system_bus == NULL)
            {
              g_warning ("Failed to get the system bus: %s", error->message);
              g_error_free (error);
              return;
            }
        }

      backlight->brightness = brightness;
      backlight->n_pending++;

      data = g_new0 (SetBrightnessData, 1);
      data->self = self;
      data->sysfs_path = g_strdup (g_udev_device_get_sysfs_path (backlight->device));

      /* The session can't write to sysfs, logind does it for us. */
      g_dbus_connection_call (self->system_bus,
                              LOGIN_DBUS_NAME,
                              LOGIN_SESSION_DBUS_PATH,
                              LOGIN_SESSION_DBUS_INTERFACE,
                              "SetBrightness",
                              g_variant_new ("(ssu)",
                                             "backlight",
                                             g_udev_device_get_name (backlight->device),
                                             (guint32) brightness),
                              NULL,
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              self->cancellable,
                              set_brightness_cb,
                              data);
    }
  else
    {
      GfMonitor *monitor;

      monitor = find_monitor (self, backlight->connector);

      if (monitor != NULL)
        {
          backlight->brightness = brightness;
          gf_monitor_set_backlight (monitor, brightness);
        }
    }
}

static GfBacklight *
find_backlight_for_device (GfShellBrightness *self,
                           GUdevDevice       *device)
{
  const char *sysfs_path;
  guint i;

  sysfs_path = g_udev_device_get_sysfs_path (device);

  for (i = 0; i < self->backlights->len; i++)
    {
      GfBacklight *backlight;

      backlight = g_ptr_array_index (self->backlights, i);

      if (backlight->device != NULL &&
          g_strcmp0 (g_udev_device_get_sysfs_path (backlight->device),
                     sysfs_path) == 0)
        return backlight;
    }

  return NULL;
}

static void
update_scale_factors (GfShellBrightness *self)
{
  double max;
  guint i;

  if (self->backlights->len == 0)
    return;

  max = 0.0;
  for (i = 0; i < self->backlights->len; i++)
    {
      GfBacklight *backlight;

      backlight = g_ptr_array_index (self->backlights, i);
      max = MAX (max, backlight->value);
    }

  /* If the maximum is 0, the ratios can't be deduced. */
  if (max > 0.01)
    {
      for (i = 0; i < self->backlights->len; i++)
        {
          GfBacklight *backlight;

          backlight = g_ptr_array_index (self->backlights, i);
          backlight->factor = backlight->value / max;
        }
    }

  self->value = max;
}

static void
update_brightness_property (GfShellBrightness *self)
{
  int percentage;

  percentage = -1;
  if (self->backlights->len > 0)
    percentage = (int) round (self->value * 100.0);

  if (gf_brightness_gen_get_brightness (self->brightness) == percentage)
    return;

  self->updating_brightness = TRUE;
  gf_brightness_gen_set_brightness (self->brightness, percentage);
  self->updating_brightness = FALSE;
}

static double
get_dimming_target (GfShellBrightness *self)
{
  if (self->power_settings == NULL)
    return 0.3;

  return g_settings_get_int (self->power_settings, "idle-brightness") / 100.0;
}

static void
apply_brightness (GfShellBrightness *self)
{
  double dimming_target;
  guint i;

  dimming_target = get_dimming_target (self);

  for (i = 0; i < self->backlights->len; i++)
    {
      GfBacklight *backlight;
      double target;
      double level;

      backlight = g_ptr_array_index (self->backlights, i);

      /* With automatic brightness, the user's brightness is a bias on the
       * target of the light sensor.
       */
      if (self->auto_brightness_target >= 0.0)
        target = CLAMP (self->auto_brightness_target + backlight->value - 0.5,
                        0.0, 1.0);
      else
        target = backlight->value;

      level = target;
      if (self->dimming)
        level = MIN (dimming_target, target);

      backlight_set_brightness (self,
                                backlight,
                                backlight->min +
                                (int) round ((backlight->max - backlight->min) *
                                             level));
    }

  update_brightness_property (self);
}

static void
external_brightness_change (GfShellBrightness *self,
                            GfBacklight       *backlight,
                            int                brightness)
{
  brightness = CLAMP (brightness, backlight->min, backlight->max);

  if (brightness == backlight->brightness)
    return;

  /* Somebody else changed the backlight. Take it as the new brightness and
   * stop dimming, as GNOME Shell does.
   */
  backlight->brightness = brightness;
  backlight->value = get_relative_brightness (backlight, brightness);
  self->dimming = FALSE;

  update_scale_factors (self);
  update_brightness_property (self);
}

static void
set_brightness_cb (GObject      *object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  SetBrightnessData *data;
  GVariant *result;
  GError *error;
  GfShellBrightness *self;
  guint i;

  data = user_data;

  error = NULL;
  result = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object),
                                          res,
                                          &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      g_free (data->sysfs_path);
      g_free (data);
      return;
    }

  self = data->self;

  if (error != NULL)
    {
      g_warning ("Failed to set the backlight brightness: %s",
                 error->message);
      g_error_free (error);
    }

  g_clear_pointer (&result, g_variant_unref);

  for (i = 0; i < self->backlights->len; i++)
    {
      GfBacklight *backlight;

      backlight = g_ptr_array_index (self->backlights, i);

      if (backlight->device == NULL ||
          backlight->n_pending == 0 ||
          g_strcmp0 (g_udev_device_get_sysfs_path (backlight->device),
                     data->sysfs_path) != 0)
        continue;

      /* Once all pending changes are done, read back what was set. */
      if (--backlight->n_pending == 0)
        {
          int brightness;

          brightness = g_udev_device_get_sysfs_attr_as_int_uncached (backlight->device,
                                                                     "brightness");

          external_brightness_change (self, backlight, brightness);
        }

      break;
    }

  g_free (data->sysfs_path);
  g_free (data);
}

static void
show_osd (GfShellBrightness *self,
          GPtrArray         *changed)
{
  GVariantBuilder builder;
  double level;
  GVariant *params;

  if (changed->len == 0)
    return;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&builder, "{sv}", "icon",
                         g_variant_new_string ("display-brightness-symbolic"));

  if (changed->len == 1)
    {
      GfBacklight *backlight;

      backlight = g_ptr_array_index (changed, 0);
      level = backlight->value;

      g_variant_builder_add (&builder, "{sv}", "connector",
                             g_variant_new_string (backlight->connector));
    }
  else
    {
      level = self->value;
    }

  g_variant_builder_add (&builder, "{sv}", "level",
                         g_variant_new_double (level));

  params = g_variant_ref_sink (g_variant_builder_end (&builder));
  flashback_osd_show (self->osd, self->monitor_manager, params);
  g_variant_unref (params);
}

static double
step_value (double value,
            int    n_steps,
            int    direction)
{
  if (direction == 0)
    {
      if (fabs (1.0 - value) < VALUE_EPSILON)
        return 0.0;

      direction = 1;
    }

  return CLAMP (value + (double) direction / n_steps, 0.0, 1.0);
}

static GfBacklight *
get_current_backlight (GfShellBrightness *self)
{
  Display *xdisplay;
  Window root;
  Window child;
  int x;
  int y;
  int win_x;
  int win_y;
  unsigned int mask;
  GList *l;

  xdisplay = gdk_x11_get_default_xdisplay ();

  if (!XQueryPointer (xdisplay, DefaultRootWindow (xdisplay),
                      &root, &child, &x, &y, &win_x, &win_y, &mask))
    return NULL;

  for (l = gf_monitor_manager_get_logical_monitors (self->monitor_manager);
       l != NULL;
       l = l->next)
    {
      GfLogicalMonitor *logical_monitor;
      GfRectangle layout;
      GList *monitors;

      logical_monitor = l->data;
      layout = gf_logical_monitor_get_layout (logical_monitor);

      if (x < layout.x || x >= layout.x + layout.width ||
          y < layout.y || y >= layout.y + layout.height)
        continue;

      for (monitors = gf_logical_monitor_get_monitors (logical_monitor);
           monitors != NULL;
           monitors = monitors->next)
        {
          const char *connector;
          guint i;

          connector = gf_monitor_get_connector (monitors->data);

          for (i = 0; i < self->backlights->len; i++)
            {
              GfBacklight *backlight;

              backlight = g_ptr_array_index (self->backlights, i);

              if (g_strcmp0 (backlight->connector, connector) == 0)
                return backlight;
            }
        }

      break;
    }

  return NULL;
}

static void
user_changed (GfShellBrightness *self)
{
  apply_brightness (self);

  gf_shell_brightness_gen_emit_brightness_changed (self->shell_brightness);
}

static void
set_global_value (GfShellBrightness *self,
                  double             value)
{
  guint i;

  self->value = CLAMP (value, 0.0, 1.0);

  for (i = 0; i < self->backlights->len; i++)
    {
      GfBacklight *backlight;

      backlight = g_ptr_array_index (self->backlights, i);
      backlight->value = CLAMP (self->value * backlight->factor, 0.0, 1.0);
    }
}

static void
step (GfShellBrightness *self,
      gboolean           current_monitor,
      int                direction)
{
  GPtrArray *changed;
  guint i;

  if (self->backlights->len == 0)
    return;

  changed = g_ptr_array_new ();

  if (current_monitor)
    {
      GfBacklight *backlight;

      backlight = get_current_backlight (self);

      if (backlight != NULL)
        {
          backlight->value = step_value (backlight->value,
                                         backlight->n_steps,
                                         direction);

          update_scale_factors (self);
          g_ptr_array_add (changed, backlight);
        }
    }
  else
    {
      set_global_value (self, step_value (self->value,
                                          self->n_steps,
                                          direction));

      for (i = 0; i < self->backlights->len; i++)
        g_ptr_array_add (changed, g_ptr_array_index (self->backlights, i));
    }

  if (changed->len > 0)
    {
      user_changed (self);
      show_osd (self, changed);
    }

  g_ptr_array_free (changed, TRUE);
}

static GfBacklight *
find_backlight (GPtrArray  *backlights,
                const char *connector)
{
  guint i;

  for (i = 0; i < backlights->len; i++)
    {
      GfBacklight *backlight;

      backlight = g_ptr_array_index (backlights, i);

      if (g_strcmp0 (backlight->connector, connector) == 0)
        return backlight;
    }

  return NULL;
}

static void
update_backlights (GfShellBrightness *self)
{
  GPtrArray *old_backlights;
  GList *l;
  int max_steps;
  gboolean has_control;
  guint i;

  old_backlights = self->backlights;
  self->backlights = g_ptr_array_new_with_free_func ((GDestroyNotify) backlight_free);

  for (l = gf_monitor_manager_get_monitors (self->monitor_manager);
       l != NULL;
       l = l->next)
    {
      GfMonitor *monitor;
      const char *connector;
      GUdevDevice *device;
      GfBacklight *backlight;
      GfBacklight *old_backlight;

      monitor = l->data;

      if (!gf_monitor_is_active (monitor))
        continue;

      connector = gf_monitor_get_connector (monitor);
      device = find_device (self, connector, gf_monitor_is_laptop_panel (monitor));
      backlight = NULL;

      if (device != NULL)
        {
          backlight = backlight_new_sysfs (device, connector);
          g_object_unref (device);
        }

      if (backlight == NULL)
        backlight = backlight_new_xrandr (monitor, connector);

      if (backlight == NULL)
        continue;

      backlight->n_steps = MIN (backlight->max - backlight->min, N_STEPS);
      backlight->value = get_relative_brightness (backlight, backlight->brightness);
      backlight->factor = 1.0;

      /* Keep the brightness chosen by the user, which may differ from the
       * current one while dimming.
       */
      old_backlight = find_backlight (old_backlights, connector);
      if (old_backlight != NULL &&
          (old_backlight->device == NULL) == (backlight->device == NULL) &&
          old_backlight->min == backlight->min &&
          old_backlight->max == backlight->max)
        {
          backlight->value = old_backlight->value;
          backlight->n_pending = old_backlight->n_pending;
        }

      g_ptr_array_add (self->backlights, backlight);
    }

  g_ptr_array_free (old_backlights, TRUE);

  max_steps = 0;
  for (i = 0; i < self->backlights->len; i++)
    {
      GfBacklight *backlight;

      backlight = g_ptr_array_index (self->backlights, i);
      max_steps = MAX (max_steps, backlight->n_steps);
    }

  self->n_steps = MIN (max_steps, N_STEPS);
  update_scale_factors (self);

  has_control = self->backlights->len > 0;
  if (gf_shell_brightness_gen_get_has_brightness_control (self->shell_brightness) != has_control)
    {
      gf_shell_brightness_gen_set_has_brightness_control (self->shell_brightness,
                                                          has_control);
    }

  apply_brightness (self);
}

static void
uevent_cb (GUdevClient       *client,
           const char        *action,
           GUdevDevice       *device,
           GfShellBrightness *self)
{
  GfBacklight *backlight;

  if (self->monitor_manager == NULL)
    return;

  if (g_strcmp0 (action, "change") != 0)
    {
      update_backlights (self);
      return;
    }

  backlight = find_backlight_for_device (self, device);

  /* Changes caused by us are read back when they are done. */
  if (backlight == NULL || backlight->n_pending > 0)
    return;

  external_brightness_change (self,
                              backlight,
                              g_udev_device_get_sysfs_attr_as_int_uncached (device,
                                                                            "brightness"));
}

static void
monitors_changed_cb (GfMonitorManager  *monitor_manager,
                     GfShellBrightness *self)
{
  update_backlights (self);
}

static gboolean
handle_set_dimming (GfShellBrightnessGen  *object,
                    GDBusMethodInvocation *invocation,
                    gboolean               enable,
                    GfShellBrightness     *self)
{
  self->dimming = enable;
  apply_brightness (self);

  gf_shell_brightness_gen_complete_set_dimming (object, invocation);

  return TRUE;
}

static gboolean
handle_set_auto_brightness_target (GfShellBrightnessGen  *object,
                                   GDBusMethodInvocation *invocation,
                                   double                 target,
                                   GfShellBrightness     *self)
{
  self->auto_brightness_target = target;
  apply_brightness (self);

  gf_shell_brightness_gen_complete_set_auto_brightness_target (object,
                                                               invocation);

  return TRUE;
}

static gboolean
handle_step_up (GfBrightnessGen       *object,
                GDBusMethodInvocation *invocation,
                gboolean               current_monitor,
                GfShellBrightness     *self)
{
  step (self, current_monitor, 1);
  gf_brightness_gen_complete_step_up (object, invocation);

  return TRUE;
}

static gboolean
handle_step_down (GfBrightnessGen       *object,
                  GDBusMethodInvocation *invocation,
                  gboolean               current_monitor,
                  GfShellBrightness     *self)
{
  step (self, current_monitor, -1);
  gf_brightness_gen_complete_step_down (object, invocation);

  return TRUE;
}

static gboolean
handle_cycle (GfBrightnessGen       *object,
              GDBusMethodInvocation *invocation,
              gboolean               current_monitor,
              GfShellBrightness     *self)
{
  step (self, current_monitor, 0);
  gf_brightness_gen_complete_cycle (object, invocation);

  return TRUE;
}

static void
brightness_notify_cb (GfBrightnessGen   *object,
                      GParamSpec        *pspec,
                      GfShellBrightness *self)
{
  int percentage;

  if (self->updating_brightness)
    return;

  percentage = gf_brightness_gen_get_brightness (object);

  if (self->backlights->len == 0 || percentage < 0)
    {
      update_brightness_property (self);
      return;
    }

  set_global_value (self, MIN (percentage, 100) / 100.0);
  user_changed (self);
}

static void
bus_acquired_handler (GDBusConnection *connection,
                      const char      *name,
                      gpointer         user_data)
{
  GfShellBrightness *self;
  GError *error;

  self = GF_SHELL_BRIGHTNESS (user_data);

  error = NULL;
  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->shell_brightness),
                                         connection,
                                         SHELL_BRIGHTNESS_DBUS_PATH,
                                         &error) ||
      !g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->brightness),
                                         connection,
                                         SHELL_BRIGHTNESS_DBUS_PATH,
                                         &error))
    {
      g_warning ("Failed to export interface: %s", error->message);
      g_error_free (error);
      return;
    }
}

static void
gf_shell_brightness_dispose (GObject *object)
{
  GfShellBrightness *self;

  self = GF_SHELL_BRIGHTNESS (object);

  if (self->bus_name_id != 0)
    {
      g_bus_unown_name (self->bus_name_id);
      self->bus_name_id = 0;
    }

  if (self->shell_brightness != NULL)
    {
      g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->shell_brightness));
      g_clear_object (&self->shell_brightness);
    }

  if (self->brightness != NULL)
    {
      g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->brightness));
      g_clear_object (&self->brightness);
    }

  if (self->monitor_manager != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->monitor_manager,
                                            monitors_changed_cb,
                                            self);
      self->monitor_manager = NULL;
    }

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->udev_client);
  g_clear_object (&self->system_bus);
  g_clear_object (&self->power_settings);
  g_clear_pointer (&self->backlights, g_ptr_array_unref);

  G_OBJECT_CLASS (gf_shell_brightness_parent_class)->dispose (object);
}

static void
gf_shell_brightness_class_init (GfShellBrightnessClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_shell_brightness_dispose;
}

static void
gf_shell_brightness_init (GfShellBrightness *self)
{
  const char *subsystems[] = { "backlight", NULL };
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;

  self->cancellable = g_cancellable_new ();
  self->backlights = g_ptr_array_new_with_free_func ((GDestroyNotify) backlight_free);
  self->auto_brightness_target = -1.0;

  source = g_settings_schema_source_get_default ();
  schema = NULL;

  if (source != NULL)
    schema = g_settings_schema_source_lookup (source, POWER_SCHEMA, TRUE);

  if (schema != NULL)
    {
      self->power_settings = g_settings_new_full (schema, NULL, NULL);
      g_settings_schema_unref (schema);
    }

  self->udev_client = g_udev_client_new (subsystems);
  g_signal_connect (self->udev_client, "uevent",
                    G_CALLBACK (uevent_cb), self);

  self->shell_brightness = gf_shell_brightness_gen_skeleton_new ();

  g_signal_connect (self->shell_brightness, "handle-set-dimming",
                    G_CALLBACK (handle_set_dimming), self);
  g_signal_connect (self->shell_brightness, "handle-set-auto-brightness-target",
                    G_CALLBACK (handle_set_auto_brightness_target), self);

  self->brightness = gf_brightness_gen_skeleton_new ();
  gf_brightness_gen_set_brightness (self->brightness, -1);

  g_signal_connect (self->brightness, "handle-step-up",
                    G_CALLBACK (handle_step_up), self);
  g_signal_connect (self->brightness, "handle-step-down",
                    G_CALLBACK (handle_step_down), self);
  g_signal_connect (self->brightness, "handle-cycle",
                    G_CALLBACK (handle_cycle), self);
  g_signal_connect (self->brightness, "notify::brightness",
                    G_CALLBACK (brightness_notify_cb), self);

  self->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                      SHELL_BRIGHTNESS_DBUS_NAME,
                                      G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                      G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                      bus_acquired_handler,
                                      NULL,
                                      NULL,
                                      self,
                                      NULL);
}

GfShellBrightness *
gf_shell_brightness_new (FlashbackOsd *osd)
{
  GfShellBrightness *self;

  self = g_object_new (GF_TYPE_SHELL_BRIGHTNESS, NULL);
  self->osd = osd;

  return self;
}

void
gf_shell_brightness_set_monitor_manager (GfShellBrightness *self,
                                         GfMonitorManager  *monitor_manager)
{
  if (self->monitor_manager == monitor_manager)
    return;

  if (self->monitor_manager != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->monitor_manager,
                                            monitors_changed_cb,
                                            self);
    }

  self->monitor_manager = monitor_manager;

  if (monitor_manager != NULL)
    {
      g_signal_connect (monitor_manager, "monitors-changed",
                        G_CALLBACK (monitors_changed_cb), self);

      update_backlights (self);
    }
}
