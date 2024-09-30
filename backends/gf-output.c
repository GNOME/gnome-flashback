/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2017-2019 Alberts Muktupāvels
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
 * - src/backends/meta-monitor-manager.c
 */

#include "config.h"
#include "gf-output-private.h"

#include "gf-crtc-private.h"

typedef struct
{
  uint64_t      id;

  GfGpu        *gpu;

  GfOutputInfo *info;

  GfMonitor    *monitor;

  /* The CRTC driving this output, NULL if the output is not enabled */
  GfCrtc       *crtc;

  gboolean      is_primary;
  gboolean      is_presentation;

  gboolean      is_underscanning;

  gboolean      has_max_bpc;
  unsigned int  max_bpc;

  int           backlight;
} GfOutputPrivate;

enum
{
  PROP_0,

  PROP_ID,
  PROP_GPU,
  PROP_INFO,

  LAST_PROP
};

static GParamSpec *output_properties[LAST_PROP] = { NULL };

enum
{
  BACKLIGHT_CHANGED,

  LAST_SIGNAL
};

static unsigned int output_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GfOutput, gf_output, G_TYPE_OBJECT)

static void
gf_output_dispose (GObject *object)
{
  GfOutput *output;
  GfOutputPrivate *priv;

  output = GF_OUTPUT (object);
  priv = gf_output_get_instance_private (output);

  g_clear_object (&priv->crtc);

  G_OBJECT_CLASS (gf_output_parent_class)->dispose (object);
}

static void
gf_output_finalize (GObject *object)
{
  GfOutput *output;
  GfOutputPrivate *priv;

  output = GF_OUTPUT (object);
  priv = gf_output_get_instance_private (output);

  g_clear_pointer (&priv->info, gf_output_info_unref);

  G_OBJECT_CLASS (gf_output_parent_class)->finalize (object);
}

static void
gf_output_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GfOutput *self;
  GfOutputPrivate *priv;

  self = GF_OUTPUT (object);
  priv = gf_output_get_instance_private (self);

  switch (property_id)
    {
      case PROP_ID:
        g_value_set_uint64 (value, priv->id);
        break;

      case PROP_GPU:
        g_value_set_object (value, priv->gpu);
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
gf_output_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GfOutput *self;
  GfOutputPrivate *priv;

  self = GF_OUTPUT (object);
  priv = gf_output_get_instance_private (self);

  switch (property_id)
    {
      case PROP_ID:
        priv->id = g_value_get_uint64 (value);
        break;

      case PROP_GPU:
        priv->gpu = g_value_get_object (value);
        break;

      case PROP_INFO:
        priv->info = gf_output_info_ref (g_value_get_boxed (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_output_class_init (GfOutputClass *output_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (output_class);

  object_class->dispose = gf_output_dispose;
  object_class->finalize = gf_output_finalize;
  object_class->get_property = gf_output_get_property;
  object_class->set_property = gf_output_set_property;

  output_properties[PROP_ID] =
    g_param_spec_uint64 ("id",
                         "id",
                         "CRTC id",
                         0,
                         UINT64_MAX,
                         0,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  output_properties[PROP_GPU] =
    g_param_spec_object ("gpu",
                         "GfGpu",
                         "GfGpu",
                         GF_TYPE_GPU,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  output_properties[PROP_INFO] =
    g_param_spec_boxed ("info",
                        "info",
                        "GfOutputInfo",
                        GF_TYPE_OUTPUT_INFO,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     output_properties);

  output_signals[BACKLIGHT_CHANGED] =
    g_signal_new ("backlight-changed",
                  G_TYPE_FROM_CLASS (output_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
gf_output_init (GfOutput *self)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  priv->backlight = -1;
}

uint64_t
gf_output_get_id (GfOutput *self)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  return priv->id;
}

GfGpu *
gf_output_get_gpu (GfOutput *output)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (output);

  return priv->gpu;
}

const GfOutputInfo *
gf_output_get_info (GfOutput *self)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  return priv->info;
}

GfMonitor *
gf_output_get_monitor (GfOutput *self)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  g_warn_if_fail (priv->monitor);

  return priv->monitor;
}

void
gf_output_set_monitor (GfOutput  *self,
                       GfMonitor *monitor)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  g_warn_if_fail (!priv->monitor);

  priv->monitor = monitor;
}

void
gf_output_unset_monitor (GfOutput *self)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  g_warn_if_fail (priv->monitor);

  priv->monitor = NULL;
}

const char *
gf_output_get_name (GfOutput *self)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  return priv->info->name;
}

void
gf_output_assign_crtc (GfOutput                 *self,
                       GfCrtc                   *crtc,
                       const GfOutputAssignment *output_assignment)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  g_assert (crtc);

  gf_output_unassign_crtc (self);

  g_set_object (&priv->crtc, crtc);

  gf_crtc_assign_output (crtc, self);

  priv->is_primary = output_assignment->is_primary;
  priv->is_presentation = output_assignment->is_presentation;
  priv->is_underscanning = output_assignment->is_underscanning;

  priv->has_max_bpc = output_assignment->has_max_bpc;

  if (priv->has_max_bpc)
    priv->max_bpc = output_assignment->max_bpc;
}

void
gf_output_unassign_crtc (GfOutput *output)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (output);

  if (priv->crtc != NULL)
    {
      gf_crtc_unassign_output (priv->crtc, output);
      g_clear_object (&priv->crtc);
    }

  priv->is_primary = FALSE;
  priv->is_presentation = FALSE;
}

GfCrtc *
gf_output_get_assigned_crtc (GfOutput *output)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (output);

  return priv->crtc;
}

gboolean
gf_output_is_laptop (GfOutput *output)
{
  const GfOutputInfo *output_info;

  output_info = gf_output_get_info (output);

  switch (output_info->connector_type)
    {
      case GF_CONNECTOR_TYPE_LVDS:
      case GF_CONNECTOR_TYPE_eDP:
      case GF_CONNECTOR_TYPE_DSI:
        return TRUE;

      case GF_CONNECTOR_TYPE_Unknown:
      case GF_CONNECTOR_TYPE_VGA:
      case GF_CONNECTOR_TYPE_DVII:
      case GF_CONNECTOR_TYPE_DVID:
      case GF_CONNECTOR_TYPE_DVIA:
      case GF_CONNECTOR_TYPE_Composite:
      case GF_CONNECTOR_TYPE_SVIDEO:
      case GF_CONNECTOR_TYPE_Component:
      case GF_CONNECTOR_TYPE_9PinDIN:
      case GF_CONNECTOR_TYPE_DisplayPort:
      case GF_CONNECTOR_TYPE_HDMIA:
      case GF_CONNECTOR_TYPE_HDMIB:
      case GF_CONNECTOR_TYPE_TV:
      case GF_CONNECTOR_TYPE_VIRTUAL:
      case GF_CONNECTOR_TYPE_DPI:
      case GF_CONNECTOR_TYPE_WRITEBACK:
      case GF_CONNECTOR_TYPE_SPI:
      case GF_CONNECTOR_TYPE_USB:
      default:
        break;
    }

  return FALSE;
}

GfMonitorTransform
gf_output_logical_to_crtc_transform (GfOutput           *output,
                                     GfMonitorTransform  transform)
{
  GfOutputPrivate *priv;
  GfMonitorTransform panel_orientation_transform;

  priv = gf_output_get_instance_private (output);

  panel_orientation_transform = priv->info->panel_orientation_transform;

  return gf_monitor_transform_transform (transform, panel_orientation_transform);
}

GfMonitorTransform
gf_output_crtc_to_logical_transform (GfOutput           *output,
                                     GfMonitorTransform  transform)
{
  GfOutputPrivate *priv;
  GfMonitorTransform panel_orientation_transform;
  GfMonitorTransform inverted_transform;

  priv = gf_output_get_instance_private (output);

  panel_orientation_transform = priv->info->panel_orientation_transform;
  inverted_transform = gf_monitor_transform_invert (panel_orientation_transform);

  return gf_monitor_transform_transform (transform, inverted_transform);
}

gboolean
gf_output_is_primary (GfOutput *self)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  return priv->is_primary;
}

gboolean
gf_output_is_presentation (GfOutput *self)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  return priv->is_presentation;
}

gboolean
gf_output_is_underscanning (GfOutput *self)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  return priv->is_underscanning;
}

gboolean
gf_output_get_max_bpc (GfOutput     *self,
                       unsigned int *max_bpc)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  if (priv->has_max_bpc && max_bpc != NULL)
    *max_bpc = priv->max_bpc;

  return priv->has_max_bpc;
}

void
gf_output_set_backlight (GfOutput *self,
                         int       backlight)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  g_return_if_fail (backlight >= priv->info->backlight_min);
  g_return_if_fail (backlight <= priv->info->backlight_max);

  priv->backlight = backlight;

  g_signal_emit (self, output_signals[BACKLIGHT_CHANGED], 0);
}

int
gf_output_get_backlight (GfOutput *self)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (self);

  return priv->backlight;
}

void
gf_output_add_possible_clone (GfOutput *self,
                              GfOutput *possible_clone)
{
  GfOutputPrivate *priv;
  GfOutputInfo *output_info;

  priv = gf_output_get_instance_private (self);

  output_info = priv->info;

  output_info->n_possible_clones++;
  output_info->possible_clones = g_renew (GfOutput *,
                                          output_info->possible_clones,
                                          output_info->n_possible_clones);
  output_info->possible_clones[output_info->n_possible_clones - 1] = possible_clone;
}
