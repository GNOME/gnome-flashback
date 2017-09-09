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
 * - src/backends/native/meta-backend-native.c
 */

#include "config.h"
#include "gf-backend-native-private.h"

struct _GfBackendNative
{
  GfBackend parent;
};

G_DEFINE_TYPE (GfBackendNative, gf_backend_native, GF_TYPE_BACKEND)

static void
gf_backend_native_class_init (GfBackendNativeClass *native_class)
{
}

static void
gf_backend_native_init (GfBackendNative *native)
{
}
