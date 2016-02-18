/*
 * Copyright (C) 2010 Red Hat, Inc.
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

#ifndef __ND_NOTIFICATION_BOX_H
#define __ND_NOTIFICATION_BOX_H

#include <gtk/gtk.h>
#include "nd-notification.h"

G_BEGIN_DECLS

#define ND_TYPE_NOTIFICATION_BOX         (nd_notification_box_get_type ())
#define ND_NOTIFICATION_BOX(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), ND_TYPE_NOTIFICATION_BOX, NdNotificationBox))
#define ND_NOTIFICATION_BOX_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), ND_TYPE_NOTIFICATION_BOX, NdNotificationBoxClass))
#define ND_IS_NOTIFICATION_BOX(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ND_TYPE_NOTIFICATION_BOX))
#define ND_IS_NOTIFICATION_BOX_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), ND_TYPE_NOTIFICATION_BOX))
#define ND_NOTIFICATION_BOX_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), ND_TYPE_NOTIFICATION_BOX, NdNotificationBoxClass))

typedef struct NdNotificationBoxPrivate NdNotificationBoxPrivate;

typedef struct
{
        GtkEventBox               parent;
        NdNotificationBoxPrivate *priv;
} NdNotificationBox;

typedef struct
{
        GtkEventBoxClass   parent_class;
} NdNotificationBoxClass;

GType               nd_notification_box_get_type             (void);

NdNotificationBox * nd_notification_box_new_for_notification (NdNotification    *notification);

NdNotification *    nd_notification_box_get_notification     (NdNotificationBox *notification_box);

G_END_DECLS

#endif /* __ND_NOTIFICATION_BOX_H */
