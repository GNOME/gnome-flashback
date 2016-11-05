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

#ifndef SN_DBUS_ITEM_SERVER_H
#define SN_DBUS_ITEM_SERVER_H

#include "sn-dbus-item.h"

G_BEGIN_DECLS

#define SN_TYPE_DBUS_ITEM_SERVER sn_dbus_item_server_get_type ()
G_DECLARE_DERIVABLE_TYPE (SnDBusItemServer, sn_dbus_item_server,
                          SN, DBUS_ITEM_SERVER, SnDBusItem)

struct _SnDBusItemServerClass
{
  SnDBusItemClass parent_class;

  void (* context_menu)       (SnDBusItemServer  *server,
                               gint               x,
                               gint               y);
  void (* activate)           (SnDBusItemServer  *server,
                               gint               x,
                               gint               y);
  void (* secondary_activate) (SnDBusItemServer  *server,
                               gint               x,
                               gint               y);
  void (* scroll)             (SnDBusItemServer  *server,
                               gint               delta,
                               SnItemOrientation  orientation);

  void (* hosts_changed)      (SnDBusItemServer  *server,
                               gboolean           registered);
};

gboolean sn_dbus_item_server_get_well_known          (SnDBusItemServer  *server);

void     sn_dbus_item_server_emit_context_menu       (SnDBusItemServer  *server,
                                                      gint               x,
                                                      gint               y);

void     sn_dbus_item_server_emit_activate           (SnDBusItemServer  *server,
                                                      gint               x,
                                                      gint               y);

void     sn_dbus_item_server_emit_secondary_activate (SnDBusItemServer  *server,
                                                      gint               x,
                                                      gint               y);

void     sn_dbus_item_server_emit_scroll             (SnDBusItemServer  *server,
                                                      gint               delta,
                                                      SnItemOrientation  orientation);

void     sn_dbus_item_server_emit_hosts_changed      (SnDBusItemServer  *server,
                                                      gboolean           registered);

G_END_DECLS

#endif
