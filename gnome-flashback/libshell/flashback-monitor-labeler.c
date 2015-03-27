/*
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
 */

#include <config.h>
#include "flashback-monitor-labeler.h"

struct _FlashbackMonitorLabeler
{
  GObject parent;
};

G_DEFINE_TYPE (FlashbackMonitorLabeler, flashback_monitor_labeler, G_TYPE_OBJECT)

static void
flashback_monitor_labeler_class_init (FlashbackMonitorLabelerClass *labeler_class)
{
}

static void
flashback_monitor_labeler_init (FlashbackMonitorLabeler *labeler)
{
}

FlashbackMonitorLabeler *
flashback_monitor_labeler_new (void)
{
  return g_object_new (FLASHBACK_TYPE_MONITOR_LABELER, NULL);
}

void
flashback_monitor_labeler_show (FlashbackMonitorLabeler *labeler,
                                const gchar             *sender,
                                GVariant                *params)
{
  g_warning ("shell: show monitor labels");
}

void
flashback_monitor_labeler_hide (FlashbackMonitorLabeler *labeler,
                                const gchar             *sender)
{
  g_warning ("shell: hide monitor labels");
}
