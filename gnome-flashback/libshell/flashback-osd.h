/*
 * Copyright (C) 2015-2019 Alberts Muktupāvels
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

#ifndef FLASHBACK_OSD_H
#define FLASHBACK_OSD_H

#include "flashback-shell.h"

G_BEGIN_DECLS

#define FLASHBACK_TYPE_OSD flashback_osd_get_type ()
G_DECLARE_FINAL_TYPE (FlashbackOsd, flashback_osd, FLASHBACK, OSD, GObject)

FlashbackOsd *flashback_osd_new  (void);

void          flashback_osd_show (FlashbackOsd     *osd,
                                  GfMonitorManager *monitor_manager,
                                  GVariant         *params);

G_END_DECLS

#endif
