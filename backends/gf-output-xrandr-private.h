/*
 * Copyright (C) 2017 Red Hat
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

#ifndef GF_OUTPUT_XRANDR_PRIVATE_H
#define GF_OUTPUT_XRANDR_PRIVATE_H

#include <X11/extensions/Xrandr.h>

#include "gf-monitor-manager-xrandr-private.h"
#include "gf-output-private.h"

G_BEGIN_DECLS

GfOutput *gf_create_xrandr_output           (GfMonitorManager *monitor_manager,
                                             XRROutputInfo    *xrandr_output,
                                             RROutput          output_id,
                                             RROutput          primary_output);

GBytes   *gf_output_xrandr_read_edid        (GfOutput         *output);

void      gf_output_xrandr_apply_mode       (GfOutput         *output);

void      gf_output_xrandr_change_backlight (GfOutput         *output,
                                             int               value);

G_END_DECLS

#endif
