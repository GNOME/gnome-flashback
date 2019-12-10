/*
 * Copyright (C) 2004-2006 William Jon McCann
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     William Jon McCann <mccann@jhu.edu>
 */

#ifndef GF_PREFS_H
#define GF_PREFS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GF_TYPE_PREFS (gf_prefs_get_type ())
G_DECLARE_FINAL_TYPE (GfPrefs, gf_prefs, GF, PREFS, GObject)

GfPrefs  *gf_prefs_new                      (void);

gboolean  gf_prefs_get_lock_disabled        (GfPrefs *self);

gboolean  gf_prefs_get_user_switch_disabled (GfPrefs *self);

gboolean  gf_prefs_get_lock_enabled         (GfPrefs *self);

guint     gf_prefs_get_lock_delay           (GfPrefs *self);

gboolean  gf_prefs_get_user_switch_enabled  (GfPrefs *self);

G_END_DECLS

#endif
