/*
 * Copyright (C) 2004-2005 William Jon McCann
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     William Jon McCann <mccann@jhu.edu>
 */

#ifndef GF_MANAGER_H
#define GF_MANAGER_H

#include "backends/gf-monitor-manager.h"
#include "libinput-sources/gf-input-sources.h"

#include "gf-fade.h"
#include "gf-grab.h"

G_BEGIN_DECLS

#define GF_TYPE_MANAGER (gf_manager_get_type ())
G_DECLARE_FINAL_TYPE (GfManager, gf_manager, GF, MANAGER, GObject)

GfManager *gf_manager_new                     (GfGrab           *grab,
                                               GfFade           *fade);

void       gf_manager_set_monitor_manager     (GfManager        *self,
                                               GfMonitorManager *monitor_manager);

void       gf_manager_set_input_sources       (GfManager        *self,
                                               GfInputSources   *input_sources);

gboolean   gf_manager_set_active              (GfManager        *self,
                                               gboolean          active);

gboolean   gf_manager_get_active              (GfManager        *self);

gboolean   gf_manager_get_lock_active         (GfManager        *self);

void       gf_manager_set_lock_active         (GfManager        *self,
                                               gboolean          lock_active);

gboolean   gf_manager_get_lock_enabled        (GfManager        *self);

void       gf_manager_set_lock_enabled        (GfManager        *self,
                                               gboolean          lock_enabled);

void       gf_manager_set_lock_timeout        (GfManager        *self,
                                               glong             lock_timeout);

void       gf_manager_set_user_switch_enabled (GfManager        *self,
                                               gboolean          user_switch_enabled);

void       gf_manager_show_message            (GfManager        *self,
                                               const char       *summary,
                                               const char       *body,
                                               const char       *icon);

void       gf_manager_request_unlock          (GfManager        *self);

G_END_DECLS

#endif
