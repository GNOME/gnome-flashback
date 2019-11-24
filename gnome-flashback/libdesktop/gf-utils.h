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

#ifndef GF_UTILS_H
#define GF_UTILS_H

#include <gio/gdesktopappinfo.h>
#include <glib.h>

G_BEGIN_DECLS

gboolean gf_launch_app_info     (GDesktopAppInfo  *app_info,
                                 GError          **error);

gboolean gf_launch_desktop_file (const char       *desktop_file,
                                 GError          **error);

gboolean gf_launch_uri          (const char       *uri,
                                 GError          **error);

double   gf_get_nautilus_scale  (void);

G_END_DECLS

#endif
