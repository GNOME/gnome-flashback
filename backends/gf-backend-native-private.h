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
 * - src/backends/native/meta-backend-native.h
 */

#ifndef GF_BACKEND_NATIVE_PRIVATE_H
#define GF_BACKEND_NATIVE_PRIVATE_H

#include "gf-backend-private.h"

G_BEGIN_DECLS

#define GF_TYPE_BACKEND_NATIVE (gf_backend_native_get_type ())
G_DECLARE_FINAL_TYPE (GfBackendNative, gf_backend_native,
                      GF, BACKEND_NATIVE, GfBackend)

G_END_DECLS

#endif
