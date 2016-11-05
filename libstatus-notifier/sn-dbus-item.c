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

#include "sn-dbus-item-server-v0.h"
#include "sn-dbus-item.h"

typedef struct _SnDBusItemPrivate
{
  gchar *bus_name;
  gchar *object_path;
} SnDBusItemPrivate;

enum
{
  PROP_0,

  PROP_BUS_NAME,
  PROP_OBJECT_PATH,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

enum
{
  ERROR,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (SnDBusItem, sn_dbus_item, G_TYPE_OBJECT)

static void
sn_dbus_item_finalize (GObject *object)
{
  SnDBusItem *impl;
  SnDBusItemPrivate *priv;

  impl = SN_DBUS_ITEM (object);
  priv = sn_dbus_item_get_instance_private (impl);

  g_free (priv->bus_name);
  g_free (priv->object_path);

  G_OBJECT_CLASS (sn_dbus_item_parent_class)->finalize (object);
}

static void
sn_dbus_item_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  SnDBusItem *impl;
  SnDBusItemPrivate *priv;

  impl = SN_DBUS_ITEM (object);
  priv = sn_dbus_item_get_instance_private (impl);

  switch (property_id)
    {
      case PROP_BUS_NAME:
        g_value_set_string (value, priv->bus_name);
        break;

      case PROP_OBJECT_PATH:
        g_value_set_string (value, priv->object_path);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
sn_dbus_item_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  SnDBusItem *impl;
  SnDBusItemPrivate *priv;

  impl = SN_DBUS_ITEM (object);
  priv = sn_dbus_item_get_instance_private (impl);

  switch (property_id)
    {
      case PROP_BUS_NAME:
        g_assert (priv->bus_name == NULL);
        priv->bus_name = g_value_dup_string (value);
        break;

      case PROP_OBJECT_PATH:
        g_assert (priv->object_path == NULL);
        priv->object_path = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
sn_dbus_item_error (SnDBusItem   *item,
                    const GError *error)
{
  if (!g_signal_has_handler_pending (item, signals[ERROR], 0, TRUE))
    g_assert_not_reached ();
}

static void
sn_dbus_item_class_init (SnDBusItemClass *impl_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (impl_class);

  object_class->finalize = sn_dbus_item_finalize;
  object_class->get_property = sn_dbus_item_get_property;
  object_class->set_property = sn_dbus_item_set_property;

  impl_class->error = sn_dbus_item_error;

  properties[PROP_BUS_NAME] =
    g_param_spec_string ("bus-name", "bus-name", "bus-name", NULL,
                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_OBJECT_PATH] =
    g_param_spec_string ("object-path", "object-path", "object-path", NULL,
                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals[ERROR] =
    g_signal_new ("error", SN_TYPE_DBUS_ITEM, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (SnDBusItemClass, error),
                  NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_ERROR);
}

static void
sn_dbus_item_init (SnDBusItem *impl)
{
}

const gchar *
sn_dbus_item_get_attention_icon_name (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_attention_icon_name (impl);
}

void
sn_dbus_item_set_attention_icon_name (SnDBusItem  *impl,
                                      const gchar *attention_icon_name)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_attention_icon_name (impl,
                                                          attention_icon_name);
}

GVariant *
sn_dbus_item_get_attention_icon_pixmap (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_attention_icon_pixmap (impl);
}

void
sn_dbus_item_set_attention_icon_pixmap (SnDBusItem *impl,
                                        GVariant   *attention_icon_pixmap)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_attention_icon_pixmap (impl,
                                                            attention_icon_pixmap);
}

const gchar *
sn_dbus_item_get_attention_movie_name (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_attention_movie_name (impl);
}

void
sn_dbus_item_set_attention_movie_name (SnDBusItem  *impl,
                                       const gchar *attention_movie_name)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_attention_movie_name (impl,
                                                           attention_movie_name);
}

SnItemCategory
sn_dbus_item_get_category (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_category (impl);
}

void
sn_dbus_item_set_category (SnDBusItem     *impl,
                           SnItemCategory  category)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_category (impl, category);
}

const gchar *
sn_dbus_item_get_id (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_id (impl);
}

void
sn_dbus_item_set_id (SnDBusItem  *impl,
                     const gchar *id)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_id (impl, id);
}

const gchar *
sn_dbus_item_get_icon_name (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_icon_name (impl);
}

void
sn_dbus_item_set_icon_name (SnDBusItem  *impl,
                            const gchar *icon_name)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_icon_name (impl, icon_name);
}

GVariant *
sn_dbus_item_get_icon_pixmap (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_icon_pixmap (impl);
}

void
sn_dbus_item_set_icon_pixmap (SnDBusItem *impl,
                              GVariant   *icon_pixmap)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_icon_pixmap (impl, icon_pixmap);
}

const gchar *
sn_dbus_item_get_icon_theme_path (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_icon_theme_path (impl);
}

void
sn_dbus_item_set_icon_theme_path (SnDBusItem  *impl,
                                  const gchar *icon_theme_path)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_icon_theme_path (impl, icon_theme_path);
}

gboolean
sn_dbus_item_get_item_is_menu (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_item_is_menu (impl);
}

void
sn_dbus_item_set_item_is_menu (SnDBusItem *impl,
                               gboolean    item_is_menu)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_item_is_menu (impl, item_is_menu);
}

GtkMenu *
sn_dbus_item_get_menu (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_menu (impl);
}

void
sn_dbus_item_set_menu (SnDBusItem *impl,
                       GtkMenu    *menu)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_menu (impl, menu);
}

const gchar *
sn_dbus_item_get_overlay_icon_name (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_overlay_icon_name (impl);
}

void
sn_dbus_item_set_overlay_icon_name (SnDBusItem  *impl,
                                    const gchar *overlay_icon_name)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_overlay_icon_name (impl,
                                                        overlay_icon_name);
}

GVariant *
sn_dbus_item_get_overlay_icon_pixmap (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_overlay_icon_pixmap (impl);
}

void
sn_dbus_item_set_overlay_icon_pixmap (SnDBusItem *impl,
                                      GVariant   *overlay_icon_pixmap)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_overlay_icon_pixmap (impl,
                                                          overlay_icon_pixmap);
}

SnItemStatus
sn_dbus_item_get_status (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_status (impl);
}

void
sn_dbus_item_set_status (SnDBusItem   *impl,
                         SnItemStatus  status)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_status (impl, status);
}

const gchar *
sn_dbus_item_get_title (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_title (impl);
}

void
sn_dbus_item_set_title (SnDBusItem  *impl,
                        const gchar *title)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_title (impl, title);
}

GVariant *
sn_dbus_item_get_tooltip (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_tooltip (impl);
}

void
sn_dbus_item_set_tooltip (SnDBusItem *impl,
                          GVariant   *tooltip)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_tooltip (impl, tooltip);
}

gint
sn_dbus_item_get_window_id (SnDBusItem *impl)
{
  return SN_DBUS_ITEM_GET_CLASS (impl)->get_window_id (impl);
}

void
sn_dbus_item_set_window_id (SnDBusItem *impl,
                            gint        window_id)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->set_window_id (impl, window_id);
}

void
sn_dbus_item_context_menu (SnDBusItem *impl,
                           gint        x,
                           gint        y)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->context_menu (impl, x, y);
}

void
sn_dbus_item_activate (SnDBusItem *impl,
                       gint        x,
                       gint        y)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->activate (impl, x, y);
}

void
sn_dbus_item_secondary_activate (SnDBusItem *impl,
                                 gint        x,
                                 gint        y)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->secondary_activate (impl, x, y);
}

void
sn_dbus_item_scroll (SnDBusItem        *impl,
                     gint               delta,
                     SnItemOrientation  orientation)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->scroll (impl, delta, orientation);
}

void
sn_dbus_item_register (SnDBusItem *impl)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->register_ (impl);
}

void
sn_dbus_item_unregister (SnDBusItem *impl)
{
  SN_DBUS_ITEM_GET_CLASS (impl)->unregister (impl);
}

const gchar *
sn_dbus_item_get_bus_name (SnDBusItem *impl)
{
  SnDBusItemPrivate *priv;

  priv = sn_dbus_item_get_instance_private (impl);

  return priv->bus_name;
}

const gchar *
sn_dbus_item_get_object_path (SnDBusItem *impl)
{
  SnDBusItemPrivate *priv;

  priv = sn_dbus_item_get_instance_private (impl);

  return priv->object_path;
}

void
sn_dbus_item_emit_error (SnDBusItem   *impl,
                         const GError *error)
{
  g_signal_emit (impl, signals[ERROR], 0, error);
}
