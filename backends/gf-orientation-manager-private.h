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
 * - src/backends/meta-orientation-manager.h
 */

#ifndef GF_ORIENTATION_MANAGER_PRIVATE_H
#define GF_ORIENTATION_MANAGER_PRIVATE_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
  GF_ORIENTATION_UNDEFINED,

  GF_ORIENTATION_NORMAL,
  GF_ORIENTATION_BOTTOM_UP,
  GF_ORIENTATION_LEFT_UP,
  GF_ORIENTATION_RIGHT_UP
} GfOrientation;

#define GF_TYPE_ORIENTATION_MANAGER (gf_orientation_manager_get_type ())
G_DECLARE_FINAL_TYPE (GfOrientationManager, gf_orientation_manager,
                      GF, ORIENTATION_MANAGER, GObject)

GfOrientationManager *gf_orientation_manager_new               (void);

GfOrientation         gf_orientation_manager_get_orientation   (GfOrientationManager *manager);

gboolean              gf_orientation_manager_has_accelerometer (GfOrientationManager *self);

G_END_DECLS

#endif
