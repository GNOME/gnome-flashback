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

#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>

#include "nd-stack.h"

#define ND_STACK_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ND_TYPE_STACK, NdStackPrivate))

#define NOTIFY_STACK_SPACING 2
#define WORKAREA_PADDING 6

struct NdStackPrivate
{
        GdkScreen      *screen;
        guint           monitor;
        NdStackLocation location;
        GList          *bubbles;
        guint           update_id;
};

static void     nd_stack_finalize    (GObject       *object);

G_DEFINE_TYPE (NdStack, nd_stack, G_TYPE_OBJECT)

GList *
nd_stack_get_bubbles (NdStack *stack)
{
        return stack->priv->bubbles;
}

static int
get_current_desktop (GdkScreen *screen)
{
        Display *display;
        Window win;
        Atom current_desktop, type;
        int format;
        unsigned long n_items, bytes_after;
        unsigned char *data_return = NULL;
        int workspace = 0;

        display = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));
        win = XRootWindow (display, GDK_SCREEN_XNUMBER (screen));

        current_desktop = XInternAtom (display, "_NET_CURRENT_DESKTOP", True);

        XGetWindowProperty (display,
                            win,
                            current_desktop,
                            0, G_MAXLONG,
                            False, XA_CARDINAL,
                            &type, &format, &n_items, &bytes_after,
                            &data_return);

        if (type == XA_CARDINAL && format == 32 && n_items > 0)
                workspace = (int) data_return[0];
        if (data_return)
                XFree (data_return);

        return workspace;
}

static gboolean
get_work_area (NdStack      *stack,
               GdkRectangle *rect)
{
        Atom            workarea;
        Atom            type;
        Window          win;
        int             format;
        gulong          num;
        gulong          leftovers;
        gulong          max_len = 4 * 32;
        guchar         *ret_workarea;
        long           *workareas;
        int             result;
        int             disp_screen;
        int             desktop;
        Display        *display;

        display = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (stack->priv->screen));
        workarea = XInternAtom (display, "_NET_WORKAREA", True);

        disp_screen = GDK_SCREEN_XNUMBER (stack->priv->screen);

        /* Defaults in case of error */
        rect->x = 0;
        rect->y = 0;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        rect->width = gdk_screen_get_width (stack->priv->screen);
        rect->height = gdk_screen_get_height (stack->priv->screen);
        G_GNUC_END_IGNORE_DEPRECATIONS

        if (workarea == None)
                return FALSE;

        win = XRootWindow (display, disp_screen);
        result = XGetWindowProperty (display,
                                     win,
                                     workarea,
                                     0,
                                     max_len,
                                     False,
                                     AnyPropertyType,
                                     &type,
                                     &format,
                                     &num,
                                     &leftovers,
                                     &ret_workarea);

        if (result != Success
            || type == None
            || format == 0
            || leftovers
            || num % 4) {
                return FALSE;
        }

        desktop = get_current_desktop (stack->priv->screen);

        workareas = (long *) ret_workarea;
        rect->x = workareas[desktop * 4];
        rect->y = workareas[desktop * 4 + 1];
        rect->width = workareas[desktop * 4 + 2];
        rect->height = workareas[desktop * 4 + 3];

        XFree (ret_workarea);

        return TRUE;
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
nd_stack_class_init (NdStackClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = nd_stack_finalize;

        g_type_class_add_private (klass, sizeof (NdStackPrivate));
}

static void
nd_stack_init (NdStack *stack)
{
        stack->priv = ND_STACK_GET_PRIVATE (stack);
        stack->priv->location = ND_STACK_LOCATION_DEFAULT;
}

static void
nd_stack_finalize (GObject *object)
{
        NdStack *stack;

        g_return_if_fail (object != NULL);
        g_return_if_fail (ND_IS_STACK (object));

        stack = ND_STACK (object);

        g_return_if_fail (stack->priv != NULL);

        if (stack->priv->update_id != 0) {
                g_source_remove (stack->priv->update_id);
        }

        g_list_free (stack->priv->bubbles);

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
nd_stack_new (GdkScreen *screen,
              guint      monitor)
{
        NdStack *stack;

        g_assert (screen != NULL && GDK_IS_SCREEN (screen));

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        g_assert (monitor < (guint)gdk_screen_get_n_monitors (screen));
        G_GNUC_END_IGNORE_DEPRECATIONS

        stack = g_object_new (ND_TYPE_STACK, NULL);
        stack->priv->screen = screen;
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
        GdkRectangle    monitor;
        GdkRectangle   *positions;
        GList          *l;
        gint            x, y;
        gint            shiftx = 0;
        gint            shifty = 0;
        int             i;
        int             n_wins;

        get_work_area (stack, &workarea);

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gdk_screen_get_monitor_geometry (stack->priv->screen,
                                         stack->priv->monitor,
                                         &monitor);
        G_GNUC_END_IGNORE_DEPRECATIONS

        gdk_rectangle_intersect (&monitor, &workarea, &workarea);

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
                     GfBubble *bubble,
                     gboolean  new_notification)
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

        if (new_notification) {
                g_signal_connect_swapped (G_OBJECT (bubble),
                                          "destroy",
                                          G_CALLBACK (nd_stack_remove_bubble),
                                          stack);
                stack->priv->bubbles = g_list_prepend (stack->priv->bubbles, bubble);
        }
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
        g_list_foreach (bubbles, (GFunc)gtk_widget_destroy, NULL);
        g_list_free (bubbles);
}
