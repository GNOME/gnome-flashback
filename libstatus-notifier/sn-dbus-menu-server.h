/*
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
 */

#ifndef SN_DBUS_MENU_SERVER_H
#define SN_DBUS_MENU_SERVER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SN_TYPE_DBUS_MENU_SERVER sn_dbus_menu_server_get_type ()
G_DECLARE_FINAL_TYPE (SnDBusMenuServer, sn_dbus_menu_server,
                      SN, DBUS_MENU_SERVER, GObject)

SnDBusMenuServer *sn_dbus_menu_server_new (GtkMenu     *menu,
                                           const gchar *object_path);

G_END_DECLS

#endif
