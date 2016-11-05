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

#ifndef SN_DBUS_ITEM_SERVER_V0_H
#define SN_DBUS_ITEM_SERVER_V0_H

#include "sn-dbus-item-server.h"

G_BEGIN_DECLS

#define SN_TYPE_DBUS_ITEM_SERVER_V0 sn_dbus_item_server_v0_get_type ()
G_DECLARE_FINAL_TYPE (SnDBusItemServerV0, sn_dbus_item_server_v0,
                      SN, DBUS_ITEM_SERVER_V0, SnDBusItemServer)

G_END_DECLS

#endif
