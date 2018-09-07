/*
 * Copyright (C) 2018 Alberts Muktupāvels
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

#ifndef GF_A11Y_KEYBOARD_H
#define GF_A11Y_KEYBOARD_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GF_TYPE_A11Y_KEYBOARD (gf_a11y_keyboard_get_type ())
G_DECLARE_FINAL_TYPE (GfA11yKeyboard, gf_a11y_keyboard,
                      GF, A11Y_KEYBOARD, GObject)

GfA11yKeyboard *gf_a11y_keyboard_new (void);

G_END_DECLS

#endif
