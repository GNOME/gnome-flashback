/*
 * Copyright (C) 2005, 2006 Elijah Newren
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
 * Adapted from mutter:
 * - src/core/boxes-private.h
 */

#ifndef GF_RECTANGLE_PRIVATE_H
#define GF_RECTANGLE_PRIVATE_H

#include "gf-rectangle.h"

G_BEGIN_DECLS

gboolean gf_rectangle_overlaps_with_region         (const GList       *spanning_rects,
                                                    const GfRectangle *rect);

gboolean gf_rectangle_is_adjacent_to_any_in_region (const GList       *spanning_rects,
                                                    GfRectangle       *rect);

gboolean gf_rectangle_is_adjacent_to               (const GfRectangle *rect,
                                                    const GfRectangle *other);

G_END_DECLS

#endif
