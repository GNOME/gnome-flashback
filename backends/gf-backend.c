/*
 * Copyright (C) 2014 Red Hat
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
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 *
 * Adapted from mutter:
 * - src/backends/meta-backend.c
 */

#include "config.h"

#include <gio/gio.h>

#include "gf-backend-native-private.h"
#include "gf-backend-x11-cm-private.h"
#include "gf-backend-x11-nested-private.h"
#include "gf-settings-private.h"

typedef struct
{
  GfSettings *settings;
} GfBackendPrivate;

static void
initable_iface_init (GInitableIface *initable_iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GfBackend, gf_backend, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (GfBackend)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         initable_iface_init))

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

  g_clear_object (&priv->settings);

  G_OBJECT_CLASS (gf_backend_parent_class)->dispose (object);
}

static void
gf_backend_class_init (GfBackendClass *backend_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (backend_class);

  object_class->dispose = gf_backend_dispose;
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
  GError *error;

  switch (type)
    {
      case GF_BACKEND_TYPE_X11_CM:
        gtype = GF_TYPE_BACKEND_X11_CM;
        break;

      case GF_BACKEND_TYPE_X11_NESTED:
        gtype = GF_TYPE_BACKEND_X11_NESTED;
        break;

      case GF_BACKEND_TYPE_NATIVE:
        gtype = GF_TYPE_BACKEND_NATIVE;
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  backend = g_object_new (gtype, NULL);

  error = NULL;
  if (!g_initable_init (G_INITABLE (backend), NULL, &error))
    {
      g_warning ("Failed to create backend: %s", error->message);

      g_object_unref (backend);
      g_error_free (error);

      return NULL;
    }

  return backend;
}
