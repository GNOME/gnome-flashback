/*
 * Copyright (C) 2017 Alberts Muktupāvels
 * Copyright (C) 2017 Red Hat
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
 * - src/backends/meta-orientation-manager.c
 */

#include "config.h"

#include <gio/gio.h>

#include "gf-orientation-manager-private.h"

#define TOUCHSCREEN_SCHEMA "org.gnome.settings-daemon.peripherals.touchscreen"
#define ORIENTATION_LOCK_KEY "orientation-lock"

struct _GfOrientationManager
{
  GObject        parent;

  GCancellable  *cancellable;

  guint          iio_watch_id;
  GDBusProxy    *iio_proxy;
  GfOrientation  prev_orientation;
  GfOrientation  curr_orientation;

  GSettings     *settings;
};

enum
{
  ORIENTATION_CHANGED,

  LAST_SIGNAL
};

static guint manager_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfOrientationManager, gf_orientation_manager, G_TYPE_OBJECT)

static GfOrientation
orientation_from_string (const gchar *orientation)
{
  if (g_strcmp0 (orientation, "normal") == 0)
    return GF_ORIENTATION_NORMAL;
  if (g_strcmp0 (orientation, "bottom-up") == 0)
    return GF_ORIENTATION_BOTTOM_UP;
  if (g_strcmp0 (orientation, "left-up") == 0)
    return GF_ORIENTATION_LEFT_UP;
  if (g_strcmp0 (orientation, "right-up") == 0)
    return GF_ORIENTATION_RIGHT_UP;

  return GF_ORIENTATION_UNDEFINED;
}

static void
read_iio_proxy (GfOrientationManager *manager)
{
  gboolean has_accel;
  GVariant *variant;

  manager->curr_orientation = GF_ORIENTATION_UNDEFINED;

  if (!manager->iio_proxy)
    return;

  has_accel = FALSE;
  variant = g_dbus_proxy_get_cached_property (manager->iio_proxy, "HasAccelerometer");

  if (variant)
    {
      has_accel = g_variant_get_boolean (variant);
      g_variant_unref (variant);
    }

  if (!has_accel)
    return;

  variant = g_dbus_proxy_get_cached_property (manager->iio_proxy, "AccelerometerOrientation");

  if (variant)
    {
      const gchar *str;

      str = g_variant_get_string (variant, NULL);
      manager->curr_orientation = orientation_from_string (str);
      g_variant_unref (variant);
    }
}

static void
sync_state (GfOrientationManager *manager)
{
  read_iio_proxy (manager);

  if (manager->prev_orientation == manager->curr_orientation)
    return;

  manager->prev_orientation = manager->curr_orientation;
  if (manager->curr_orientation == GF_ORIENTATION_UNDEFINED)
    return;

  if (g_settings_get_boolean (manager->settings, ORIENTATION_LOCK_KEY))
    return;

  g_signal_emit (manager, manager_signals[ORIENTATION_CHANGED], 0);
}

static void
orientation_lock_changed_cb (GSettings   *settings,
                             const gchar *key,
                             gpointer     user_data)
{
  GfOrientationManager *manager;

  manager = GF_ORIENTATION_MANAGER (user_data);

  sync_state (manager);
}

static void
iio_properties_changed_cb (GDBusProxy *proxy,
                           GVariant   *changed_properties,
                           GStrv       invalidated_properties,
                           gpointer    user_data)
{
  GfOrientationManager *manager;

  manager = GF_ORIENTATION_MANAGER (user_data);

  sync_state (manager);
}

static void
accelerometer_claimed_cb (GObject      *source,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  GVariant *variant;
  GError *error;
  GfOrientationManager *manager;

  error = NULL;
  variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);

  if (!variant)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to claim accelerometer: %s", error->message);

      g_error_free (error);
      return;
    }

  manager = GF_ORIENTATION_MANAGER (user_data);

  g_variant_unref (variant);
  sync_state (manager);
}

static void
iio_proxy_ready_cb (GObject      *source,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  GDBusProxy *proxy;
  GError *error;
  GfOrientationManager *manager;

  error = NULL;
  proxy = g_dbus_proxy_new_finish (res, &error);

  if (!proxy)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to obtain IIO DBus proxy: %s", error->message);

      g_error_free (error);
      return;
    }

  manager = GF_ORIENTATION_MANAGER (user_data);
  manager->iio_proxy = proxy;

  g_signal_connect (manager->iio_proxy, "g-properties-changed",
                    G_CALLBACK (iio_properties_changed_cb), manager);

  g_dbus_proxy_call (manager->iio_proxy,
                     "ClaimAccelerometer",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     manager->cancellable,
                     accelerometer_claimed_cb,
                     manager);
}

static void
iio_sensor_appeared_cb (GDBusConnection *connection,
                        const gchar     *name,
                        const gchar     *name_owner,
                        gpointer         user_data)
{
  GfOrientationManager *manager;

  manager = GF_ORIENTATION_MANAGER (user_data);

  manager->cancellable = g_cancellable_new ();
  g_dbus_proxy_new (connection,
                    G_DBUS_PROXY_FLAGS_NONE,
                    NULL,
                    "net.hadess.SensorProxy",
                    "/net/hadess/SensorProxy",
                    "net.hadess.SensorProxy",
                    manager->cancellable,
                    iio_proxy_ready_cb,
                    manager);
}

static void
iio_sensor_vanished_cb (GDBusConnection *connection,
                        const gchar     *name,
                        gpointer         user_data)
{
  GfOrientationManager *manager;

  manager = GF_ORIENTATION_MANAGER (user_data);

  g_cancellable_cancel (manager->cancellable);
  g_clear_object (&manager->cancellable);

  g_clear_object (&manager->iio_proxy);

  sync_state (manager);
}

static void
gf_orientation_manager_dispose (GObject *object)
{
  GfOrientationManager *manager;

  manager = GF_ORIENTATION_MANAGER (object);

  if (manager->cancellable != NULL)
    {
      g_cancellable_cancel (manager->cancellable);
      g_clear_object (&manager->cancellable);
    }

  if (manager->iio_watch_id != 0)
    {
      g_bus_unwatch_name (manager->iio_watch_id);
      manager->iio_watch_id = 0;
    }

  g_clear_object (&manager->iio_proxy);
  g_clear_object (&manager->settings);

  G_OBJECT_CLASS (gf_orientation_manager_parent_class)->dispose (object);
}

static void
gf_orientation_manager_install_signals (GObjectClass *object_class)
{
  manager_signals[ORIENTATION_CHANGED] =
    g_signal_new ("orientation-changed",
                  G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gf_orientation_manager_class_init (GfOrientationManagerClass *manager_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (manager_class);

  object_class->dispose = gf_orientation_manager_dispose;

  gf_orientation_manager_install_signals (object_class);
}

static void
gf_orientation_manager_init (GfOrientationManager *manager)
{
  manager->iio_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                            "net.hadess.SensorProxy",
                                            G_BUS_NAME_WATCHER_FLAGS_NONE,
                                            iio_sensor_appeared_cb,
                                            iio_sensor_vanished_cb,
                                            manager,
                                            NULL);

  manager->settings = g_settings_new (TOUCHSCREEN_SCHEMA);

  g_signal_connect (manager->settings, "changed::" ORIENTATION_LOCK_KEY,
                    G_CALLBACK (orientation_lock_changed_cb), manager);

  sync_state (manager);
}

GfOrientationManager *
gf_orientation_manager_new (void)
{
  return g_object_new (GF_TYPE_ORIENTATION_MANAGER, NULL);
}

GfOrientation
gf_orientation_manager_get_orientation (GfOrientationManager *manager)
{
  return manager->curr_orientation;
}
