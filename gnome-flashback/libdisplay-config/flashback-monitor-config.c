/*
 * Copyright (C) 2001 Havoc Pennington
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
 * - /src/backends/meta-monitor-config.c
 */

#include <config.h>
#include "flashback-monitor-config.h"

struct _FlashbackMonitorConfig
{
  GObject parent;
};

G_DEFINE_TYPE (FlashbackMonitorConfig, flashback_monitor_config, G_TYPE_OBJECT)

static void
flashback_monitor_config_finalize (GObject *object)
{
  G_OBJECT_CLASS (flashback_monitor_config_parent_class)->finalize (object);
}

static void
flashback_monitor_config_class_init (FlashbackMonitorConfigClass *config_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (config_class);

  object_class->finalize = flashback_monitor_config_finalize;
}

static void
flashback_monitor_config_init (FlashbackMonitorConfig *config)
{
}

FlashbackMonitorConfig *
flashback_monitor_config_new (void)
{
  return g_object_new (FLASHBACK_TYPE_MONITOR_CONFIG,
                       NULL);
}

gboolean
flashback_monitor_config_apply_stored (FlashbackMonitorConfig  *config,
                                       FlashbackMonitorManager *manager)
{
}

void
flashback_monitor_config_make_default (FlashbackMonitorConfig  *config,
                                       FlashbackMonitorManager *manager)
{
}

void
flashback_monitor_config_update_current (FlashbackMonitorConfig  *config,
                                         FlashbackMonitorManager *manager)
{
}

void
flashback_monitor_config_make_persistent (FlashbackMonitorConfig  *config)
{
}

void
flashback_monitor_config_restore_previous (FlashbackMonitorConfig  *config,
                                           FlashbackMonitorManager *manager)
{
}

void
meta_crtc_info_free (MetaCRTCInfo *info)
{
  g_ptr_array_free (info->outputs, TRUE);
  g_slice_free (MetaCRTCInfo, info);
}

void
meta_output_info_free (MetaOutputInfo *info)
{
  g_slice_free (MetaOutputInfo, info);
}
