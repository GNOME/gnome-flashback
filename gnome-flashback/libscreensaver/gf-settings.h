/*
 * Copyright (C) 2004-2006 William Jon McCann
 * Copyright (C) 2016 Alberts Muktupāvels
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
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     William Jon McCann <mccann@jhu.edu>
 */

#ifndef GF_SETTINGS_H
#define GF_SETTINGS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GF_TYPE_SETTINGS gf_settings_get_type ()
G_DECLARE_FINAL_TYPE (GfSettings, gf_settings, GF, SETTINGS, GObject)

GfSettings  *gf_settings_new                           (void);

gboolean     gf_settings_get_lock_disabled             (GfSettings *settings);

gboolean     gf_settings_get_user_switch_disabled      (GfSettings *settings);

gboolean     gf_settings_get_embedded_keyboard_enabled (GfSettings *settings);

const gchar *gf_settings_get_embedded_keyboard_command (GfSettings *settings);

gboolean     gf_settings_get_idle_activation_enabled   (GfSettings *settings);

gboolean     gf_settings_get_lock_enabled              (GfSettings *settings);

guint        gf_settings_get_lock_delay                (GfSettings *settings);

gboolean     gf_settings_get_logout_enabled            (GfSettings *settings);

guint        gf_settings_get_logout_delay              (GfSettings *settings);

const gchar *gf_settings_get_logout_command            (GfSettings *settings);

gboolean     gf_settings_get_status_message_enabled    (GfSettings *settings);

gboolean     gf_settings_get_user_switch_enabled       (GfSettings *settings);

G_END_DECLS

#endif
