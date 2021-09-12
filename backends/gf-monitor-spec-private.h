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
 * - src/backends/meta-monitor.h
 */

#ifndef GF_MONITOR_SPEC_PRIVATE_H
#define GF_MONITOR_SPEC_PRIVATE_H

#include "gf-monitor-manager-types-private.h"

G_BEGIN_DECLS

struct _GfMonitorSpec
{
  gchar *connector;
  gchar *vendor;
  gchar *product;
  gchar *serial;
};

GfMonitorSpec *gf_monitor_spec_clone   (GfMonitorSpec  *spec);

guint          gf_monitor_spec_hash    (gconstpointer   key);

gboolean       gf_monitor_spec_equals  (GfMonitorSpec  *spec,
                                        GfMonitorSpec  *other_spec);

gint           gf_monitor_spec_compare (GfMonitorSpec  *spec_a,
                                        GfMonitorSpec  *spec_b);

void           gf_monitor_spec_free    (GfMonitorSpec  *spec);

gboolean       gf_verify_monitor_spec  (GfMonitorSpec  *spec,
                                        GError        **error);

G_END_DECLS

#endif
