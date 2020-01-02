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

#include "config.h"
#include "gf-input-source-ibus.h"

struct _GfInputSourceIBus
{
  GfInputSource parent;
};

G_DEFINE_TYPE (GfInputSourceIBus, gf_input_source_ibus, GF_TYPE_INPUT_SOURCE)

static void
gf_input_source_ibus_class_init (GfInputSourceIBusClass *self_class)
{
}

static void
gf_input_source_ibus_init (GfInputSourceIBus *self)
{
}
