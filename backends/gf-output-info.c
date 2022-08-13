/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2017-2020 Alberts Muktupāvels
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
 */

#include "config.h"
#include "gf-output-info-private.h"

#include "gf-edid-private.h"

G_DEFINE_BOXED_TYPE (GfOutputInfo,
                     gf_output_info,
                     gf_output_info_ref,
                     gf_output_info_unref)

GfOutputInfo *
gf_output_info_new (void)
{
  GfOutputInfo *self;

  self = g_new0 (GfOutputInfo, 1);
  g_ref_count_init (&self->ref_count);

  return self;
}

GfOutputInfo *
gf_output_info_ref (GfOutputInfo *self)
{
  g_ref_count_inc (&self->ref_count);

  return self;
}

void
gf_output_info_unref (GfOutputInfo *self)
{
  if (g_ref_count_dec (&self->ref_count))
    {
      g_free (self->name);
      g_free (self->vendor);
      g_free (self->product);
      g_free (self->serial);
      g_free (self->modes);
      g_free (self->possible_crtcs);
      g_free (self->possible_clones);
      g_free (self);
    }
}

void
gf_output_info_parse_edid (GfOutputInfo *output_info,
                           GBytes       *edid)
{
  GfEdidInfo *parsed_edid;
  gsize len;

  if (!edid)
    {
      output_info->vendor = g_strdup ("unknown");
      output_info->product = g_strdup ("unknown");
      output_info->serial = g_strdup ("unknown");
      return;
    }

  parsed_edid = gf_edid_info_new_parse (g_bytes_get_data (edid, &len));
  if (parsed_edid)
    {
      output_info->vendor = g_strndup (parsed_edid->manufacturer_code, 4);
      if (!g_utf8_validate (output_info->vendor, -1, NULL))
        g_clear_pointer (&output_info->vendor, g_free);

      output_info->product = g_strndup (parsed_edid->dsc_product_name, 14);
      if (!g_utf8_validate (output_info->product, -1, NULL) ||
          output_info->product[0] == '\0')
        {
          g_clear_pointer (&output_info->product, g_free);
          output_info->product = g_strdup_printf ("0x%04x", (unsigned) parsed_edid->product_code);
        }

      output_info->serial = g_strndup (parsed_edid->dsc_serial_number, 14);
      if (!g_utf8_validate (output_info->serial, -1, NULL) ||
          output_info->serial[0] == '\0')
        {
          g_clear_pointer (&output_info->serial, g_free);
          output_info->serial = g_strdup_printf ("0x%08x", parsed_edid->serial_number);
        }

      g_free (parsed_edid);
    }

  if (!output_info->vendor)
    output_info->vendor = g_strdup ("unknown");

  if (!output_info->product)
    output_info->product = g_strdup ("unknown");

  if (!output_info->serial)
    output_info->serial = g_strdup ("unknown");
}
