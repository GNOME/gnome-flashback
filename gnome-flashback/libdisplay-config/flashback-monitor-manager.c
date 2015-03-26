/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2015 Alberts Muktupāvels
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
 * Adapted from mutter 3.16.0:
 * - /src/backends/meta-monitor-manager.c
 */

#include <config.h>
#include "flashback-monitor-manager.h"

G_DEFINE_TYPE (FlashbackMonitorManager, flashback_monitor_manager, G_TYPE_OBJECT)

static void
flashback_monitor_manager_finalize (GObject *object)
{
  G_OBJECT_CLASS (flashback_monitor_manager_parent_class)->finalize (object);
}

static void
flashback_monitor_manager_class_init (FlashbackMonitorManagerClass *manager_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (manager_class);

  object_class->finalize = flashback_monitor_manager_finalize;
}

static void
flashback_monitor_manager_init (FlashbackMonitorManager *manager)
{
}

FlashbackMonitorManager *
flashback_monitor_manager_new (void)
{
  return g_object_new (FLASHBACK_TYPE_MONITOR_MANAGER,
                       NULL);
}

void
flashback_monitor_manager_apply_configuration (FlashbackMonitorManager  *manager,
                                               MetaCRTCInfo            **crtcs,
                                               unsigned int              n_crtcs,
                                               MetaOutputInfo          **outputs,
                                               unsigned int              n_outputs)
{
}

void
flashback_monitor_manager_change_backlight (FlashbackMonitorManager *manager,
					                                  MetaOutput              *output,
					                                  gint                     value)
{
}

void
flashback_monitor_manager_get_crtc_gamma (FlashbackMonitorManager  *manager,
                                          MetaCRTC                 *crtc,
                                          gsize                    *size,
                                          unsigned short          **red,
                                          unsigned short          **green,
                                          unsigned short          **blue)
{
  *size = 0;
  *red = NULL;
  *green = NULL;
  *blue = NULL;
}

void
flashback_monitor_manager_set_crtc_gamma (FlashbackMonitorManager *manager,
                                          MetaCRTC                *crtc,
                                          gsize                    size,
                                          unsigned short          *red,
                                          unsigned short          *green,
                                          unsigned short          *blue)
{
}

char *
flashback_monitor_manager_get_edid_file (FlashbackMonitorManager *manager,
                                         MetaOutput              *output)
{
  return NULL;
}

GBytes *
flashback_monitor_manager_read_edid (FlashbackMonitorManager *manager,
                                     MetaOutput              *output)
{
  return NULL;
}
