/*
 * Copyright (C) 2015 Sebastian Geiger
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

#ifndef GF_INPUT_SOURCE_H
#define GF_INPUT_SOURCE_H

#include <glib-object.h>
#include "gf-ibus-manager.h"

#define GF_TYPE_INPUT_SOURCE gf_input_source_get_type ()
G_DECLARE_FINAL_TYPE (GfInputSource, gf_input_source, GF, INPUT_SOURCE, GObject)

GfInputSource *gf_input_source_new (GfIBusManager *ibus_manager,
                                    const char    *id,
                                    const char    *display_name,
                                    const char    *short_name,
                                    guint          index);

#endif
