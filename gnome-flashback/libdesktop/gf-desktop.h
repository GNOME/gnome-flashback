/*
 * Copyright (C) 2019 Alberts Muktupāvels
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

#ifndef GF_DESKTOP_H
#define GF_DESKTOP_H

#include "backends/gf-monitor-manager.h"

G_BEGIN_DECLS

#define GF_TYPE_DESKTOP (gf_desktop_get_type ())
G_DECLARE_FINAL_TYPE (GfDesktop, gf_desktop, GF, DESKTOP, GObject)

GfDesktop *gf_desktop_new                 (void);

void       gf_desktop_set_monitor_manager (GfDesktop        *self,
                                           GfMonitorManager *monitor_manager);

G_END_DECLS

#endif
