/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
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
 * Adapted from mutter:
 * - src/backends/meta-monitor-manager-dummy.c
 */

#include "config.h"
#include "gf-monitor-manager-dummy-private.h"

struct _GfMonitorManagerDummy
{
  GfMonitorManager parent;
};

G_DEFINE_TYPE (GfMonitorManagerDummy, gf_monitor_manager_dummy, GF_TYPE_MONITOR_MANAGER)

static void
gf_monitor_manager_dummy_class_init (GfMonitorManagerDummyClass *dummy_class)
{
}

static void
gf_monitor_manager_dummy_init (GfMonitorManagerDummy *dummy)
{
}