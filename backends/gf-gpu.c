/*
 * Copyright (C) 2017 Red Hat
 * Copyright (C) 2019 Alberts Muktupāvels
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
#include "gf-gpu-private.h"

#include "gf-backend-private.h"
#include "gf-output-private.h"

typedef struct
{
  GfBackend *backend;

  GList     *outputs;
  GList     *crtcs;
  GList     *modes;
} GfGpuPrivate;

enum
{
  PROP_0,

  PROP_BACKEND,

  LAST_PROP
};

static GParamSpec *gpu_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE_WITH_PRIVATE (GfGpu, gf_gpu, G_TYPE_OBJECT)

static void
gf_gpu_finalize (GObject *object)
{
  GfGpu *gpu;
  GfGpuPrivate *priv;

  gpu = GF_GPU (object);
  priv = gf_gpu_get_instance_private (gpu);

  g_list_free_full (priv->outputs, g_object_unref);
  g_list_free_full (priv->modes, g_object_unref);
  g_list_free_full (priv->crtcs, g_object_unref);

  G_OBJECT_CLASS (gf_gpu_parent_class)->finalize (object);
}

static void
gf_gpu_get_property (GObject    *object,
                     guint       property_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
  GfGpu *gpu;
  GfGpuPrivate *priv;

  gpu = GF_GPU (object);
  priv = gf_gpu_get_instance_private (gpu);

  switch (property_id)
    {
      case PROP_BACKEND:
        g_value_set_object (value, priv->backend);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_gpu_set_property (GObject      *object,
                     guint         property_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
  GfGpu *gpu;
  GfGpuPrivate *priv;

  gpu = GF_GPU (object);
  priv = gf_gpu_get_instance_private (gpu);

  switch (property_id)
    {
      case PROP_BACKEND:
        priv->backend = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_gpu_install_properties (GObjectClass *object_class)
{
  gpu_properties[PROP_BACKEND] =
    g_param_spec_object ("backend",
                         "GfBackend",
                         "GfBackend",
                         GF_TYPE_BACKEND,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     gpu_properties);
}

static void
gf_gpu_class_init (GfGpuClass *gpu_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (gpu_class);

  object_class->finalize = gf_gpu_finalize;
  object_class->get_property = gf_gpu_get_property;
  object_class->set_property = gf_gpu_set_property;

  gf_gpu_install_properties (object_class);
}

static void
gf_gpu_init (GfGpu *gpu)
{
}

gboolean
gf_gpu_read_current (GfGpu   *gpu,
                     GError **error)
{
  GfGpuPrivate *priv;
  gboolean ret;
  GList *old_outputs;
  GList *old_crtcs;
  GList *old_modes;

  priv = gf_gpu_get_instance_private (gpu);

  /* TODO: Get rid of this when objects incref:s what they need instead. */
  old_outputs = priv->outputs;
  old_crtcs = priv->crtcs;
  old_modes = priv->modes;

  ret = GF_GPU_GET_CLASS (gpu)->read_current (gpu, error);

  g_list_free_full (old_outputs, g_object_unref);
  g_list_free_full (old_modes, g_object_unref);
  g_list_free_full (old_crtcs, g_object_unref);

  return ret;
}

gboolean
gf_gpu_has_hotplug_mode_update (GfGpu *gpu)
{
  GfGpuPrivate *priv;
  GList *l;

  priv = gf_gpu_get_instance_private (gpu);

  for (l = priv->outputs; l; l = l->next)
    {
      GfOutput *output;
      const GfOutputInfo *output_info;

      output = l->data;
      output_info = gf_output_get_info (output);

      if (output_info->hotplug_mode_update)
        return TRUE;
    }

  return FALSE;
}

GfBackend *
gf_gpu_get_backend (GfGpu *self)
{
  GfGpuPrivate *priv;

  priv = gf_gpu_get_instance_private (self);

  return priv->backend;
}

GList *
gf_gpu_get_outputs (GfGpu *gpu)
{
  GfGpuPrivate *priv;

  priv = gf_gpu_get_instance_private (gpu);

  return priv->outputs;
}

GList *
gf_gpu_get_crtcs (GfGpu *gpu)
{
  GfGpuPrivate *priv;

  priv = gf_gpu_get_instance_private (gpu);

  return priv->crtcs;
}

GList *
gf_gpu_get_modes (GfGpu *gpu)
{
  GfGpuPrivate *priv;

  priv = gf_gpu_get_instance_private (gpu);

  return priv->modes;
}

void
gf_gpu_take_outputs (GfGpu *gpu,
                     GList *outputs)
{
  GfGpuPrivate *priv;

  priv = gf_gpu_get_instance_private (gpu);

  priv->outputs = outputs;
}

void
gf_gpu_take_crtcs (GfGpu *gpu,
                   GList *crtcs)
{
  GfGpuPrivate *priv;

  priv = gf_gpu_get_instance_private (gpu);

  priv->crtcs = crtcs;
}

void
gf_gpu_take_modes (GfGpu *gpu,
                   GList *modes)
{
  GfGpuPrivate *priv;

  priv = gf_gpu_get_instance_private (gpu);

  priv->modes = modes;
}
