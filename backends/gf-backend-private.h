/*
 * Copyright (C) 2014 Red Hat
 * Copyright (C) 2017-2019 Alberts Muktupāvels
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
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 *
 * Adapted from mutter:
 * - src/backends/meta-backend-private.h
 */

#ifndef GF_BACKEND_PRIVATE_H
#define GF_BACKEND_PRIVATE_H

#include "gf-backend.h"
#include "gf-monitor-manager-private.h"
#include "gf-monitor-manager-types-private.h"
#include "gf-orientation-manager-private.h"
#include "gf-settings-private.h"

G_BEGIN_DECLS

struct _GfBackendClass
{
  GObjectClass parent_class;

  void               (* post_init)              (GfBackend  *backend);

  GfMonitorManager * (* create_monitor_manager) (GfBackend  *backend,
                                                 GError    **error);

  gboolean           (* is_lid_closed)          (GfBackend  *self);

};

GfOrientationManager *gf_backend_get_orientation_manager (GfBackend *backend);

void                  gf_backend_monitors_changed        (GfBackend *backend);

gboolean              gf_backend_is_lid_closed           (GfBackend *self);

void                  gf_backend_add_gpu                 (GfBackend *self,
                                                          GfGpu     *gpu);

GList                *gf_backend_get_gpus                (GfBackend *self);

G_END_DECLS

#endif
