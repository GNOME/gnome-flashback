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

#ifndef GF_INPUT_SOURCE_IBUS_H
#define GF_INPUT_SOURCE_IBUS_H

#include "gf-input-source.h"

G_BEGIN_DECLS

#define GF_TYPE_INPUT_SOURCE_IBUS (gf_input_source_ibus_get_type ())
G_DECLARE_FINAL_TYPE (GfInputSourceIBus, gf_input_source_ibus,
                      GF, INPUT_SOURCE_IBUS, GfInputSource)

G_END_DECLS

#endif
