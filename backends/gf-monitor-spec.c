/*
 * Copyright (C) 2016 Red Hat
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
 * Adapted from mutter:
 * - src/backends/meta-monitor.c
 */

#include "config.h"

#include <gio/gio.h>
#include <string.h>

#include "gf-monitor-spec-private.h"

GfMonitorSpec *
gf_monitor_spec_clone (GfMonitorSpec *spec)
{
  GfMonitorSpec *new_spec;

  new_spec = g_new0 (GfMonitorSpec, 1);

  new_spec->connector = g_strdup (spec->connector);
  new_spec->vendor = g_strdup (spec->vendor);
  new_spec->product = g_strdup (spec->product);
  new_spec->serial = g_strdup (spec->serial);

  return new_spec;
}

guint
gf_monitor_spec_hash (gconstpointer key)
{
  const GfMonitorSpec *monitor_spec = key;

  return (g_str_hash (monitor_spec->connector) +
          g_str_hash (monitor_spec->vendor) +
          g_str_hash (monitor_spec->product) +
          g_str_hash (monitor_spec->serial));
}

gboolean
gf_monitor_spec_equals (GfMonitorSpec *spec,
                        GfMonitorSpec *other_spec)
{
  return (g_str_equal (spec->connector, other_spec->connector) &&
          g_str_equal (spec->vendor, other_spec->vendor) &&
          g_str_equal (spec->product, other_spec->product) &&
          g_str_equal (spec->serial, other_spec->serial));
}

gint
gf_monitor_spec_compare (GfMonitorSpec *spec_a,
                         GfMonitorSpec *spec_b)
{
  gint ret;

  ret = g_strcmp0 (spec_a->connector, spec_b->connector);
  if (ret != 0)
    return ret;

  ret = g_strcmp0 (spec_a->vendor, spec_b->vendor);
  if (ret != 0)
    return ret;

  ret = g_strcmp0 (spec_a->product, spec_b->product);
  if (ret != 0)
    return ret;

  return g_strcmp0 (spec_a->serial, spec_b->serial);
}

void
gf_monitor_spec_free (GfMonitorSpec *spec)
{
  g_free (spec->connector);
  g_free (spec->vendor);
  g_free (spec->product);
  g_free (spec->serial);
  g_free (spec);
}

gboolean
gf_verify_monitor_spec (GfMonitorSpec  *spec,
                        GError        **error)
{
  if (spec->connector &&
      spec->vendor &&
      spec->product &&
      spec->serial)
    {
      return TRUE;
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Monitor spec incomplete");

      return FALSE;
    }
}
