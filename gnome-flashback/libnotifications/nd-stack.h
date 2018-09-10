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

#ifndef __ND_STACK_H
#define __ND_STACK_H

#include <gtk/gtk.h>
#include "gf-bubble.h"

G_BEGIN_DECLS

#define ND_TYPE_STACK         (nd_stack_get_type ())
#define ND_STACK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), ND_TYPE_STACK, NdStack))
#define ND_STACK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), ND_TYPE_STACK, NdStackClass))
#define ND_IS_STACK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ND_TYPE_STACK))
#define ND_IS_STACK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), ND_TYPE_STACK))
#define ND_STACK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), ND_TYPE_STACK, NdStackClass))

typedef struct NdStackPrivate NdStackPrivate;

typedef struct
{
        GObject         parent;
        NdStackPrivate *priv;
} NdStack;

typedef struct
{
        GObjectClass   parent_class;
} NdStackClass;

typedef enum
{
        ND_STACK_LOCATION_UNKNOWN = -1,
        ND_STACK_LOCATION_TOP_LEFT,
        ND_STACK_LOCATION_TOP_RIGHT,
        ND_STACK_LOCATION_BOTTOM_LEFT,
        ND_STACK_LOCATION_BOTTOM_RIGHT,
        ND_STACK_LOCATION_DEFAULT = ND_STACK_LOCATION_TOP_RIGHT
} NdStackLocation;

GType           nd_stack_get_type              (void);

NdStack *       nd_stack_new                   (GdkMonitor     *monitor);

void            nd_stack_set_location          (NdStack        *stack,
                                                NdStackLocation location);
void            nd_stack_add_bubble            (NdStack        *stack,
                                                GfBubble       *bubble);
void            nd_stack_remove_bubble         (NdStack        *stack,
                                                GfBubble       *bubble);
void            nd_stack_remove_all            (NdStack        *stack);
GList *         nd_stack_get_bubbles           (NdStack        *stack);
void            nd_stack_queue_update_position (NdStack        *stack);

G_END_DECLS

#endif /* __ND_STACK_H */
