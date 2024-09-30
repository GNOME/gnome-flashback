/*
 * Copyright (C) 2014 Red Hat
 * Copyright (C) 2017 Alberts Muktupāvels
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
 * - src/meta/meta-backend.h
 */

#ifndef GF_BACKEND_H
#define GF_BACKEND_H

#include "gf-monitor-manager.h"
#include "gf-settings.h"

G_BEGIN_DECLS

#define GF_TYPE_BACKEND (gf_backend_get_type ())
G_DECLARE_DERIVABLE_TYPE (GfBackend, gf_backend, GF, BACKEND, GObject)

typedef enum
{
  GF_BACKEND_TYPE_X11_CM
} GfBackendType;

GfBackend        *gf_backend_new                 (GfBackendType  type);

GfMonitorManager *gf_backend_get_monitor_manager (GfBackend     *backend);

GfSettings       *gf_backend_get_settings        (GfBackend     *backend);

G_END_DECLS

#endif
