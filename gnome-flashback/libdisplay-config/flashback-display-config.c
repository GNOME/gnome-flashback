/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2014 Alberts Muktupāvels
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
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>
#include "flashback-confirm-dialog.h"
#include "flashback-display-config.h"
#include "flashback-monitor-manager.h"

struct _FlashbackDisplayConfig
{
  GObject                  parent;
  FlashbackMonitorManager *manager;
};

G_DEFINE_TYPE (FlashbackDisplayConfig, flashback_display_config, G_TYPE_OBJECT)

static void
flashback_display_config_finalize (GObject *object)
{
  FlashbackDisplayConfig *config;

  config = FLASHBACK_DISPLAY_CONFIG (object);

  g_clear_object (&config->manager);

  G_OBJECT_CLASS (flashback_display_config_parent_class)->finalize (object);
}

static void
flashback_display_config_class_init (FlashbackDisplayConfigClass *config_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (config_class);

  object_class->finalize = flashback_display_config_finalize;
}

static void
flashback_display_config_init (FlashbackDisplayConfig *config)
{
  config->manager = flashback_monitor_manager_new ();
}

FlashbackDisplayConfig *
flashback_display_config_new (void)
{
  return g_object_new (FLASHBACK_TYPE_DISPLAY_CONFIG,
                       NULL);
}

/**
 * flashback_display_config_get_monitor_manager:
 * @config: a #FlashbackMonitorManager
 *
 * Returns: (transfer none):
 */
FlashbackMonitorManager *
flashback_display_config_get_monitor_manager (FlashbackDisplayConfig *config)
{
  if (config == NULL)
    return NULL;

  return config->manager;
}
