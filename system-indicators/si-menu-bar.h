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

#ifndef SI_MENU_BAR_H
#define SI_MENU_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SI_TYPE_MENU_BAR (si_menu_bar_get_type ())
G_DECLARE_FINAL_TYPE (SiMenuBar, si_menu_bar, SI, MENU_BAR, GtkMenuBar)

GtkWidget *si_menu_bar_new (void);

G_END_DECLS

#endif
