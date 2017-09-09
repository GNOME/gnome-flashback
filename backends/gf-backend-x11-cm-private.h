/*
 * Copyright (C) 2017 Alberts Muktupāvels
 * Copyright (C) 2017 Red Hat
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
 * Adapted from mutter:
 * - src/backends/x11/cm/meta-backend-x11-cm.h
 */

#ifndef GF_BACKEND_X11_CM_PRIVATE_H
#define GF_BACKEND_X11_CM_PRIVATE_H

#include "gf-backend-x11-private.h"

G_BEGIN_DECLS

#define GF_TYPE_BACKEND_X11_CM (gf_backend_x11_cm_get_type ())
G_DECLARE_FINAL_TYPE (GfBackendX11Cm, gf_backend_x11_cm,
                      GF, BACKEND_X11_CM, GfBackendX11)

G_END_DECLS

#endif
