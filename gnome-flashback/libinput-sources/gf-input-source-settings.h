/*
 * Copyright (C) 2015 Alberts Muktupāvels
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

#ifndef GF_INPUT_SOURCE_SETTINGS_H
#define GF_INPUT_SOURCE_SETTINGS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GF_TYPE_INPUT_SOURCE_SETTINGS gf_input_source_settings_get_type ()
G_DECLARE_FINAL_TYPE (GfInputSourceSettings, gf_input_source_settings,
                      GF, INPUT_SOURCE_SETTINGS, GObject)

GfInputSourceSettings  *gf_input_source_settings_new             (void);

GVariant               *gf_input_source_settings_get_sources     (GfInputSourceSettings *settings);

void                    gf_input_source_settings_set_mru_sources (GfInputSourceSettings *settings,
                                                                  GVariant              *mru_sources);

GVariant               *gf_input_source_settings_get_mru_sources (GfInputSourceSettings *settings);

gchar                 **gf_input_source_settings_get_xkb_options (GfInputSourceSettings *settings);

gboolean                gf_input_source_settings_get_per_window  (GfInputSourceSettings *settings);

G_END_DECLS

#endif
