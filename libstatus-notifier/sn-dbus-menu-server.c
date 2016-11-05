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

#include "sn-dbus-menu-server.h"
#include "sn-dbus-menu-gen.h"

struct _SnDBusMenuServer
{
  GObject  parent;

  GtkMenu *menu;
  gchar   *object_path;
};

enum
{
  PROP_0,

  PROP_MENU,
  PROP_OBJECT_PATH,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (SnDBusMenuServer, sn_dbus_menu_server, G_TYPE_OBJECT)

static void
sn_dbus_menu_server_constructed (GObject *object)
{
  G_OBJECT_CLASS (sn_dbus_menu_server_parent_class)->constructed (object);
}

static void
sn_dbus_menu_server_dispose (GObject *object)
{
  SnDBusMenuServer *server;

  server = SN_DBUS_MENU_SERVER (object);

  if (server->menu != NULL)
    {
      gtk_widget_destroy (GTK_WIDGET (server->menu));
      server->menu = NULL;
    }

  G_OBJECT_CLASS (sn_dbus_menu_server_parent_class)->dispose (object);
}

static void
sn_dbus_menu_server_finalize (GObject *object)
{
  SnDBusMenuServer *server;

  server = SN_DBUS_MENU_SERVER (object);

  g_free (server->object_path);

  G_OBJECT_CLASS (sn_dbus_menu_server_parent_class)->finalize (object);
}

static void
sn_dbus_menu_server_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  SnDBusMenuServer *server;

  server = SN_DBUS_MENU_SERVER (object);

  switch (property_id)
    {
      case PROP_MENU:
        server->menu = g_value_dup_object (value);
        break;

      case PROP_OBJECT_PATH:
        server->object_path = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
sn_dbus_menu_server_class_init (SnDBusMenuServerClass *server_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (server_class);

  object_class->constructed = sn_dbus_menu_server_constructed;
  object_class->dispose = sn_dbus_menu_server_dispose;
  object_class->finalize = sn_dbus_menu_server_finalize;
  object_class->set_property = sn_dbus_menu_server_set_property;

  properties[PROP_MENU] =
    g_param_spec_object ("menu", "menu", "menu", GTK_TYPE_MENU,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_OBJECT_PATH] =
    g_param_spec_string ("object-path", "object-path", "object-path", NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
sn_dbus_menu_server_init (SnDBusMenuServer *server)
{
}

SnDBusMenuServer *
sn_dbus_menu_server_new (GtkMenu     *menu,
                         const gchar *object_path)
{
  return g_object_new (SN_TYPE_DBUS_MENU_SERVER,
                       "menu", menu,
                       "object-path", object_path,
                       NULL);
}
