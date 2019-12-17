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

#ifndef SI_APPLET_H
#define SI_APPLET_H

#include <libgnome-panel/gp-applet.h>

G_BEGIN_DECLS

#define SI_TYPE_APPLET (si_applet_get_type ())
G_DECLARE_FINAL_TYPE (SiApplet, si_applet, SI, APPLET, GpApplet)

G_END_DECLS

#endif
