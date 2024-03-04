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

#include "config.h"

#include <string.h>
#include <strings.h>
#include <glib.h>

#include "nd-stack.h"

#define NOTIFY_STACK_SPACING 2
#define WORKAREA_PADDING 6

struct NdStackPrivate
{
        GdkMonitor     *monitor;
        NdStackLocation location;
        GList          *bubbles;
        guint           update_id;
        GSettings      *settings;
};

static void     nd_stack_finalize    (GObject       *object);

G_DEFINE_TYPE_WITH_PRIVATE (NdStack, nd_stack, G_TYPE_OBJECT)

GList *
nd_stack_get_bubbles (NdStack *stack)
{
        return stack->priv->bubbles;
}

static void
get_origin_coordinates (NdStackLocation stack_location,
                        GdkRectangle       *workarea,
                        gint               *x,
                        gint               *y,
                        gint               *shiftx,
                        gint               *shifty,
                        gint                width,
                        gint                height)
{
        switch (stack_location) {
        case ND_STACK_LOCATION_TOP_LEFT:
                *x = workarea->x;
                *y = workarea->y;
                *shifty = height;
                break;

        case ND_STACK_LOCATION_TOP_RIGHT:
                *x = workarea->x + workarea->width - width;
                *y = workarea->y;
                *shifty = height;
                break;

        case ND_STACK_LOCATION_BOTTOM_LEFT:
                *x = workarea->x;
                *y = workarea->y + workarea->height - height;
                break;

        case ND_STACK_LOCATION_BOTTOM_RIGHT:
                *x = workarea->x + workarea->width - width;
                *y = workarea->y + workarea->height - height;
                break;

        case ND_STACK_LOCATION_UNKNOWN:
        default:
                g_assert_not_reached ();
        }
}

static void
translate_coordinates (NdStackLocation stack_location,
                       GdkRectangle       *workarea,
                       gint               *x,
                       gint               *y,
                       gint               *shiftx,
                       gint               *shifty,
                       gint                width,
                       gint                height)
{
        switch (stack_location) {
        case ND_STACK_LOCATION_TOP_LEFT:
                *x = workarea->x;
                *y += *shifty;
                *shifty = height;
                break;

        case ND_STACK_LOCATION_TOP_RIGHT:
                *x = workarea->x + workarea->width - width;
                *y += *shifty;
                *shifty = height;
                break;

        case ND_STACK_LOCATION_BOTTOM_LEFT:
                *x = workarea->x;
                *y -= height;
                break;

        case ND_STACK_LOCATION_BOTTOM_RIGHT:
                *x = workarea->x + workarea->width - width;
                *y -= height;
                break;

        case ND_STACK_LOCATION_UNKNOWN:
        default:
                g_assert_not_reached ();
        }
}

static void
location_changed_cb (GSettings     *settings,
                     const char    *key,
                     NdStack       *stack)
{
  stack->priv->location = g_settings_get_enum (stack->priv->settings, "location");
}

static void
nd_stack_class_init (NdStackClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = nd_stack_finalize;
}

static void
nd_stack_init (NdStack *stack)
{
        stack->priv = nd_stack_get_instance_private (stack);
        stack->priv->settings = g_settings_new ("org.gnome.gnome-flashback.notifications");
        stack->priv->location = g_settings_get_enum (stack->priv->settings, "location");

        g_signal_connect (stack->priv->settings, "changed::location",
                          G_CALLBACK (location_changed_cb), stack);
}

static void
nd_stack_finalize (GObject *object)
{
        NdStack *stack;

        g_return_if_fail (object != NULL);
        g_return_if_fail (ND_IS_STACK (object));

        stack = ND_STACK (object);

        g_return_if_fail (stack->priv != NULL);

        g_clear_object (&stack->priv->settings);

        if (stack->priv->update_id != 0) {
                g_source_remove (stack->priv->update_id);
        }

        g_list_free_full (stack->priv->bubbles, (GDestroyNotify) gtk_widget_destroy);

        G_OBJECT_CLASS (nd_stack_parent_class)->finalize (object);
}

void
nd_stack_set_location (NdStack        *stack,
                       NdStackLocation location)
{
        g_return_if_fail (ND_IS_STACK (stack));

        stack->priv->location = location;
}

NdStack *
nd_stack_new (GdkMonitor *monitor)
{
        NdStack *stack;

        g_assert (monitor != NULL && GDK_IS_MONITOR (monitor));

        stack = g_object_new (ND_TYPE_STACK, NULL);
        stack->priv->monitor = monitor;

        return stack;
}


static void
add_padding_to_rect (GdkRectangle *rect)
{
        rect->x += WORKAREA_PADDING;
        rect->y += WORKAREA_PADDING;
        rect->width -= WORKAREA_PADDING * 2;
        rect->height -= WORKAREA_PADDING * 2;

        if (rect->width < 0)
                rect->width = 0;
        if (rect->height < 0)
                rect->height = 0;
}

static void
nd_stack_shift_notifications (NdStack     *stack,
                              GfBubble    *bubble,
                              GList      **nw_l,
                              gint         init_width,
                              gint         init_height,
                              gint        *nw_x,
                              gint        *nw_y)
{
        GdkRectangle    workarea;
        GdkRectangle   *positions;
        GList          *l;
        gint            x, y;
        gint            shiftx = 0;
        gint            shifty = 0;
        int             i;
        int             n_wins;

        gdk_monitor_get_workarea (stack->priv->monitor, &workarea);

        add_padding_to_rect (&workarea);

        n_wins = g_list_length (stack->priv->bubbles);
        positions = g_new0 (GdkRectangle, n_wins);

        get_origin_coordinates (stack->priv->location,
                                &workarea,
                                &x, &y,
                                &shiftx,
                                &shifty,
                                init_width,
                                init_height);

        if (nw_x != NULL)
                *nw_x = x;

        if (nw_y != NULL)
                *nw_y = y;

        for (i = 0, l = stack->priv->bubbles; l != NULL; i++, l = l->next) {
                GfBubble       *nw2 = GF_BUBBLE (l->data);
                GtkRequisition  req;

                if (bubble == NULL || nw2 != bubble) {
                        gtk_widget_get_preferred_size (GTK_WIDGET (nw2), NULL, &req);

                        translate_coordinates (stack->priv->location,
                                               &workarea,
                                               &x,
                                               &y,
                                               &shiftx,
                                               &shifty,
                                               req.width,
                                               req.height + NOTIFY_STACK_SPACING);
                        positions[i].x = x;
                        positions[i].y = y;
                } else if (nw_l != NULL) {
                        *nw_l = l;
                        positions[i].x = -1;
                        positions[i].y = -1;
                }
        }

        /* move bubbles at the bottom of the stack first
           to avoid overlapping */
        for (i = n_wins - 1, l = g_list_last (stack->priv->bubbles); l != NULL; i--, l = l->prev) {
                GfBubble *nw2 = GF_BUBBLE (l->data);

                if (bubble == NULL || nw2 != bubble) {
                        gtk_window_move (GTK_WINDOW (nw2), positions[i].x, positions[i].y);
                }
        }

        g_free (positions);
}

static void
update_position (NdStack *stack)
{
        nd_stack_shift_notifications (stack,
                                      NULL, /* window */
                                      NULL, /* list pointer */
                                      0, /* init width */
                                      0, /* init height */
                                      NULL, /* out window x */
                                      NULL); /* out window y */
}

static gboolean
update_position_idle (NdStack *stack)
{
        update_position (stack);

        stack->priv->update_id = 0;
        return FALSE;
}

void
nd_stack_queue_update_position (NdStack *stack)
{
        if (stack->priv->update_id != 0) {
                return;
        }

        stack->priv->update_id = g_idle_add ((GSourceFunc) update_position_idle, stack);
}

void
nd_stack_add_bubble (NdStack  *stack,
                     GfBubble *bubble)
{
        GtkRequisition  req;
        int             x, y;

        gtk_widget_get_preferred_size (GTK_WIDGET (bubble), NULL, &req);
        nd_stack_shift_notifications (stack,
                                      bubble,
                                      NULL,
                                      req.width,
                                      req.height + NOTIFY_STACK_SPACING,
                                      &x,
                                      &y);
        gtk_widget_show (GTK_WIDGET (bubble));
        gtk_window_move (GTK_WINDOW (bubble), x, y);

        g_signal_connect_object (bubble, "destroy",
                                 G_CALLBACK (nd_stack_remove_bubble), stack,
                                 G_CONNECT_SWAPPED);
        stack->priv->bubbles = g_list_prepend (stack->priv->bubbles, bubble);
}

void
nd_stack_remove_bubble (NdStack  *stack,
                        GfBubble *bubble)
{
        GList *remove_l = NULL;

        nd_stack_shift_notifications (stack,
                                      bubble,
                                      &remove_l,
                                      0,
                                      0,
                                      NULL,
                                      NULL);

        if (remove_l != NULL)
                stack->priv->bubbles = g_list_delete_link (stack->priv->bubbles, remove_l);

        if (gtk_widget_get_realized (GTK_WIDGET (bubble)))
                gtk_widget_unrealize (GTK_WIDGET (bubble));
}

void
nd_stack_remove_all (NdStack  *stack)
{
        GList *bubbles;

        bubbles = g_list_copy (stack->priv->bubbles);
        g_list_free_full (bubbles, (GDestroyNotify) gtk_widget_destroy);
}
