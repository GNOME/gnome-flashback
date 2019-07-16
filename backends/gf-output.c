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
#include "gf-edid-private.h"
#include "gf-output-private.h"

typedef struct
{
  /* The CRTC driving this output, NULL if the output is not enabled */
  GfCrtc *crtc;
} GfOutputPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GfOutput, gf_output, G_TYPE_OBJECT)

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

  output = GF_OUTPUT (object);

  g_free (output->name);
  g_free (output->vendor);
  g_free (output->product);
  g_free (output->serial);
  g_free (output->modes);
  g_free (output->possible_crtcs);
  g_free (output->possible_clones);

  if (output->driver_notify)
    output->driver_notify (output);

  G_OBJECT_CLASS (gf_output_parent_class)->finalize (object);
}

static void
gf_output_class_init (GfOutputClass *output_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (output_class);

  object_class->dispose = gf_output_dispose;
  object_class->finalize = gf_output_finalize;
}

static void
gf_output_init (GfOutput *output)
{
}

GfGpu *
gf_output_get_gpu (GfOutput *output)
{
  return output->gpu;
}

void
gf_output_assign_crtc (GfOutput *output,
                       GfCrtc   *crtc)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (output);

  g_assert (crtc);

  g_set_object (&priv->crtc, crtc);
}

void
gf_output_unassign_crtc (GfOutput *output)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (output);

  g_clear_object (&priv->crtc);
}

GfCrtc *
gf_output_get_assigned_crtc (GfOutput *output)
{
  GfOutputPrivate *priv;

  priv = gf_output_get_instance_private (output);

  return priv->crtc;
}

void
gf_output_parse_edid (GfOutput *output,
                      GBytes   *edid)
{
  MonitorInfo *parsed_edid;
  gsize len;

  if (!edid)
    {
      output->vendor = g_strdup ("unknown");
      output->product = g_strdup ("unknown");
      output->serial = g_strdup ("unknown");
      return;
    }

  parsed_edid = decode_edid (g_bytes_get_data (edid, &len));
  if (parsed_edid)
    {
      output->vendor = g_strndup (parsed_edid->manufacturer_code, 4);
      if (!g_utf8_validate (output->vendor, -1, NULL))
        g_clear_pointer (&output->vendor, g_free);

      output->product = g_strndup (parsed_edid->dsc_product_name, 14);
      if (!g_utf8_validate (output->product, -1, NULL) || output->product[0] == '\0')
        {
          g_clear_pointer (&output->product, g_free);
          output->product = g_strdup_printf ("0x%04x", (unsigned) parsed_edid->product_code);
        }

      output->serial = g_strndup (parsed_edid->dsc_serial_number, 14);
      if (!g_utf8_validate (output->serial, -1, NULL) || output->serial[0] == '\0')
        {
          g_clear_pointer (&output->serial, g_free);
          output->serial = g_strdup_printf ("0x%08x", parsed_edid->serial_number);
        }

      g_free (parsed_edid);
    }

  if (!output->vendor)
    output->vendor = g_strdup ("unknown");

  if (!output->product)
    output->product = g_strdup ("unknown");

  if (!output->serial)
    output->serial = g_strdup ("unknown");
}

gboolean
gf_output_is_laptop (GfOutput *output)
{
  switch (output->connector_type)
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
      default:
        break;
    }

  return FALSE;
}
