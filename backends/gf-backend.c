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

  GList                *gpus;
} GfBackendPrivate;

enum
{
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

  object_class->dispose = gf_backend_dispose;
  object_class->finalize = gf_backend_finalize;

  backend_class->post_init = gf_backend_real_post_init;

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
