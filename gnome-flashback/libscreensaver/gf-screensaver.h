/*
 * Copyright (C) 2016-2020 Alberts Muktupāvels
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

#ifndef GF_SCREENSAVER_H
#define GF_SCREENSAVER_H

#include "backends/gf-monitor-manager.h"
#include "libinput-sources/gf-input-sources.h"

G_BEGIN_DECLS

#define GF_TYPE_SCREENSAVER gf_screensaver_get_type ()
G_DECLARE_FINAL_TYPE (GfScreensaver, gf_screensaver, GF, SCREENSAVER, GObject)

GfScreensaver *gf_screensaver_new                 (void);

void           gf_screensaver_set_monitor_manager (GfScreensaver    *self,
                                                   GfMonitorManager *monitor_manager);

void           gf_screensaver_set_input_sources   (GfScreensaver    *self,
                                                   GfInputSources   *input_sources);

G_END_DECLS

#endif
