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

#ifndef SN_ITEM_H
#define SN_ITEM_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * SN_TYPE_ITEM:
 *
 * The type for SnItem.
 */
#define SN_TYPE_ITEM sn_item_get_type ()
G_DECLARE_DERIVABLE_TYPE (SnItem, sn_item, SN, ITEM, GObject)

/**
 * SnItemCategory:
 * @SN_ITEM_CATEGORY_APPLICATION_STATUS: The item describes the status
 *     of a generic application, for instance the current state of a media
 *     player. In the case where the category of the item can not be known,
 *     such as when the item is being proxied from another incompatible or
 *     emulated system, ApplicationStatus can be used a sensible default
 *     fallback.
 * @SN_ITEM_CATEGORY_COMMUNICATIONS: The item describes the status of
 *     communication oriented applications, like an instant messenger or
 *     an email client.
 * @SN_ITEM_CATEGORY_SYSTEM_SERVICES: The item describes services of the
 *     system not seen as a stand alone application by the user, such as an
 *     indicator for the activity of a disk indexing service.
 * @SN_ITEM_CATEGORY_HARDWARE: The item describes the state and control of
 *     a particular hardware, such as an indicator of the battery charge or
 *     sound card volume control.
 *
 * Describes the category of this item.
 */
typedef enum
{
  SN_ITEM_CATEGORY_APPLICATION_STATUS,
  SN_ITEM_CATEGORY_COMMUNICATIONS,
  SN_ITEM_CATEGORY_SYSTEM_SERVICES,
  SN_ITEM_CATEGORY_HARDWARE
} SnItemCategory;

/**
 * SnItemStatus:
 * @SN_ITEM_STATUS_PASSIVE: The item doesn't convey important information
 *     to the user, it can be considered an "idle" status and is likely
 *     that visualizations will chose to hide it.
 * @SN_ITEM_STATUS_ACTIVE: The item is active, is more important that the
 *     item will be shown in some way to the user.
 * @SN_ITEM_STATUS_NEEDS_ATTENTION: The item carries really important
 *     information for the user, such as battery charge running out and
 *     is wants to incentive the direct user intervention. Visualizations
 *     should emphasize in some way the items with NeedsAttention status.
 *
 * Describes the status of this item or of the associated application.
 */
typedef enum
{
  SN_ITEM_STATUS_PASSIVE,
  SN_ITEM_STATUS_ACTIVE,
  SN_ITEM_STATUS_NEEDS_ATTENTION,
} SnItemStatus;

/**
 * SnItemOrientation:
 * @SN_ITEM_ORIENTATION_HORIZONTAL: Horizontal orientation.
 * @SN_ITEM_ORIENTATION_VERTICAL: Vertical orientation.
 *
 * The orientation of a scroll request performed on the representation of
 * the item in the visualization.
 */
typedef enum
{
  SN_ITEM_ORIENTATION_HORIZONTAL,
  SN_ITEM_ORIENTATION_VERTICAL
} SnItemOrientation;

/**
 * SnItemClass:
 * @parent_class: The parent class.
 * @context_menu: Asks the status notifier item to show a context menu, this
 *     is typically a consequence of user input, such as mouse right click
 *     over the graphical representation of the item.
 * @activate: Asks the status notifier item for activation, this is typically
 *     a consequence of user input, such as mouse left click over the
 *     graphical representation of the item. The application will perform any
 *     task is considered appropriate as an activation request.
 * @secondary_activate: Is to be considered a secondary and less important
 *     form of activation compared to Activate. This is typically a
 *     consequence of user input, such as mouse middle click over the
 *     graphical representation of the item. The application will perform any
 *     task is considered appropriate as an activation request.
 * @scroll: The user asked for a scroll action. This is caused from input
 *     such as mouse wheel over the graphical representation of the item.
 * @changed: Signal is emitted when @item properties has changed.
 * @ready: Signal is emitted when @item is ready to be used by hosts.
 * @error: Signal is emitted when error has occurred.
 *
 * The class structure for the #SnItem class.
 */
struct _SnItemClass
{
  GObjectClass parent_class;

  void (* context_menu)       (SnItem            *item,
                               gint               x,
                               gint               y);
  void (* activate)           (SnItem            *item,
                               gint               x,
                               gint               y);
  void (* secondary_activate) (SnItem            *item,
                               gint               x,
                               gint               y);
  void (* scroll)             (SnItem            *item,
                               gint               delta,
                               SnItemOrientation  orientation);

  void (* changed)            (SnItem            *item);
  void (* ready)              (SnItem            *item);

  void (* error)              (SnItem            *item,
                               const GError      *error);

  /*< private >*/
  gpointer padding[10];
};

SnItem          *sn_item_new_v0                     (SnItemCategory       category,
                                                     const gchar         *id);

SnItem          *sn_item_new_v1                     (SnItemCategory       category,
                                                     const gchar         *id);

const gchar     *sn_item_get_attention_icon_name    (SnItem              *item);
void             sn_item_set_attention_icon_name    (SnItem              *item,
                                                     const gchar         *attention_icon_name);

GdkPixbuf      **sn_item_get_attention_icon_pixbufs (SnItem              *item);
void             sn_item_set_attention_icon_pixbufs (SnItem              *item,
                                                     GdkPixbuf          **attention_icon_pixbufs);

const gchar     *sn_item_get_attention_movie_name   (SnItem              *item);
void             sn_item_set_attention_movie_name   (SnItem              *item,
                                                     const gchar         *attention_movie_name);

SnItemCategory   sn_item_get_category               (SnItem              *item);

const gchar     *sn_item_get_id                     (SnItem              *item);

const gchar     *sn_item_get_icon_name              (SnItem              *item);
void             sn_item_set_icon_name              (SnItem              *item,
                                                     const gchar         *icon_name);

GdkPixbuf      **sn_item_get_icon_pixbufs           (SnItem              *item);
void             sn_item_set_icon_pixbufs           (SnItem              *item,
                                                     GdkPixbuf          **icon_pixbufs);

const gchar     *sn_item_get_icon_theme_path        (SnItem              *item);
void             sn_item_set_icon_theme_path        (SnItem              *item,
                                                     const gchar         *icon_theme_path);

gboolean         sn_item_get_item_is_menu           (SnItem              *item);
void             sn_item_set_item_is_menu           (SnItem              *item,
                                                     gboolean             item_is_menu);

GtkMenu         *sn_item_get_menu                   (SnItem              *item);
void             sn_item_set_menu                   (SnItem              *item,
                                                     GtkMenu             *menu);

const gchar     *sn_item_get_overlay_icon_name      (SnItem              *item);
void             sn_item_set_overlay_icon_name      (SnItem              *item,
                                                     const gchar         *overlay_icon_name);

GdkPixbuf      **sn_item_get_overlay_icon_pixbufs   (SnItem              *item);
void             sn_item_set_overlay_icon_pixbufs   (SnItem              *item,
                                                     GdkPixbuf          **overlay_icon_pixbufs);

SnItemStatus     sn_item_get_status                 (SnItem              *item);
void             sn_item_set_status                 (SnItem              *item,
                                                     SnItemStatus         status);

const gchar     *sn_item_get_title                  (SnItem              *item);
void             sn_item_set_title                  (SnItem              *item,
                                                     const gchar         *title);

gboolean         sn_item_get_tooltip                (SnItem              *item,
                                                     const gchar        **icon_name,
                                                     GdkPixbuf         ***icon_pixbufs,
                                                     const gchar        **title,
                                                     const gchar        **text);
void             sn_item_set_tooltip                (SnItem              *item,
                                                     const gchar         *icon_name,
                                                     GdkPixbuf          **icon_pixbufs,
                                                     const gchar         *title,
                                                     const gchar         *text);

gint             sn_item_get_window_id              (SnItem              *item);
void             sn_item_set_window_id              (SnItem              *item,
                                                     gint                 window_id);

void             sn_item_context_menu               (SnItem              *item,
                                                     gint                 x,
                                                     gint                 y);
void             sn_item_activate                   (SnItem              *item,
                                                     gint                 x,
                                                     gint                 y);
void             sn_item_secondary_activate         (SnItem              *item,
                                                     gint                 x,
                                                     gint                 y);
void             sn_item_scroll                     (SnItem              *item,
                                                     gint                 delta,
                                                     SnItemOrientation    orientation);

void             sn_item_register                   (SnItem              *item);
void             sn_item_unregister                 (SnItem              *item);

const gchar     *sn_item_get_bus_name               (SnItem              *item);
const gchar     *sn_item_get_object_path            (SnItem              *item);

G_END_DECLS

#endif
