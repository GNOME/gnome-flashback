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

#ifndef SN_DBUS_ITEM_H
#define SN_DBUS_ITEM_H

#include "sn-item.h"

G_BEGIN_DECLS

#define SN_TYPE_DBUS_ITEM sn_dbus_item_get_type ()
G_DECLARE_DERIVABLE_TYPE (SnDBusItem, sn_dbus_item, SN, DBUS_ITEM, GObject)

struct _SnDBusItemClass
{
  GObjectClass parent_class;

  const gchar *  (* get_attention_icon_name)   (SnDBusItem        *impl);
  void           (* set_attention_icon_name)   (SnDBusItem        *impl,
                                                const gchar       *attention_icon_name);

  GVariant *     (* get_attention_icon_pixmap) (SnDBusItem        *impl);
  void           (* set_attention_icon_pixmap) (SnDBusItem        *impl,
                                                GVariant          *attention_icon_pixmap);

  const gchar *  (* get_attention_movie_name)  (SnDBusItem        *impl);
  void           (* set_attention_movie_name)  (SnDBusItem        *impl,
                                                const gchar       *attention_movie_name);

  SnItemCategory (* get_category)              (SnDBusItem        *impl);
  void           (* set_category)              (SnDBusItem        *impl,
                                                SnItemCategory     category);

  const gchar *  (* get_id)                    (SnDBusItem        *impl);
  void           (* set_id)                    (SnDBusItem        *impl,
                                                const gchar       *id);

  const gchar *  (* get_icon_name)             (SnDBusItem        *impl);
  void           (* set_icon_name)             (SnDBusItem        *impl,
                                                const gchar       *icon_name);

  GVariant *     (* get_icon_pixmap)           (SnDBusItem        *impl);
  void           (* set_icon_pixmap)           (SnDBusItem        *impl,
                                                GVariant          *icon_pixmap);

  const gchar *  (* get_icon_theme_path)       (SnDBusItem        *impl);
  void           (* set_icon_theme_path)       (SnDBusItem        *impl,
                                                const gchar       *icon_theme_path);

  gboolean       (* get_item_is_menu)          (SnDBusItem        *impl);
  void           (* set_item_is_menu)          (SnDBusItem        *impl,
                                                gboolean           item_is_menu);

  GtkMenu *      (* get_menu)                  (SnDBusItem        *impl);
  void           (* set_menu)                  (SnDBusItem        *impl,
                                                GtkMenu           *menu);

  const gchar *  (* get_overlay_icon_name)     (SnDBusItem        *impl);
  void           (* set_overlay_icon_name)     (SnDBusItem        *impl,
                                                const gchar       *overlay_icon_name);

  GVariant *     (* get_overlay_icon_pixmap)   (SnDBusItem        *impl);
  void           (* set_overlay_icon_pixmap)   (SnDBusItem        *impl,
                                                GVariant          *overlay_icon_pixmap);

  SnItemStatus   (* get_status)                (SnDBusItem        *impl);
  void           (* set_status)                (SnDBusItem        *impl,
                                                SnItemStatus       status);

  const gchar *  (* get_title)                 (SnDBusItem        *impl);
  void           (* set_title)                 (SnDBusItem        *impl,
                                                const gchar       *title);

  GVariant *     (* get_tooltip)               (SnDBusItem        *impl);
  void           (* set_tooltip)               (SnDBusItem        *impl,
                                                GVariant          *tooltip);

  gint           (* get_window_id)             (SnDBusItem        *impl);
  void           (* set_window_id)             (SnDBusItem        *impl,
                                                gint               window_id);

  void           (* context_menu)              (SnDBusItem        *impl,
                                                gint               x,
                                                gint               y);
  void           (* activate)                  (SnDBusItem        *impl,
                                                gint               x,
                                                gint               y);
  void           (* secondary_activate)        (SnDBusItem        *impl,
                                                gint               x,
                                                gint               y);
  void           (* scroll)                    (SnDBusItem        *impl,
                                                gint               delta,
                                                SnItemOrientation  orientation);

  void           (* register_)                 (SnDBusItem        *impl);
  void           (* unregister)                (SnDBusItem        *impl);

  void           (* error)                     (SnDBusItem        *impl,
                                                const GError      *error);
};

const gchar    *sn_dbus_item_get_attention_icon_name   (SnDBusItem        *impl);
void            sn_dbus_item_set_attention_icon_name   (SnDBusItem        *impl,
                                                        const gchar       *attention_icon_name);

GVariant       *sn_dbus_item_get_attention_icon_pixmap (SnDBusItem        *impl);
void            sn_dbus_item_set_attention_icon_pixmap (SnDBusItem        *impl,
                                                        GVariant          *attention_icon_pixmap);

const gchar    *sn_dbus_item_get_attention_movie_name  (SnDBusItem        *impl);
void            sn_dbus_item_set_attention_movie_name  (SnDBusItem        *impl,
                                                        const gchar       *attention_movie_name);

SnItemCategory  sn_dbus_item_get_category              (SnDBusItem        *impl);
void            sn_dbus_item_set_category              (SnDBusItem        *impl,
                                                        SnItemCategory     category);

const gchar    *sn_dbus_item_get_id                    (SnDBusItem        *impl);
void            sn_dbus_item_set_id                    (SnDBusItem        *impl,
                                                        const gchar       *id);

const gchar    *sn_dbus_item_get_icon_name             (SnDBusItem        *impl);
void            sn_dbus_item_set_icon_name             (SnDBusItem        *impl,
                                                        const gchar       *icon_name);

GVariant       *sn_dbus_item_get_icon_pixmap           (SnDBusItem        *impl);
void            sn_dbus_item_set_icon_pixmap           (SnDBusItem        *impl,
                                                        GVariant          *icon_pixmap);

const gchar    *sn_dbus_item_get_icon_theme_path       (SnDBusItem        *impl);
void            sn_dbus_item_set_icon_theme_path       (SnDBusItem        *impl,
                                                        const gchar       *icon_theme_path);

gboolean        sn_dbus_item_get_item_is_menu          (SnDBusItem        *impl);
void            sn_dbus_item_set_item_is_menu          (SnDBusItem        *impl,
                                                        gboolean           item_is_menu);

GtkMenu        *sn_dbus_item_get_menu                  (SnDBusItem        *impl);
void            sn_dbus_item_set_menu                  (SnDBusItem        *impl,
                                                        GtkMenu           *menu);

const gchar    *sn_dbus_item_get_overlay_icon_name     (SnDBusItem        *impl);
void            sn_dbus_item_set_overlay_icon_name     (SnDBusItem        *impl,
                                                        const gchar       *overlay_icon_name);

GVariant       *sn_dbus_item_get_overlay_icon_pixmap   (SnDBusItem        *impl);
void            sn_dbus_item_set_overlay_icon_pixmap   (SnDBusItem        *impl,
                                                        GVariant          *overlay_icon_pixmap);

SnItemStatus    sn_dbus_item_get_status                (SnDBusItem        *impl);
void            sn_dbus_item_set_status                (SnDBusItem        *impl,
                                                        SnItemStatus       status);

const gchar    *sn_dbus_item_get_title                 (SnDBusItem        *impl);
void            sn_dbus_item_set_title                 (SnDBusItem        *impl,
                                                        const gchar       *title);

GVariant       *sn_dbus_item_get_tooltip               (SnDBusItem        *impl);
void            sn_dbus_item_set_tooltip               (SnDBusItem        *impl,
                                                        GVariant          *tooltip);

gint            sn_dbus_item_get_window_id             (SnDBusItem        *impl);
void            sn_dbus_item_set_window_id             (SnDBusItem        *impl,
                                                        gint               window_id);

void            sn_dbus_item_context_menu              (SnDBusItem        *impl,
                                                        gint               x,
                                                        gint               y);
void            sn_dbus_item_activate                  (SnDBusItem        *impl,
                                                        gint               x,
                                                        gint               y);
void            sn_dbus_item_secondary_activate        (SnDBusItem        *impl,
                                                        gint               x,
                                                        gint               y);
void            sn_dbus_item_scroll                    (SnDBusItem        *impl,
                                                        gint               delta,
                                                        SnItemOrientation  orientation);

void            sn_dbus_item_register                  (SnDBusItem        *impl);
void            sn_dbus_item_unregister                (SnDBusItem        *impl);

const gchar    *sn_dbus_item_get_bus_name              (SnDBusItem        *impl);
const gchar    *sn_dbus_item_get_object_path           (SnDBusItem        *impl);

void            sn_dbus_item_emit_error                (SnDBusItem        *impl,
                                                        const GError      *error);

G_END_DECLS

#endif
