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

#ifndef GF_INPUT_SOURCE_MANAGER_H
#define GF_INPUT_SOURCE_MANAGER_H

#include <glib-object.h>
#include "gf-ibus-manager.h"
#include "gf-input-source.h"

#define GF_TYPE_INPUT_SOURCE_MANAGER gf_input_source_manager_get_type ()
G_DECLARE_FINAL_TYPE (GfInputSourceManager, gf_input_source_manager,
                      GF, INPUT_SOURCE_MANAGER, GObject)

GfInputSourceManager *gf_input_source_manager_new                (GfIBusManager        *manager);

void                  gf_input_source_manager_reload             (GfInputSourceManager *manager);

GfInputSource        *gf_input_source_manager_get_current_source (GfInputSourceManager *manager);

GList                *gf_input_source_manager_get_input_sources  (GfInputSourceManager *manager);

void                  gf_input_source_manager_activate_next_source (GfInputSourceManager *manager);

#endif
