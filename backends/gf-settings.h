/*
 * Copyright (C) 2024 Alberts Muktupāvels
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

#ifndef GF_SETTINGS_H
#define GF_SETTINGS_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GfSettings GfSettings;

int gf_settings_get_ui_scaling_factor (GfSettings *settings);

G_END_DECLS

#endif
