/*
 * Copyright (C) 2017 Red Hat
 * Copyright (C) 2018-2019 Alberts Muktupāvels
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
#include "gf-crtc-private.h"

#include "gf-gpu-private.h"

typedef struct
{
  uint64_t  id;

  GfGpu    *gpu;
} GfCrtcPrivate;

enum
{
  PROP_0,

  PROP_ID,
  PROP_GPU,

  LAST_PROP
};

static GParamSpec *crtc_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE_WITH_PRIVATE (GfCrtc, gf_crtc, G_TYPE_OBJECT)

static void
gf_crtc_finalize (GObject *object)
{
  GfCrtc *crtc;

  crtc = GF_CRTC (object);

  if (crtc->driver_notify)
    crtc->driver_notify (crtc);

  g_clear_pointer (&crtc->config, g_free);

  G_OBJECT_CLASS (gf_crtc_parent_class)->finalize (object);
}

static void
gf_crtc_get_property (GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  GfCrtc *self;
  GfCrtcPrivate *priv;

  self = GF_CRTC (object);
  priv = gf_crtc_get_instance_private (self);

  switch (property_id)
    {
      case PROP_ID:
        g_value_set_uint64 (value, priv->id);
        break;

      case PROP_GPU:
        g_value_set_object (value, priv->gpu);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_crtc_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  GfCrtc *self;
  GfCrtcPrivate *priv;

  self = GF_CRTC (object);
  priv = gf_crtc_get_instance_private (self);

  switch (property_id)
    {
      case PROP_ID:
        priv->id = g_value_get_uint64 (value);
        break;

      case PROP_GPU:
        priv->gpu = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_crtc_class_init (GfCrtcClass *crtc_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (crtc_class);

  object_class->finalize = gf_crtc_finalize;
  object_class->get_property = gf_crtc_get_property;
  object_class->set_property = gf_crtc_set_property;

  crtc_properties[PROP_ID] =
    g_param_spec_uint64 ("id",
                         "id",
                         "id",
                         0,
                         UINT64_MAX,
                         0,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  crtc_properties[PROP_GPU] =
    g_param_spec_object ("gpu",
                         "GfGpu",
                         "GfGpu",
                         GF_TYPE_GPU,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     crtc_properties);
}

static void
gf_crtc_init (GfCrtc *crtc)
{
}

uint64_t
gf_crtc_get_id (GfCrtc *self)
{
  GfCrtcPrivate *priv;

  priv = gf_crtc_get_instance_private (self);

  return priv->id;
}

GfGpu *
gf_crtc_get_gpu (GfCrtc *crtc)
{
  GfCrtcPrivate *priv;

  priv = gf_crtc_get_instance_private (crtc);

  return priv->gpu;
}

void
gf_crtc_set_config (GfCrtc             *crtc,
                    GfRectangle        *layout,
                    GfCrtcMode         *mode,
                    GfMonitorTransform  transform)
{
  GfCrtcConfig *config;

  gf_crtc_unset_config (crtc);

  config = g_new0 (GfCrtcConfig, 1);
  config->layout = *layout;
  config->mode = mode;
  config->transform = transform;

  crtc->config = config;
}

void
gf_crtc_unset_config (GfCrtc *crtc)
{
  g_clear_pointer (&crtc->config, g_free);
}
