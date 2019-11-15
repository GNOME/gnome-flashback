/*
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
 */

#ifndef GF_DESKTOP_ENUMS_H
#define GF_DESKTOP_ENUMS_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  GF_ICON_SIZE_16PX = 16,
  GF_ICON_SIZE_22PX = 22,
  GF_ICON_SIZE_24PX = 24,
  GF_ICON_SIZE_32PX = 32,
  GF_ICON_SIZE_48PX = 48,
  GF_ICON_SIZE_64PX = 64,
  GF_ICON_SIZE_72PX = 72,
  GF_ICON_SIZE_96PX = 96,
  GF_ICON_SIZE_128PX = 128
} GfIconSize;

typedef enum
{
  GF_PLACEMENT_AUTO_ARRANGE_ICONS,
  GF_PLACEMENT_ALIGN_ICONS_TO_GRID,
  GF_PLACEMENT_FREE,

  GF_PLACEMENT_LAST /*< skip >*/
} GfPlacement;

typedef enum
{
  GF_SORT_BY_NAME,
  GF_SORT_BY_DATE_MODIFIED,
  GF_SORT_BY_SIZE,

  GF_SORT_BY_LAST /*< skip >*/
} GfSortBy;

G_END_DECLS

#endif
