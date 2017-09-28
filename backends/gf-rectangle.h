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
 * - src/meta/boxes.h
 */

#ifndef GF_RECTANGLE_H
#define GF_RECTANGLE_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct
{
  gint x;
  gint y;
  gint width;
  gint height;
} GfRectangle;

gboolean gf_rectangle_equal         (const GfRectangle *src1,
                                     const GfRectangle *src2);

gboolean gf_rectangle_vert_overlap  (const GfRectangle *rect1,
                                     const GfRectangle *rect2);

gboolean gf_rectangle_horiz_overlap (const GfRectangle *rect1,
                                     const GfRectangle *rect2);

gboolean gf_rectangle_contains_rect (const GfRectangle *outer_rect,
                                     const GfRectangle *inner_rect);

G_END_DECLS

#endif
