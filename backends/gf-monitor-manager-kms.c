/*
 * Copyright (C) 2013 Red Hat Inc.
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
 *     Giovanni Campagna <gcampagn@redhat.com>
 *
 * Adapted from mutter:
 * - src/backends/native/meta-monitor-manager-kms.c
 */

#include "config.h"
#include "gf-monitor-manager-kms-private.h"

struct _GfMonitorManagerKms
{
  GfMonitorManager parent;
};

G_DEFINE_TYPE (GfMonitorManagerKms, gf_monitor_manager_kms, GF_TYPE_MONITOR_MANAGER)

static void
gf_monitor_manager_kms_class_init (GfMonitorManagerKmsClass *kms_class)
{
}

static void
gf_monitor_manager_kms_init (GfMonitorManagerKms *kms)
{
}
