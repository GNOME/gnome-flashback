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

#ifndef SI_DESKTOP_MENU_ITEM_H
#define SI_DESKTOP_MENU_ITEM_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SI_TYPE_DESKTOP_MENU_ITEM (si_desktop_menu_item_get_type ())
G_DECLARE_FINAL_TYPE (SiDesktopMenuItem, si_desktop_menu_item,
                      SI, DESKTOP_MENU_ITEM, GtkMenuItem)

GtkWidget *si_desktop_menu_item_new (const char *label,
                                     const char *desktop_id);

G_END_DECLS

#endif
