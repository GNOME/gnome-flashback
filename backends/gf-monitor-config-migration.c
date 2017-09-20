/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013, 2017 Red Hat Inc.
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
 * - src/backends/meta-monitor-config-migration.c
 */

/*
 * Portions of this file are derived from:
 * - gnome-desktop/libgnome-desktop/gnome-rr-config.c
 *
 * Copyright 2007, 2008, Red Hat, Inc.
 * Copyright 2010 Giovanni Campagna
 *
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

#include "config.h"

#include <gio/gio.h>

#include "gf-monitor-config-migration-private.h"
#include "gf-monitor-config-store-private.h"

gboolean
gf_migrate_old_monitors_config (GfMonitorConfigStore  *config_store,
                                GFile                 *in_file,
                                GError               **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Not implemented");
  return FALSE;
}

gboolean
gf_migrate_old_user_monitors_config (GfMonitorConfigStore  *config_store,
                                     GError               **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Not implemented");
  return FALSE;
}

gboolean
gf_finish_monitors_config_migration (GfMonitorManager  *monitor_manager,
                                     GfMonitorsConfig  *config,
                                     GError           **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Not implemented");
  return FALSE;
}
