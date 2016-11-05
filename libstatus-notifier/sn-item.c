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

/**
 * SECTION: sn-item
 * @title: SnItem
 * @short_description: A Status Notifier Item
 *
 * A Status Notifier Item
 */

/**
 * SnItem:
 *
 * #SnItem is an opaque data structure and can only be accessed using the
 * following functions.
 */

#include "config.h"

#include <gio/gio.h>

#include "sn-dbus-item-server-v0.h"
#include "sn-dbus-item.h"
#include "sn-enum-types.h"
#include "sn-item.h"

typedef struct _SnItemPrivate SnItemPrivate;
struct _SnItemPrivate
{
  guint            version;

  SnItemCategory   category;
  gchar           *id;

  SnDBusItem      *impl;
};

enum
{
  PROP_0,

  PROP_VERSION,

  PROP_CATEGORY,
  PROP_ID,
  PROP_TITLE,
  PROP_STATUS,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

enum
{
  CONTEXT_MENU,
  ACTIVATE,
  SECONDARY_ACTIVATE,
  SCROLL,

  ERROR,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (SnItem, sn_item, G_TYPE_OBJECT)

static GdkPixbuf **
gvariant_to_gdk_pixbufs (GVariant *variant)
{
  GVariantIter iter;
  gsize n_pixbufs;
  GdkPixbuf **pixbufs;
  guint i;
  gint width;
  gint height;
  GVariant *value;

  n_pixbufs = g_variant_iter_init (&iter, variant);
  if (n_pixbufs == 0)
    return NULL;

  pixbufs = g_new0 (GdkPixbuf *, n_pixbufs + 1);
  i = 0;

  while (g_variant_iter_next (&iter, "(ii@ay)", &width, &height, &value))
    {
      GBytes *bytes;
      gint rowstride;

      bytes = g_variant_get_data_as_bytes (value);
      rowstride = g_bytes_get_size (bytes) / height;

      pixbufs[i++] = gdk_pixbuf_new_from_bytes (bytes, GDK_COLORSPACE_RGB,
                                                TRUE, 8, width, height,
                                                rowstride);

      g_bytes_unref (bytes);
      g_variant_unref (value);
    }

  pixbufs[i] = NULL;

  return pixbufs;
}

static GVariantBuilder *
gdk_pixbufs_to_gvariant_builder (GdkPixbuf **pixbufs)
{
  GVariantBuilder *builder;
  guint i;

  builder = g_variant_builder_new (G_VARIANT_TYPE ("a(iiay)"));

  if (pixbufs == NULL)
    return builder;

  for (i = 0; pixbufs[i] != NULL; i++)
    {
      gint width;
      gint height;
      GBytes *bytes;
      GVariant *variant;

      width = gdk_pixbuf_get_width (pixbufs[i]);
      height = gdk_pixbuf_get_height (pixbufs[i]);
      bytes = gdk_pixbuf_read_pixel_bytes (pixbufs[i]);

      variant = g_variant_new_from_bytes (G_VARIANT_TYPE ("ay"), bytes, TRUE);
      g_bytes_unref (bytes);

      g_variant_builder_open (builder, G_VARIANT_TYPE ("(iiay)"));
      g_variant_builder_add (builder, "i", width);
      g_variant_builder_add (builder, "i", height);
      g_variant_builder_add_value (builder, variant);
      g_variant_builder_close (builder);
    }

  return builder;
}

static void
context_menu_cb (SnDBusItemServer *server,
                 gint              x,
                 gint              y,
                 SnItem           *item)
{
  g_signal_emit (item, signals[CONTEXT_MENU], 0, x, y);
}

static void
activate_cb (SnDBusItemServer *server,
             gint              x,
             gint              y,
             SnItem           *item)
{
  g_signal_emit (item, signals[ACTIVATE], 0, x, y);
}

static void
secondary_activate_cb (SnDBusItemServer *server,
                       gint              x,
                       gint              y,
                       SnItem           *item)
{
  g_signal_emit (item, signals[SECONDARY_ACTIVATE], 0, x, y);
}

static void
scroll_cb (SnDBusItemServer  *server,
           gint               delta,
           SnItemOrientation  orientation,
           SnItem            *item)
{
  g_signal_emit (item, signals[SCROLL], 0, delta, orientation);
}

static void
hosts_changed_cb (SnDBusItemServer *server,
                  gboolean          is_host_registered,
                  SnItem           *item)
{
}

static void
error_cb (SnDBusItem   *dbus_item,
          const GError *error,
          SnItem       *item)
{
  g_signal_emit (item, signals[ERROR], 0, error);
}

static void
sn_item_constructed (GObject *object)
{
  SnItem *item;
  SnItemPrivate *priv;
  GType type;

  item = SN_ITEM (object);
  priv = sn_item_get_instance_private (item);

  G_OBJECT_CLASS (sn_item_parent_class)->constructed (object);

    {
#if 0
      if (priv->version == 1)
        type = SN_TYPE_DBUS_ITEM_SERVER_V1;
      else
        type = SN_TYPE_DBUS_ITEM_SERVER_V0;
#endif

      type = SN_TYPE_DBUS_ITEM_SERVER_V0;
      priv->impl = g_object_new (type,
                                 "well-known", FALSE,
                                 NULL);

      g_signal_connect (priv->impl, "context-menu",
                        G_CALLBACK (context_menu_cb), item);
      g_signal_connect (priv->impl, "activate",
                        G_CALLBACK (activate_cb), item);
      g_signal_connect (priv->impl, "secondary-activate",
                        G_CALLBACK (secondary_activate_cb), item);
      g_signal_connect (priv->impl, "scroll",
                        G_CALLBACK (scroll_cb), item);
      g_signal_connect (priv->impl, "hosts-changed",
                        G_CALLBACK (hosts_changed_cb), item);

      sn_dbus_item_set_category (priv->impl, priv->category);
      sn_dbus_item_set_id (priv->impl, priv->id);
    }

  g_signal_connect (priv->impl, "error", G_CALLBACK (error_cb), item);
}

static void
sn_item_dispose (GObject *object)
{
  SnItem *item;
  SnItemPrivate *priv;

  item = SN_ITEM (object);
  priv = sn_item_get_instance_private (item);

  g_clear_object (&priv->impl);

  G_OBJECT_CLASS (sn_item_parent_class)->dispose (object);
}

static void
sn_item_finalize (GObject *object)
{
  SnItem *item;
  SnItemPrivate *priv;

  item = SN_ITEM (object);
  priv = sn_item_get_instance_private (item);

  g_free (priv->id);

  G_OBJECT_CLASS (sn_item_parent_class)->finalize (object);
}

static void
sn_item_get_property (GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  SnItem *item;
  SnItemPrivate *priv;

  item = SN_ITEM (object);
  priv = sn_item_get_instance_private (item);

  switch (property_id)
    {
      case PROP_VERSION:
        g_value_set_uint (value, priv->version);
        break;

      case PROP_CATEGORY:
        g_value_set_enum (value, priv->category);
        break;

      case PROP_ID:
        g_value_set_string (value, priv->id);
        break;

      case PROP_TITLE:
        g_value_set_string (value, sn_item_get_title (item));
        break;

      case PROP_STATUS:
        g_value_set_enum (value, sn_item_get_status (item));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
sn_item_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  SnItem *item;
  SnItemPrivate *priv;

  item = SN_ITEM (object);
  priv = sn_item_get_instance_private (item);

  switch (property_id)
    {
      case PROP_VERSION:
        priv->version = g_value_get_uint (value);
        break;

      case PROP_CATEGORY:
        priv->category = g_value_get_enum (value);
        break;

      case PROP_ID:
        priv->id = g_value_dup_string (value);
        break;

      case PROP_TITLE:
        sn_item_set_title (item, g_value_get_string (value));
        break;

      case PROP_STATUS:
        sn_item_set_status (item, g_value_get_enum (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
sn_item_error (SnItem       *item,
               const GError *error)
{
  if (!g_signal_has_handler_pending (item, signals[ERROR], 0, TRUE))
    g_warning ("SnItem error: %s", error->message);
}

static void
sn_item_class_init (SnItemClass *item_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (item_class);

  object_class->constructed = sn_item_constructed;
  object_class->dispose = sn_item_dispose;
  object_class->finalize = sn_item_finalize;
  object_class->get_property = sn_item_get_property;
  object_class->set_property = sn_item_set_property;

  item_class->error = sn_item_error;

  properties[PROP_VERSION] =
    g_param_spec_uint ("version", "version", "version", 0, G_MAXUINT, 0,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  /**
   * SnItem:category:
   *
   * Describes the category of this item.
   */
  properties[PROP_CATEGORY] =
    g_param_spec_enum ("category",
                       "Category",
                       "Describes the category of this item.",
                       SN_TYPE_ITEM_CATEGORY,
                       SN_ITEM_CATEGORY_APPLICATION_STATUS,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  /**
   * SnItem:id:
   *
   * It's a name that should be unique for this application and consistent
   * between sessions, such as the application name itself.
   */
  properties[PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "Unique application identifier",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * SnItem:title:
   *
   * It's a name that describes the application, it can be more descriptive
   * than Id.
   */
  properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "It's a name that describes the application",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * SnItem:status:
   *
   * Describes the status of this item or of the associated application.
   */
  properties[PROP_STATUS] =
    g_param_spec_enum ("status",
                       "Status",
                       "Status of the item",
                       SN_TYPE_ITEM_STATUS,
                       SN_ITEM_STATUS_PASSIVE,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /**
   * SnItem::context-menu:
   * @item: the object on which the signal is emitted
   * @x: the x coordinate on screen
   * @y: the y coordinate on screen
   *
   * Asks the status notifier item to show a context menu, this is
   * typically a consequence of user input, such as mouse right click over
   * the graphical representation of the item.
   *
   * The x and y parameters are in screen coordinates and is to be
   * considered an hint to the item about where to show the context menu.
   */
  signals[CONTEXT_MENU] =
    g_signal_new ("context-menu", SN_TYPE_ITEM, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (SnItemClass, context_menu),
                  NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

  /**
   * SnItem::activate:
   * @item: the object on which the signal is emitted
   * @x: the x coordinate on screen
   * @y: the y coordinate on screen
   *
   * Asks the status notifier item for activation, this is typically a
   * consequence of user input, such as mouse left click over the graphical
   * representation of the item. The application will perform any task is
   * considered appropriate as an activation request.
   *
   * The x and y parameters are in screen coordinates and is to be
   * considered an hint to the item where to show eventual windows (if any).
   */
  signals[ACTIVATE] =
    g_signal_new ("activate", SN_TYPE_ITEM, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (SnItemClass, activate),
                  NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

  /**
   * SnItem::secondary-activate:
   * @item: the object on which the signal is emitted
   * @x: the x coordinate on screen
   * @y: the y coordinate on screen
   *
   * Is to be considered a secondary and less important form of activation
   * compared to Activate. This is typically a consequence of user input,
   * such as mouse middle click over the graphical representation of the
   * item. The application will perform any task is considered appropriate
   * as an activation request.
   *
   * The x and y parameters are in screen coordinates and is to be
   * considered an hint to the item where to show eventual windows (if any).
   */
  signals[SECONDARY_ACTIVATE] =
    g_signal_new ("secondary-activate", SN_TYPE_ITEM, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (SnItemClass, secondary_activate),
                  NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

  /**
   * SnItem::scroll:
   * @item: the object on which the signal is emitted
   * @delta: the amount of scroll
   * @orientation: orientation of the scroll request
   *
   * The user asked for a scroll action. This is caused from input such as
   * mouse wheel over the graphical representation of the item.
   *
   * The delta parameter represent the amount of scroll, the orientation
   * parameter represent the horizontal or vertical orientation of the
   * scroll request and its legal values are horizontal and vertical.
   */
  signals[SCROLL] =
    g_signal_new ("scroll", SN_TYPE_ITEM, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (SnItemClass, scroll), NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_INT, SN_TYPE_ITEM_ORIENTATION);

  /**
   * SnItem::error:
   * @item: the object on which the signal is emitted
   * @error: the #GError
   *
   * The ::error signal is emitted when error has occurred.
   */
  signals[ERROR] =
    g_signal_new ("error", SN_TYPE_ITEM, G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (SnItemClass, error), NULL,
                  NULL, NULL, G_TYPE_NONE, 1, G_TYPE_ERROR);
}

static void
sn_item_init (SnItem *item)
{
}

/**
 * sn_item_new_v0:
 * @category: the category of this item.
 * @id: unique application identifier
 *
 * Creates a new #SnItem.
 *
 * Returns: (transfer full): a newly created #SnItem.
 */
SnItem *
sn_item_new_v0 (SnItemCategory  category,
                const gchar    *id)
{
  return g_object_new (SN_TYPE_ITEM,
                       "version", 0,
                       "category", category,
                       "id", id,
                       NULL);
}

/**
 * sn_item_new_v1:
 * @category: the category of this item.
 * @id: unique application identifier
 *
 * Creates a new #SnItem.
 *
 * Returns: (transfer full): a newly created #SnItem.
 */
SnItem *
sn_item_new_v1 (SnItemCategory  category,
                const gchar    *id)
{
  return g_object_new (SN_TYPE_ITEM,
                       "version", 1,
                       "category", category,
                       "id", id,
                       NULL);
}

/**
 * sn_item_get_attention_icon_name:
 * @item: a #SnItem
 *
 * Returns the attention icon name.
 *
 * Returns: the attention icon name.
 */
const gchar *
sn_item_get_attention_icon_name (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_val_if_fail (SN_IS_ITEM (item), NULL);

  priv = sn_item_get_instance_private (item);

  return sn_dbus_item_get_attention_icon_name (priv->impl);
}

/**
 * sn_item_set_attention_icon_name:
 * @item: a #SnItem
 * @attention_icon_name: the attention icon pixmap
 *
 * Set the attention icon pixmap.
 */
void
sn_item_set_attention_icon_name (SnItem      *item,
                                 const gchar *attention_icon_name)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_set_attention_icon_name (priv->impl, attention_icon_name);
}

/**
 * sn_item_get_attention_icon_pixbufs:
 * @item: a #SnItem
 *
 * Returns the attention icon pixbufs.
 *
 * Returns: (transfer full): the attention icon pixbufs.
 */
GdkPixbuf **
sn_item_get_attention_icon_pixbufs (SnItem *item)
{
  SnItemPrivate *priv;
  GVariant *variant;

  g_return_val_if_fail (SN_IS_ITEM (item), NULL);

  priv = sn_item_get_instance_private (item);

  variant = sn_dbus_item_get_attention_icon_pixmap (priv->impl);

  return gvariant_to_gdk_pixbufs (variant);
}

/**
 * sn_item_set_attention_icon_pixbufs:
 * @item: a #SnItem
 * @attention_icon_pixbufs: the attention icon pixbufs
 *
 * Set the attention icon pixbufs.
 */
void
sn_item_set_attention_icon_pixbufs (SnItem     *item,
                                    GdkPixbuf **attention_icon_pixbufs)
{
  SnItemPrivate *priv;
  GVariantBuilder *builder;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  builder = gdk_pixbufs_to_gvariant_builder (attention_icon_pixbufs);

  sn_dbus_item_set_attention_icon_pixmap (priv->impl,
                                          g_variant_builder_end (builder));
  g_variant_builder_unref (builder);
}

/**
 * sn_item_get_attention_movie_name:
 * @item: a #SnItem
 *
 * Returns the attention movie name.
 *
 * Returns: the attention movie name.
 */
const gchar *
sn_item_get_attention_movie_name (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_val_if_fail (SN_IS_ITEM (item), NULL);

  priv = sn_item_get_instance_private (item);

  return sn_dbus_item_get_attention_movie_name (priv->impl);
}

/**
 * sn_item_set_attention_movie_name:
 * @item: a #SnItem
 * @attention_movie_name: the attention movie name
 *
 * Set the attention movie name.
 */
void
sn_item_set_attention_movie_name (SnItem      *item,
                                  const gchar *attention_movie_name)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_set_attention_movie_name (priv->impl, attention_movie_name);
}

/**
 * sn_item_get_category:
 * @item: a #SnItem
 *
 * Returns the category of @item.
 *
 * Returns: the #SnItemCategory of @item.
 */
SnItemCategory
sn_item_get_category (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_val_if_fail (SN_IS_ITEM (item), SN_ITEM_CATEGORY_APPLICATION_STATUS);

  priv = sn_item_get_instance_private (item);

  return sn_dbus_item_get_category (priv->impl);
}

/**
 * sn_item_get_id:
 * @item: a #SnItem
 *
 * Returns the id of @item.
 *
 * Returns: the id of @item.
 */
const gchar *
sn_item_get_id (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_val_if_fail (SN_IS_ITEM (item), NULL);

  priv = sn_item_get_instance_private (item);

  return sn_dbus_item_get_id (priv->impl);
}

/**
 * sn_item_get_icon_name:
 * @item: a #SnItem
 *
 * Returns the icon name.
 *
 * Returns: the icon name.
 */
const gchar *
sn_item_get_icon_name (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_val_if_fail (SN_IS_ITEM (item), NULL);

  priv = sn_item_get_instance_private (item);

  return sn_dbus_item_get_icon_name (priv->impl);
}

/**
 * sn_item_set_icon_name:
 * @item: a #SnItem
 * @icon_name: the icon name
 *
 * Set the icon name.
 */
void
sn_item_set_icon_name (SnItem      *item,
                       const gchar *icon_name)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_set_icon_name (priv->impl, icon_name);
}

/**
 * sn_item_get_icon_pixbufs:
 * @item: a #SnItem
 *
 * Returns the icon pixbufs.
 *
 * Returns: (transfer full): the icon pixbufs
 */
GdkPixbuf **
sn_item_get_icon_pixbufs (SnItem *item)
{
  SnItemPrivate *priv;
  GVariant *variant;

  g_return_val_if_fail (SN_IS_ITEM (item), NULL);

  priv = sn_item_get_instance_private (item);

  variant = sn_dbus_item_get_icon_pixmap (priv->impl);

  return gvariant_to_gdk_pixbufs (variant);
}

/**
 * sn_item_set_icon_pixbufs:
 * @item: a #SnItem
 * @icon_pixbufs: the icon pixbufs
 *
 * Set the icon pixbufs.
 */
void
sn_item_set_icon_pixbufs (SnItem     *item,
                          GdkPixbuf **icon_pixbufs)
{
  SnItemPrivate *priv;
  GVariantBuilder *builder;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  builder = gdk_pixbufs_to_gvariant_builder (icon_pixbufs);

  sn_dbus_item_set_icon_pixmap (priv->impl, g_variant_builder_end (builder));
  g_variant_builder_unref (builder);
}

/**
 * sn_item_get_icon_theme_path:
 * @item: a #SnItem
 *
 * Returns the icon theme path.
 *
 * Returns: the icon theme path.
 */
const gchar *
sn_item_get_icon_theme_path (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_val_if_fail (SN_IS_ITEM (item), NULL);

  priv = sn_item_get_instance_private (item);

  return sn_dbus_item_get_icon_theme_path (priv->impl);
}

/**
 * sn_item_set_icon_theme_path:
 * @item: a #SnItem
 * @icon_theme_path: the icon theme path
 *
 * Set the icon theme path.
 */
void
sn_item_set_icon_theme_path (SnItem      *item,
                             const gchar *icon_theme_path)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_set_icon_theme_path (priv->impl, icon_theme_path);
}

/**
 * sn_item_get_item_is_menu:
 * @item: a #SnItem
 *
 * Returns if item is menu.
 *
 * Returns: %TRUE if @item is menu, %FALSE otherwise.
 */
gboolean
sn_item_get_item_is_menu (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_val_if_fail (SN_IS_ITEM (item), FALSE);

  priv = sn_item_get_instance_private (item);

  return sn_dbus_item_get_item_is_menu (priv->impl);
}

/**
 * sn_item_set_item_is_menu:
 * @item: a #SnItem
 * @item_is_menu: %TRUE if @item is menu
 *
 * Set if this @item is menu. Default is %FALSE.
 */
void
sn_item_set_item_is_menu (SnItem   *item,
                          gboolean  item_is_menu)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_set_item_is_menu (priv->impl, item_is_menu);
}

/**
 * sn_item_get_menu:
 * @item: a #SnItem
 *
 * Returns the menu.
 *
 * Returns: (transfer none): the menu.
 */
GtkMenu *
sn_item_get_menu (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_val_if_fail (SN_IS_ITEM (item), NULL);

  priv = sn_item_get_instance_private (item);

  return sn_dbus_item_get_menu (priv->impl);
}

/**
 * sn_item_set_menu:
 * @item: a #SnItem
 * @menu: the menu
 *
 * Set the menu.
 */
void
sn_item_set_menu (SnItem  *item,
                  GtkMenu *menu)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_set_menu (priv->impl, menu);
}

/**
 * sn_item_get_overlay_icon_name:
 * @item: a #SnItem
 *
 * Returns the overlay icon name.
 *
 * Returns: the overlay icon name.
 */
const gchar *
sn_item_get_overlay_icon_name (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_val_if_fail (SN_IS_ITEM (item), NULL);

  priv = sn_item_get_instance_private (item);

  return sn_dbus_item_get_overlay_icon_name (priv->impl);
}

/**
 * sn_item_set_overlay_icon_name:
 * @item: a #SnItem
 * @overlay_icon_name: the overlay icon name
 *
 * Set the overlay icon name.
 */
void
sn_item_set_overlay_icon_name (SnItem      *item,
                               const gchar *overlay_icon_name)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_set_overlay_icon_name (priv->impl, overlay_icon_name);
}

/**
 * sn_item_get_overlay_icon_pixbufs:
 * @item: a #SnItem
 *
 * Returns the overlay icon pixbufs.
 *
 * Returns: (transfer full): the overlay icon pixbufs.
 */
GdkPixbuf **
sn_item_get_overlay_icon_pixbufs (SnItem *item)
{
  SnItemPrivate *priv;
  GVariant *variant;

  g_return_val_if_fail (SN_IS_ITEM (item), NULL);

  priv = sn_item_get_instance_private (item);

  variant = sn_dbus_item_get_overlay_icon_pixmap (priv->impl);

  return gvariant_to_gdk_pixbufs (variant);
}

/**
 * sn_item_set_overlay_icon_pixbufs:
 * @item: a #SnItem
 * @overlay_icon_pixbufs: the overlay icon pixbufs
 *
 * Set the overlay icon pixbufs.
 */
void
sn_item_set_overlay_icon_pixbufs (SnItem     *item,
                                  GdkPixbuf **overlay_icon_pixbufs)
{
  SnItemPrivate *priv;
  GVariantBuilder *builder;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  builder = gdk_pixbufs_to_gvariant_builder (overlay_icon_pixbufs);

  sn_dbus_item_set_overlay_icon_pixmap (priv->impl,
                                        g_variant_builder_end (builder));
  g_variant_builder_unref (builder);
}

/**
 * sn_item_get_status:
 * @item: a #SnItem
 *
 * Returns the status of @item.
 *
 * Returns: the #SnItemStatus of @item.
 */
SnItemStatus
sn_item_get_status (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_val_if_fail (SN_IS_ITEM (item), SN_ITEM_STATUS_PASSIVE);

  priv = sn_item_get_instance_private (item);

  return sn_dbus_item_get_status (priv->impl);
}

/**
 * sn_item_set_status:
 * @item: a #SnItem
 * @status: the #SnItemStatus
 *
 * Set the status of this @item.
 */
void
sn_item_set_status (SnItem       *item,
                    SnItemStatus  status)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_set_status (priv->impl, status);
}

/**
 * sn_item_get_title:
 * @item: a #SnItem
 *
 * Returns the title of @item.
 *
 * Returns: the title of @item.
 */
const gchar *
sn_item_get_title (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_val_if_fail (SN_IS_ITEM (item), NULL);

  priv = sn_item_get_instance_private (item);

  return sn_dbus_item_get_title (priv->impl);
}

/**
 * sn_item_set_title:
 * @item: a #SnItem
 * @title: the title.
 *
 * Set the title of this @item.
 */
void
sn_item_set_title (SnItem      *item,
                   const gchar *title)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_set_title (priv->impl, title);
}

/**
 * sn_item_get_tooltip:
 * @item: a #SnItem
 * @icon_name: (out) (allow-none) (transfer none): location to store the
 *     icon name, or %NULL
 * @icon_pixbufs: (out) (allow-none) (transfer full): location to store the
 *     icon pixbufs, or %NULL
 * @title: (out) (allow-none) (transfer none): location to store the title,
 *     or %NULL
 * @text: (out) (allow-none) (transfer none): location to store the text, or
 *     %NULL
 *
 * Get the tooltip.
 *
 * Returns: %TRUE if the tooltop exists, %FALSE otherwise.
 */
gboolean
sn_item_get_tooltip (SnItem        *item,
                     const gchar  **icon_name,
                     GdkPixbuf   ***icon_pixbufs,
                     const gchar  **title,
                     const gchar  **text)
{
  SnItemPrivate *priv;
  GVariant *tooltip;
  const gchar *tmp_icon_name;
  GVariant *variant;
  const gchar *tmp_title;
  const gchar *tmp_text;

  if (icon_name)
    *icon_name = NULL;

  if (icon_pixbufs)
    g_assert (*icon_pixbufs == NULL);

  if (title)
    *title = NULL;

  if (text)
    *text = NULL;

  g_return_val_if_fail (SN_IS_ITEM (item), FALSE);

  priv = sn_item_get_instance_private (item);
  tooltip = sn_dbus_item_get_tooltip (priv->impl);

  if (tooltip == NULL)
    return FALSE;

  g_variant_get (tooltip, "(&sa(iiay)&s&s)", &tmp_icon_name,
                 &variant, &tmp_title, &tmp_text);

  if (icon_name && *tmp_icon_name != '\0')
    *icon_name = tmp_icon_name;

  if (icon_pixbufs)
    *icon_pixbufs = gvariant_to_gdk_pixbufs (variant);

  if (title && *tmp_title != '\0')
    *title = tmp_title;

  if (text && *tmp_text != '\0')
    *text = tmp_text;

  return TRUE;
}

/**
 * sn_item_set_tooltip:
 * @item: a #SnItem
 * @icon_name: the icon name
 * @icon_pixbufs: the icon pixbufs
 * @title: the title
 * @text: the text
 *
 * Set the tooltip.
 */
void
sn_item_set_tooltip (SnItem       *item,
                     const gchar  *icon_name,
                     GdkPixbuf   **icon_pixbufs,
                     const gchar  *title,
                     const gchar  *text)
{
  SnItemPrivate *priv;
  GVariantBuilder *builder;
  GVariant *tooltip;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  builder = gdk_pixbufs_to_gvariant_builder (icon_pixbufs);
  tooltip = g_variant_new ("(sa(iiay)ss)", icon_name ? icon_name : "",
                           builder, title ? title : "", text ? text : "");

  sn_dbus_item_set_tooltip (priv->impl, tooltip);
}

/**
 * sn_item_get_window_id:
 * @item: a #SnItem
 *
 * Returns the window id.
 *
 * Returns: the window id.
 */
gint
sn_item_get_window_id (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_val_if_fail (SN_IS_ITEM (item), 0);

  priv = sn_item_get_instance_private (item);

  return sn_dbus_item_get_window_id (priv->impl);
}

/**
 * sn_item_set_window_id:
 * @item: a #SnItem
 * @window_id: the window id
 *
 * Set the window id for this @item.
 */
void
sn_item_set_window_id (SnItem *item,
                       gint    window_id)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_set_window_id (priv->impl, window_id);
}

/**
 * sn_item_context_menu:
 * @item: a #SnItem
 * @x: the x coordinate on screen
 * @y: the y coordinate on screen
 *
 * Asks the status notifier item to show a context menu, this is typically
 * a consequence of user input, such as mouse right click over the graphical
 * representation of the item.
 *
 * The x and y parameters are in screen coordinates and is to be considered
 * an hint to the item about where to show the context menu.
 */
void
sn_item_context_menu (SnItem *item,
                      gint    x,
                      gint    y)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_context_menu (priv->impl, x, y);
}

/**
 * sn_item_activate:
 * @item: a #SnItem
 * @x: the x coordinate on screen
 * @y: the y coordinate on screen
 *
 * Asks the status notifier item for activation, this is typically a
 * consequence of user input, such as mouse left click over the graphical
 * representation of the item. The application will perform any task is
 * considered appropriate as an activation request.
 *
 * The x and y parameters are in screen coordinates and is to be considered
 * an hint to the item where to show eventual windows (if any).
 */
void
sn_item_activate (SnItem *item,
                  gint    x,
                  gint    y)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_activate (priv->impl, x, y);
}

/**
 * sn_item_secondary_activate:
 * @item: a #SnItem
 * @x: the x coordinate on screen
 * @y: the y coordinate on screen
 *
 * Is to be considered a secondary and less important form of activation
 * compared to Activate. This is typically a consequence of user input, such
 * as mouse middle click over the graphical representation of the item. The
 * application will perform any task is considered appropriate as an
 * activation request.
 *
 * The x and y parameters are in screen coordinates and is to be considered
 * an hint to the item where to show eventual windows (if any).
 */
void
sn_item_secondary_activate (SnItem *item,
                            gint    x,
                            gint    y)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_secondary_activate (priv->impl, x, y);
}

/**
 * sn_item_scroll:
 * @item: a #SnItem
 * @delta: the amount of scroll
 * @orientation: orientation of the scroll request
 *
 * The user asked for a scroll action. This is caused from input such as
 * mouse wheel over the graphical representation of the item.
 *
 * The delta parameter represent the amount of scroll, the orientation
 * parameter represent the horizontal or vertical orientation of the scroll
 * request and its legal values are horizontal and vertical.
 */
void
sn_item_scroll (SnItem            *item,
                gint               delta,
                SnItemOrientation  orientation)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_scroll (priv->impl, delta, orientation);
}

/**
 * sn_item_register:
 * @item: a #SnItem
 *
 * Register @item with Status Notifier Watcher.
 */
void
sn_item_register (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_register (priv->impl);
}

/**
 * sn_item_unregister:
 * @item: a #SnItem
 *
 * Unregister @item from Status Notifier Watcher.
 */
void
sn_item_unregister (SnItem *item)
{
  SnItemPrivate *priv;

  g_return_if_fail (SN_IS_ITEM (item));

  priv = sn_item_get_instance_private (item);

  sn_dbus_item_unregister (priv->impl);
}
