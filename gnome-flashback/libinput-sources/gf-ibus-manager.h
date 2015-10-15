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

#ifndef GF_IBUS_MANAGER_H
#define GF_IBUS_MANAGER_H

#include <glib-object.h>
#include <ibus-1.0/ibus.h>

#define GF_TYPE_IBUS_MANAGER gf_ibus_manager_get_type ()
G_DECLARE_FINAL_TYPE (GfIBusManager, gf_ibus_manager,
                      GF, IBUS_MANAGER, GObject)

GfIBusManager  *gf_ibus_manager_new               (void);

void            gf_ibus_manager_activate_property (GfIBusManager  *manager,
                                                   const gchar    *prop_name,
                                                   guint           prop_state);

IBusEngineDesc *gf_ibus_manager_get_engine_desc   (GfIBusManager  *manager,
                                                   const gchar    *id);

void            gf_ibus_manager_set_engine        (GfIBusManager  *manager,
                                                   const gchar    *id);

void            gf_ibus_manager_preload_engines   (GfIBusManager  *manager,
                                                   gchar         **engines);

#endif
