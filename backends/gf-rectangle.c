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
 * - src/core/boxes.c
 */

#include "config.h"
#include "gf-rectangle-private.h"

static gboolean
gf_rectangle_overlap (const GfRectangle *rect1,
                      const GfRectangle *rect2)
{
  g_return_val_if_fail (rect1 != NULL, FALSE);
  g_return_val_if_fail (rect2 != NULL, FALSE);

  return !((rect1->x + rect1->width  <= rect2->x) ||
           (rect2->x + rect2->width  <= rect1->x) ||
           (rect1->y + rect1->height <= rect2->y) ||
           (rect2->y + rect2->height <= rect1->y));
}

gboolean
gf_rectangle_equal (const GfRectangle *src1,
                    const GfRectangle *src2)
{
  return ((src1->x == src2->x) &&
          (src1->y == src2->y) &&
          (src1->width == src2->width) &&
          (src1->height == src2->height));
}

gboolean
gf_rectangle_vert_overlap (const GfRectangle *rect1,
                           const GfRectangle *rect2)
{
  return (rect1->y < rect2->y + rect2->height &&
          rect2->y < rect1->y + rect1->height);
}

gboolean
gf_rectangle_horiz_overlap (const GfRectangle *rect1,
                            const GfRectangle *rect2)
{
  return (rect1->x < rect2->x + rect2->width &&
          rect2->x < rect1->x + rect1->width);
}

gboolean
gf_rectangle_contains_rect (const GfRectangle *outer_rect,
                            const GfRectangle *inner_rect)
{
  return inner_rect->x >= outer_rect->x &&
         inner_rect->y >= outer_rect->y &&
         inner_rect->x + inner_rect->width <= outer_rect->x + outer_rect->width &&
         inner_rect->y + inner_rect->height <= outer_rect->y + outer_rect->height;
}

gboolean
gf_rectangle_overlaps_with_region (const GList       *spanning_rects,
                                   const GfRectangle *rect)
{
  const GList *temp;
  gboolean overlaps;

  temp = spanning_rects;
  overlaps = FALSE;
  while (!overlaps && temp != NULL)
    {
      overlaps = overlaps || gf_rectangle_overlap (temp->data, rect);
      temp = temp->next;
    }

  return overlaps;
}

gboolean
gf_rectangle_is_adjacent_to_any_in_region (const GList *spanning_rects,
                                           GfRectangle *rect)
{
  const GList *l;

  for (l = spanning_rects; l; l = l->next)
    {
      GfRectangle *other = (GfRectangle *) l->data;

      if (rect == other || gf_rectangle_equal (rect, other))
        continue;

      if (gf_rectangle_is_adjacent_to (rect, other))
        return TRUE;
    }

  return FALSE;
}

gboolean
gf_rectangle_is_adjacent_to (const GfRectangle *rect,
                             const GfRectangle *other)
{
  gint rect_x1 = rect->x;
  gint rect_y1 = rect->y;
  gint rect_x2 = rect->x + rect->width;
  gint rect_y2 = rect->y + rect->height;
  gint other_x1 = other->x;
  gint other_y1 = other->y;
  gint other_x2 = other->x + other->width;
  gint other_y2 = other->y + other->height;

  if ((rect_x1 == other_x2 || rect_x2 == other_x1) &&
      !(rect_y2 <= other_y1 || rect_y1 >= other_y2))
    return TRUE;
  else if ((rect_y1 == other_y2 || rect_y2 == other_y1) &&
           !(rect_x2 <= other_x1 || rect_x1 >= other_x2))
    return TRUE;
  else
    return FALSE;
}
