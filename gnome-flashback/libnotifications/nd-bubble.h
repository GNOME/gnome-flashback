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

#ifndef __ND_BUBBLE_H
#define __ND_BUBBLE_H

#include <gtk/gtk.h>
#include "nd-notification.h"

G_BEGIN_DECLS

#define ND_TYPE_BUBBLE         (nd_bubble_get_type ())
#define ND_BUBBLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), ND_TYPE_BUBBLE, NdBubble))
#define ND_BUBBLE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), ND_TYPE_BUBBLE, NdBubbleClass))
#define ND_IS_BUBBLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ND_TYPE_BUBBLE))
#define ND_IS_BUBBLE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), ND_TYPE_BUBBLE))
#define ND_BUBBLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), ND_TYPE_BUBBLE, NdBubbleClass))

typedef struct NdBubblePrivate NdBubblePrivate;

typedef struct
{
        GtkWindow        parent;
        NdBubblePrivate *priv;
} NdBubble;

typedef struct
{
        GtkWindowClass   parent_class;

        void          (* changed) (NdBubble      *bubble);
} NdBubbleClass;

GType               nd_bubble_get_type                      (void);

NdBubble *          nd_bubble_new_for_notification          (NdNotification *notification);

NdNotification *    nd_bubble_get_notification              (NdBubble       *bubble);

G_END_DECLS

#endif /* __ND_BUBBLE_H */
