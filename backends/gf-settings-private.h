/*
 * Copyright (C) 2017-2019 Alberts Muktupāvels
 * Copyright (C) 2017 Red Hat
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
 * - src/backends/meta-settings-private.h
 * - src/meta/meta-settings.h
 */

#ifndef GF_SETTINGS_PRIVATE_H
#define GF_SETTINGS_PRIVATE_H

#include "gf-backend.h"
#include "gf-settings.h"

G_BEGIN_DECLS

#define GF_TYPE_SETTINGS (gf_settings_get_type ())
G_DECLARE_FINAL_TYPE (GfSettings, gf_settings, GF, SETTINGS, GObject)

GfSettings *gf_settings_new                       (GfBackend  *backend);

void        gf_settings_post_init                 (GfSettings *settings);

gboolean    gf_settings_get_global_scaling_factor (GfSettings *settings,
                                                   gint       *global_scaling_factor);

G_END_DECLS

#endif
