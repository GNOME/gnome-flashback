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
#include <ibus-1.0/ibus.h>

#include "gf-ibus-manager.h"

#define GF_TYPE_INPUT_SOURCE gf_input_source_get_type ()
G_DECLARE_FINAL_TYPE (GfInputSource, gf_input_source,
                      GF, INPUT_SOURCE, GObject)

GfInputSource *gf_input_source_new              (GfIBusManager *ibus_manager,
                                                 const gchar   *type,
                                                 const gchar   *id,
                                                 const gchar   *display_name,
                                                 const gchar   *short_name,
                                                 guint          index);

const gchar   *gf_input_source_get_source_type  (GfInputSource *source);

const gchar   *gf_input_source_get_id           (GfInputSource *source);

const gchar   *gf_input_source_get_display_name (GfInputSource *source);

const gchar   *gf_input_source_get_short_name   (GfInputSource *source);

void           gf_input_source_set_short_name   (GfInputSource *source,
                                                 const gchar   *short_name);

guint          gf_input_source_get_index        (GfInputSource *source);

const gchar   *gf_input_source_get_xkb_id       (GfInputSource *source);

void           gf_input_source_activate         (GfInputSource *source);

IBusPropList  *gf_input_source_get_properties   (GfInputSource *source);

void           gf_input_source_set_properties   (GfInputSource *source,
                                                 IBusPropList  *prop_list);

#endif
