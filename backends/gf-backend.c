/*
 * Copyright (C) 2014 Red Hat
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 *
 * Adapted from mutter:
 * - src/backends/meta-backend.c
 */

#include "config.h"

#include <gio/gio.h>

#include "gf-backend-x11-cm-private.h"
#include "gf-gpu-private.h"
#include "gf-orientation-manager-private.h"
#include "gf-settings-private.h"

typedef struct
{
  GfSettings           *settings;
  GfOrientationManager *orientation_manager;

  GfMonitorManager     *monitor_manager;

  guint                 upower_watch_id;
  GDBusProxy           *upower_proxy;
  gboolean              lid_is_closed;

  GList                *gpus;
} GfBackendPrivate;

enum
{
  LID_IS_CLOSED_CHANGED,
  GPU_ADDED,

  LAST_SIGNAL
};

static guint backend_signals[LAST_SIGNAL] = { 0 };

static void
initable_iface_init (GInitableIface *initable_iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GfBackend, gf_backend, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (GfBackend)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         initable_iface_init))

static GfMonitorManager *
create_monitor_manager (GfBackend  *backend,
                        GError    **error)
{
  return GF_BACKEND_GET_CLASS (backend)->create_monitor_manager (backend, error);
}

static void
upower_properties_changed_cb (GDBusProxy *proxy,
                              GVariant   *changed_properties,
                              GStrv       invalidated_properties,
                              GfBackend  *self)
{
  GfBackendPrivate *priv;
  GVariant *v;
  gboolean lid_is_closed;

  priv = gf_backend_get_instance_private (self);

  v = g_variant_lookup_value (changed_properties,
                              "LidIsClosed",
                              G_VARIANT_TYPE_BOOLEAN);

  if (v == NULL)
    return;

  lid_is_closed = g_variant_get_boolean (v);
  g_variant_unref (v);

  if (priv->lid_is_closed == lid_is_closed)
    return;

  priv->lid_is_closed = lid_is_closed;

  g_signal_emit (self,
                 backend_signals[LID_IS_CLOSED_CHANGED],
                 0,
                 priv->lid_is_closed);
}

static void
upower_ready_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GError *error;
  GDBusProxy *proxy;
  GfBackend *self;
  GfBackendPrivate *priv;
  GVariant *v;

  error = NULL;
  proxy = g_dbus_proxy_new_finish (res, &error);

  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to create UPower proxy: %s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_BACKEND (user_data);
  priv = gf_backend_get_instance_private (self);

  priv->upower_proxy = proxy;

  g_signal_connect (proxy,
                    "g-properties-changed",
                    G_CALLBACK (upower_properties_changed_cb),
                    self);

  v = g_dbus_proxy_get_cached_property (proxy, "LidIsClosed");

  if (v == NULL)
    return;

  priv->lid_is_closed = g_variant_get_boolean (v);
  g_variant_unref (v);

  if (priv->lid_is_closed)
    {
      g_signal_emit (self,
                     backend_signals[LID_IS_CLOSED_CHANGED],
                     0,
                     priv->lid_is_closed);
    }
}

static void
upower_appeared_cb (GDBusConnection *connection,
                    const char      *name,
                    const char      *name_owner,
                    gpointer         user_data)
{
  GfBackend *self;

  self = GF_BACKEND (user_data);

  g_dbus_proxy_new (connection,
                    G_DBUS_PROXY_FLAGS_NONE,
                    NULL,
                    "org.freedesktop.UPower",
                    "/org/freedesktop/UPower",
                    "org.freedesktop.UPower",
                    NULL,
                    upower_ready_cb,
                    self);
}

static void
upower_vanished_cb (GDBusConnection *connection,
                    const char      *name,
                    gpointer         user_data)
{
  GfBackend *self;
  GfBackendPrivate *priv;

  self = GF_BACKEND (user_data);
  priv = gf_backend_get_instance_private (self);

  g_clear_object (&priv->upower_proxy);
}

static gboolean
gf_backend_initable_init (GInitable     *initable,
                          GCancellable  *cancellable,
                          GError       **error)
{
  GfBackend *backend;
  GfBackendPrivate *priv;

  backend = GF_BACKEND (initable);
  priv = gf_backend_get_instance_private (backend);

  priv->settings = gf_settings_new (backend);
  priv->orientation_manager = gf_orientation_manager_new ();

  priv->monitor_manager = create_monitor_manager (backend, error);
  if (!priv->monitor_manager)
    return FALSE;

  return TRUE;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = gf_backend_initable_init;
}

static gboolean
gf_backend_real_is_lid_closed (GfBackend *self)
{
  GfBackendPrivate *priv;

  priv = gf_backend_get_instance_private (self);

  return priv->lid_is_closed;
}

static void
gf_backend_constructed (GObject *object)
{
  GfBackend *self;
  GfBackendClass *self_class;
  GfBackendPrivate *priv;

  self = GF_BACKEND (object);
  self_class = GF_BACKEND_GET_CLASS (self);
  priv = gf_backend_get_instance_private (self);

  G_OBJECT_CLASS (gf_backend_parent_class)->constructed (object);

  if (self_class->is_lid_closed != gf_backend_real_is_lid_closed)
    return;

  priv->upower_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                            "org.freedesktop.UPower",
                                            G_BUS_NAME_WATCHER_FLAGS_NONE,
                                            upower_appeared_cb,
                                            upower_vanished_cb,
                                            self,
                                            NULL);
}

static void
gf_backend_dispose (GObject *object)
{
  GfBackend *backend;
  GfBackendPrivate *priv;

  backend = GF_BACKEND (object);
  priv = gf_backend_get_instance_private (backend);

  g_clear_object (&priv->monitor_manager);
  g_clear_object (&priv->orientation_manager);
  g_clear_object (&priv->settings);

  G_OBJECT_CLASS (gf_backend_parent_class)->dispose (object);
}

static void
gf_backend_finalize (GObject *object)
{
  GfBackend *self;
  GfBackendPrivate *priv;

  self = GF_BACKEND (object);
  priv = gf_backend_get_instance_private (self);

  if (priv->upower_watch_id != 0)
    {
      g_bus_unwatch_name (priv->upower_watch_id);
      priv->upower_watch_id = 0;
    }

  g_clear_object (&priv->upower_proxy);

  g_list_free_full (priv->gpus, g_object_unref);

  G_OBJECT_CLASS (gf_backend_parent_class)->finalize (object);
}

static void
gf_backend_real_post_init (GfBackend *backend)
{
  GfBackendPrivate *priv;

  priv = gf_backend_get_instance_private (backend);

  gf_monitor_manager_setup (priv->monitor_manager);
}

static void
gf_backend_class_init (GfBackendClass *backend_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (backend_class);

  object_class->constructed = gf_backend_constructed;
  object_class->dispose = gf_backend_dispose;
  object_class->finalize = gf_backend_finalize;

  backend_class->post_init = gf_backend_real_post_init;
  backend_class->is_lid_closed = gf_backend_real_is_lid_closed;

  backend_signals[LID_IS_CLOSED_CHANGED] =
    g_signal_new ("lid-is-closed-changed",
                  G_TYPE_FROM_CLASS (backend_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  backend_signals[GPU_ADDED] =
    g_signal_new ("gpu-added",
                  G_TYPE_FROM_CLASS (backend_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  GF_TYPE_GPU);
}

static void
gf_backend_init (GfBackend *backend)
{
}

GfBackend *
gf_backend_new (GfBackendType type)
{
  GType gtype;
  GfBackend *backend;
  GfBackendPrivate *priv;
  GError *error;

  switch (type)
    {
      case GF_BACKEND_TYPE_X11_CM:
        gtype = GF_TYPE_BACKEND_X11_CM;
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  backend = g_object_new (gtype, NULL);
  priv = gf_backend_get_instance_private (backend);

  error = NULL;
  if (!g_initable_init (G_INITABLE (backend), NULL, &error))
    {
      g_warning ("Failed to create backend: %s", error->message);

      g_object_unref (backend);
      g_error_free (error);

      return NULL;
    }

  GF_BACKEND_GET_CLASS (backend)->post_init (backend);
  gf_settings_post_init (priv->settings);

  return backend;
}

GfMonitorManager *
gf_backend_get_monitor_manager (GfBackend *backend)
{
  GfBackendPrivate *priv;

  priv = gf_backend_get_instance_private (backend);

  return priv->monitor_manager;
}

GfOrientationManager *
gf_backend_get_orientation_manager (GfBackend *backend)
{
  GfBackendPrivate *priv;

  priv = gf_backend_get_instance_private (backend);

  return priv->orientation_manager;
}

GfSettings *
gf_backend_get_settings (GfBackend *backend)
{
  GfBackendPrivate *priv;

  priv = gf_backend_get_instance_private (backend);

  return priv->settings;
}

void
gf_backend_monitors_changed (GfBackend *backend)
{
}

gboolean
gf_backend_is_lid_closed (GfBackend *self)
{
  return GF_BACKEND_GET_CLASS (self)->is_lid_closed (self);
}

void
gf_backend_add_gpu (GfBackend *self,
                    GfGpu     *gpu)
{
  GfBackendPrivate *priv;

  priv = gf_backend_get_instance_private (self);

  priv->gpus = g_list_append (priv->gpus, gpu);

  g_signal_emit (self, backend_signals[GPU_ADDED], 0, gpu);
}

GList *
gf_backend_get_gpus (GfBackend *self)
{
  GfBackendPrivate *priv;

  priv = gf_backend_get_instance_private (self);

  return priv->gpus;
}
