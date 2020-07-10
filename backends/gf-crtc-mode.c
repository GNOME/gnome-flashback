/*
 * Copyright (C) 2017-2020 Red Hat
 * Copyright (C) 2018-2020 Alberts Muktupāvels
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
#include "gf-crtc-mode-private.h"

typedef struct
{
  uint64_t        id;
  char           *name;
  GfCrtcModeInfo *info;
} GfCrtcModePrivate;

enum
{
  PROP_0,

  PROP_ID,
  PROP_NAME,
  PROP_INFO,

  LAST_PROP
};

static GParamSpec *crtc_mode_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE_WITH_PRIVATE (GfCrtcMode, gf_crtc_mode, G_TYPE_OBJECT)

static void
gf_crtc_mode_finalize (GObject *object)
{
  GfCrtcMode *self;
  GfCrtcModePrivate *priv;

  self = GF_CRTC_MODE (object);
  priv = gf_crtc_mode_get_instance_private (self);

  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->info, gf_crtc_mode_info_unref);

  G_OBJECT_CLASS (gf_crtc_mode_parent_class)->finalize (object);
}

static void
gf_crtc_mode_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GfCrtcMode *self;
  GfCrtcModePrivate *priv;

  self = GF_CRTC_MODE (object);
  priv = gf_crtc_mode_get_instance_private (self);

  switch (property_id)
    {
      case PROP_ID:
        g_value_set_uint64 (value, priv->id);
        break;

      case PROP_NAME:
        g_value_set_string (value, priv->name);
        break;

      case PROP_INFO:
        g_value_set_boxed (value, priv->info);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_crtc_mode_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GfCrtcMode *self;
  GfCrtcModePrivate *priv;

  self = GF_CRTC_MODE (object);
  priv = gf_crtc_mode_get_instance_private (self);

  switch (property_id)
    {
      case PROP_ID:
        priv->id = g_value_get_uint64 (value);
        break;

      case PROP_NAME:
        priv->name = g_value_dup_string (value);
        break;

      case PROP_INFO:
        priv->info = gf_crtc_mode_info_ref (g_value_get_boxed (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_crtc_mode_class_init (GfCrtcModeClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->finalize = gf_crtc_mode_finalize;
  object_class->get_property = gf_crtc_mode_get_property;
  object_class->set_property = gf_crtc_mode_set_property;

  crtc_mode_properties[PROP_ID] =
    g_param_spec_uint64 ("id",
                         "id",
                         "CRTC mode id",
                         0,
                         UINT64_MAX,
                         0,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  crtc_mode_properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "name",
                         "Name of CRTC mode",
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  crtc_mode_properties[PROP_INFO] =
    g_param_spec_boxed ("info",
                        "info",
                        "GfCrtcModeInfo",
                        GF_TYPE_CRTC_MODE_INFO,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     LAST_PROP,
                                     crtc_mode_properties);
}

static void
gf_crtc_mode_init (GfCrtcMode *self)
{
}

uint64_t
gf_crtc_mode_get_id (GfCrtcMode *self)
{
  GfCrtcModePrivate *priv;

  priv = gf_crtc_mode_get_instance_private (self);

  return priv->id;
}

const char *
gf_crtc_mode_get_name (GfCrtcMode *self)
{
  GfCrtcModePrivate *priv;

  priv = gf_crtc_mode_get_instance_private (self);

  return priv->name;
}

const GfCrtcModeInfo *
gf_crtc_mode_get_info (GfCrtcMode *self)
{
  GfCrtcModePrivate *priv;

  priv = gf_crtc_mode_get_instance_private (self);

  return priv->info;
}
