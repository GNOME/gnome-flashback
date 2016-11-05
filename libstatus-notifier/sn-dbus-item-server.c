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

#include "config.h"

#include "sn-dbus-item-server.h"
#include "sn-enum-types.h"

typedef struct
{
  gboolean well_known;
} SnDBusItemServerPrivate;

enum
{
  PROP_0,

  PROP_WELL_KNOWN,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

enum
{
  CONTEXT_MENU,
  ACTIVATE,
  SECONDARY_ACTIVATE,
  SCROLL,

  HOSTS_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (SnDBusItemServer, sn_dbus_item_server,
                                     SN_TYPE_DBUS_ITEM)

static void
sn_dbus_item_server_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  SnDBusItemServer *server;
  SnDBusItemServerPrivate *priv;

  server = SN_DBUS_ITEM_SERVER (object);
  priv = sn_dbus_item_server_get_instance_private (server);

  switch (property_id)
    {
      case PROP_WELL_KNOWN:
        priv->well_known = g_value_get_boolean (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
sn_dbus_item_server_context_menu (SnDBusItemServer *server,
                                  gint              x,
                                  gint              y)
{
  if (!g_signal_has_handler_pending (server, signals[CONTEXT_MENU], 0, TRUE))
    g_assert_not_reached ();
}

static void
sn_dbus_item_server_activate (SnDBusItemServer *server,
                              gint              x,
                              gint              y)
{
  if (!g_signal_has_handler_pending (server, signals[ACTIVATE], 0, TRUE))
    g_assert_not_reached ();
}

static void
sn_dbus_item_server_secondary_activate (SnDBusItemServer *server,
                                        gint              x,
                                        gint              y)
{
  if (!g_signal_has_handler_pending (server, signals[SECONDARY_ACTIVATE],
                                     0, TRUE))
    {
      g_assert_not_reached ();
    }
}

static void
sn_dbus_item_server_scroll (SnDBusItemServer  *server,
                            gint               delta,
                            SnItemOrientation  orientation)
{
  if (!g_signal_has_handler_pending (server, signals[SCROLL], 0, TRUE))
    g_assert_not_reached ();
}

static void
sn_dbus_item_server_hosts_changed (SnDBusItemServer *server,
                                   gboolean          registered)
{
  if (!g_signal_has_handler_pending (server, signals[HOSTS_CHANGED], 0, TRUE))
    g_assert_not_reached ();
}

static void
sn_dbus_item_server_class_init (SnDBusItemServerClass *server_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (server_class);

  object_class->set_property = sn_dbus_item_server_set_property;

  server_class->context_menu = sn_dbus_item_server_context_menu;
  server_class->activate = sn_dbus_item_server_activate;
  server_class->secondary_activate = sn_dbus_item_server_secondary_activate;
  server_class->scroll = sn_dbus_item_server_scroll;
  server_class->hosts_changed = sn_dbus_item_server_hosts_changed;

  properties[PROP_WELL_KNOWN] =
    g_param_spec_boolean ("well-known", "well-known", "well-known", FALSE,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                          G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals[CONTEXT_MENU] =
    g_signal_new ("context-menu", SN_TYPE_DBUS_ITEM_SERVER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE,
                  2, G_TYPE_INT, G_TYPE_INT);

  signals[ACTIVATE] =
    g_signal_new ("activate", SN_TYPE_DBUS_ITEM_SERVER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE,
                  2, G_TYPE_INT, G_TYPE_INT);

  signals[SECONDARY_ACTIVATE] =
    g_signal_new ("secondary-activate", SN_TYPE_DBUS_ITEM_SERVER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE,
                  2, G_TYPE_INT, G_TYPE_INT);

  signals[SCROLL] =
    g_signal_new ("scroll", SN_TYPE_DBUS_ITEM_SERVER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE,
                  2, G_TYPE_INT, SN_TYPE_ITEM_ORIENTATION);

  signals[HOSTS_CHANGED] =
    g_signal_new ("hosts-changed", SN_TYPE_DBUS_ITEM_SERVER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE,
                  1, G_TYPE_BOOLEAN);
}

static void
sn_dbus_item_server_init (SnDBusItemServer *server)
{
}

gboolean
sn_dbus_item_server_get_well_known (SnDBusItemServer *server)
{
  SnDBusItemServerPrivate *priv;

  priv = sn_dbus_item_server_get_instance_private (server);

  return priv->well_known;
}

void
sn_dbus_item_server_emit_context_menu (SnDBusItemServer *server,
                                       gint              x,
                                       gint              y)
{
  g_signal_emit (server, signals[CONTEXT_MENU], 0, x, y);
}

void
sn_dbus_item_server_emit_activate (SnDBusItemServer *server,
                                   gint              x,
                                   gint              y)
{
  g_signal_emit (server, signals[ACTIVATE], 0, x, y);
}

void
sn_dbus_item_server_emit_secondary_activate (SnDBusItemServer *server,
                                             gint              x,
                                             gint              y)
{
  g_signal_emit (server, signals[SECONDARY_ACTIVATE], 0, x, y);
}

void
sn_dbus_item_server_emit_scroll (SnDBusItemServer  *server,
                                 gint               delta,
                                 SnItemOrientation  orientation)
{
  g_signal_emit (server, signals[SCROLL], 0, delta, orientation);
}

void
sn_dbus_item_server_emit_hosts_changed (SnDBusItemServer *server,
                                        gboolean          registered)
{
  g_signal_emit (server, signals[HOSTS_CHANGED], 0, registered);
}
