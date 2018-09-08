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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>

#include "nd-queue.h"

#include "nd-notification.h"
#include "nd-notification-box.h"
#include "nd-stack.h"

#define WIDTH         400

typedef struct
{
        NdStack   **stacks;
        int         n_stacks;
        Atom        workarea_atom;
} NotifyScreen;

struct NdQueuePrivate
{
        GHashTable    *notifications;
        GHashTable    *bubbles;
        GQueue        *queue;

        GtkStatusIcon *status_icon;
        GIcon         *numerable_icon;
        GtkWidget     *dock;
        GtkWidget     *dock_scrolled_window;

        NotifyScreen  *screen;

        guint          update_id;
};

enum {
        CHANGED,
        LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

static void     nd_queue_finalize       (GObject        *object);
static void     queue_update            (NdQueue        *queue);
static void     on_notification_close   (NdNotification *notification,
                                         int             reason,
                                         NdQueue        *queue);

static gpointer queue_object = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (NdQueue, nd_queue, G_TYPE_OBJECT)

static void
create_stack_for_monitor (NdQueue    *queue,
                          GdkScreen  *screen,
                          int         monitor_num)
{
        NotifyScreen *nscreen;

        nscreen = queue->priv->screen;

        nscreen->stacks[monitor_num] = nd_stack_new (screen,
                                                     monitor_num);
}

static void
on_screen_monitors_changed (GdkScreen *screen,
                            NdQueue   *queue)
{
        NotifyScreen *nscreen;
        int           n_monitors;
        int           i;

        nscreen = queue->priv->screen;

        n_monitors = gdk_screen_get_n_monitors (screen);

        if (n_monitors > nscreen->n_stacks) {
                /* grow */
                nscreen->stacks = g_renew (NdStack *,
                                           nscreen->stacks,
                                           n_monitors);

                /* add more stacks */
                for (i = nscreen->n_stacks; i < n_monitors; i++) {
                        create_stack_for_monitor (queue, screen, i);
                }

                nscreen->n_stacks = n_monitors;
        } else if (n_monitors < nscreen->n_stacks) {
                NdStack *last_stack;

                last_stack = nscreen->stacks[n_monitors - 1];

                /* transfer items before removing stacks */
                for (i = n_monitors; i < nscreen->n_stacks; i++) {
                        NdStack     *stack;
                        GList       *bubbles;
                        GList       *l;

                        stack = nscreen->stacks[i];
                        bubbles = g_list_copy (nd_stack_get_bubbles (stack));
                        for (l = bubbles; l != NULL; l = l->next) {
                                /* skip removing the bubble from the
                                   old stack since it will try to
                                   unrealize the window.  And the
                                   stack is going away anyhow. */
                                nd_stack_add_bubble (last_stack, l->data, TRUE);
                        }
                        g_list_free (bubbles);
                        g_object_unref (stack);
                        nscreen->stacks[i] = NULL;
                }

                /* remove the extra stacks */
                nscreen->stacks = g_renew (NdStack *,
                                           nscreen->stacks,
                                           n_monitors);
                nscreen->n_stacks = n_monitors;
        }
}

static void
create_stacks_for_screen (NdQueue   *queue,
                          GdkScreen *screen)
{
        NotifyScreen *nscreen;
        int           i;

        nscreen = queue->priv->screen;

        nscreen->n_stacks = gdk_screen_get_n_monitors (screen);

        nscreen->stacks = g_renew (NdStack *,
                                   nscreen->stacks,
                                   nscreen->n_stacks);

        for (i = 0; i < nscreen->n_stacks; i++) {
                create_stack_for_monitor (queue, screen, i);
        }
}

static GdkFilterReturn
screen_xevent_filter (GdkXEvent    *xevent,
                      GdkEvent     *event,
                      NotifyScreen *nscreen)
{
        XEvent *xev;

        xev = (XEvent *) xevent;

        if (xev->type == PropertyNotify &&
            xev->xproperty.atom == nscreen->workarea_atom) {
                int i;

                for (i = 0; i < nscreen->n_stacks; i++) {
                        nd_stack_queue_update_position (nscreen->stacks[i]);
                }
        }

        return GDK_FILTER_CONTINUE;
}

static void
create_screen (NdQueue *queue)
{
        GdkDisplay *display;
        GdkScreen  *screen;
        GdkWindow  *gdkwindow;

        g_assert (queue->priv->screen == NULL);

        display = gdk_display_get_default ();
        screen = gdk_display_get_default_screen (display);

        g_signal_connect (screen,
                          "monitors-changed",
                          G_CALLBACK (on_screen_monitors_changed),
                          queue);

        queue->priv->screen = g_new0 (NotifyScreen, 1);
        queue->priv->screen->workarea_atom = XInternAtom (GDK_DISPLAY_XDISPLAY (display), "_NET_WORKAREA", True);

        gdkwindow = gdk_screen_get_root_window (screen);
        gdk_window_add_filter (gdkwindow, (GdkFilterFunc) screen_xevent_filter, queue->priv->screen);
        gdk_window_set_events (gdkwindow, gdk_window_get_events (gdkwindow) | GDK_PROPERTY_CHANGE_MASK);

        create_stacks_for_screen (queue, screen);
}

static void
nd_queue_class_init (NdQueueClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = nd_queue_finalize;

        signals [CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (NdQueueClass, changed),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
}

static void
ungrab (NdQueue *queue,
        guint    time)
{
        GdkDisplay *display;
        GdkSeat *seat;

        display = gtk_widget_get_display (queue->priv->dock);
        seat = gdk_display_get_default_seat (display);

        gdk_seat_ungrab (seat);

        gtk_grab_remove (queue->priv->dock);

        /* hide again */
        gtk_widget_hide (queue->priv->dock);
}

static void
popdown_dock (NdQueue *queue)
{
        ungrab (queue, GDK_CURRENT_TIME);
        queue_update (queue);
}

static void
release_grab (NdQueue        *queue,
              GdkEventButton *event)
{
        ungrab (queue, event->time);
}

/* This is called when the grab is broken for
 * either the dock, or the scale itself */
static void
grab_notify (NdQueue   *queue,
             gboolean   was_grabbed)
{
        GtkWidget *current;

        if (was_grabbed) {
                return;
        }

        if (!gtk_widget_has_grab (queue->priv->dock)) {
                return;
        }

        current = gtk_grab_get_current ();
        if (current == queue->priv->dock
            || gtk_widget_is_ancestor (current, queue->priv->dock)) {
                return;
        }

        popdown_dock (queue);
}

static void
on_dock_grab_notify (GtkWidget *widget,
                     gboolean   was_grabbed,
                     NdQueue   *queue)
{
        grab_notify (queue, was_grabbed);
}

static gboolean
on_dock_grab_broken_event (GtkWidget *widget,
                           gboolean   was_grabbed,
                           NdQueue   *queue)
{
        grab_notify (queue, FALSE);

        return FALSE;
}

static gboolean
on_dock_key_release (GtkWidget   *widget,
                     GdkEventKey *event,
                     NdQueue     *queue)
{
        if (event->keyval == GDK_KEY_Escape) {
                popdown_dock (queue);
                return TRUE;
        }

        return TRUE;
}

static void
clear_stacks (NdQueue *queue)
{
        NotifyScreen *nscreen;
        gint          i;

        nscreen = queue->priv->screen;
        for (i = 0; i < nscreen->n_stacks; i++) {
                NdStack *stack;
                stack = nscreen->stacks[i];
                nd_stack_remove_all (stack);
        }
}

static void
_nd_queue_remove_all (NdQueue *queue)
{
        GHashTableIter iter;
        gpointer       key, value;
        gboolean       changed;

        changed = FALSE;

        clear_stacks (queue);

        g_queue_clear (queue->priv->queue);
        g_hash_table_iter_init (&iter, queue->priv->notifications);
        while (g_hash_table_iter_next (&iter, &key, &value)) {
                NdNotification *n = ND_NOTIFICATION (value);

                g_signal_handlers_disconnect_by_func (n, G_CALLBACK (on_notification_close), queue);
                nd_notification_close (n, ND_NOTIFICATION_CLOSED_USER);
                g_hash_table_iter_remove (&iter);
                changed = TRUE;
        }
        popdown_dock (queue);
        queue_update (queue);

        if (changed) {
                g_signal_emit (queue, signals[CHANGED], 0);
        }
}

static void
on_clear_all_clicked (GtkButton *button,
                      NdQueue   *queue)
{
        _nd_queue_remove_all (queue);
}

static gboolean
on_dock_button_press (GtkWidget      *widget,
                      GdkEventButton *event,
                      NdQueue        *queue)
{
        GtkWidget *event_widget;

        if (event->type != GDK_BUTTON_PRESS) {
                return FALSE;
        }
        event_widget = gtk_get_event_widget ((GdkEvent *)event);
        g_debug ("Button press: %p dock=%p", event_widget, widget);
        if (event_widget == widget) {
                release_grab (queue, event);
                return TRUE;
        }

        return FALSE;
}

static void
create_dock (NdQueue *queue)
{
        GtkWidget *frame;
        GtkWidget *box;
        GtkWidget *button;

        queue->priv->dock = gtk_window_new (GTK_WINDOW_POPUP);
        gtk_widget_add_events (queue->priv->dock,
                               GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

        gtk_widget_set_name (queue->priv->dock, "notification-popup-window");
        g_signal_connect (queue->priv->dock,
                          "grab-notify",
                          G_CALLBACK (on_dock_grab_notify),
                          queue);
        g_signal_connect (queue->priv->dock,
                          "grab-broken-event",
                          G_CALLBACK (on_dock_grab_broken_event),
                          queue);
        g_signal_connect (queue->priv->dock,
                          "key-release-event",
                          G_CALLBACK (on_dock_key_release),
                          queue);
        g_signal_connect (queue->priv->dock,
                          "button-press-event",
                          G_CALLBACK (on_dock_button_press),
                          queue);
#if 0
        g_signal_connect (queue->priv->dock,
                          "scroll-event",
                          G_CALLBACK (on_dock_scroll_event),
                          queue);
#endif
        gtk_window_set_decorated (GTK_WINDOW (queue->priv->dock), FALSE);

        frame = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
        gtk_container_add (GTK_CONTAINER (queue->priv->dock), frame);

        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width (GTK_CONTAINER (box), 2);
        gtk_container_add (GTK_CONTAINER (frame), box);

        queue->priv->dock_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (queue->priv->dock_scrolled_window),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request (queue->priv->dock_scrolled_window,
                                     WIDTH,
                                     -1);
        gtk_box_pack_start (GTK_BOX (box), queue->priv->dock_scrolled_window, TRUE, TRUE, 0);

        button = gtk_button_new_with_label (_("Clear all notifications"));
        g_signal_connect (button, "clicked", G_CALLBACK (on_clear_all_clicked), queue);
        gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);
}

static void
nd_queue_init (NdQueue *queue)
{
        queue->priv = nd_queue_get_instance_private (queue);
        queue->priv->notifications = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
        queue->priv->bubbles = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
        queue->priv->queue = g_queue_new ();
        queue->priv->status_icon = NULL;

        create_dock (queue);
        create_screen (queue);
}

static void
destroy_screen (NdQueue *queue)
{
        GdkDisplay *display;
        GdkScreen  *screen;
        GdkWindow  *gdkwindow;
        gint        i;

        display = gdk_display_get_default ();
        screen = gdk_display_get_default_screen (display);

        g_signal_handlers_disconnect_by_func (screen,
                                              G_CALLBACK (on_screen_monitors_changed),
                                              queue);

        gdkwindow = gdk_screen_get_root_window (screen);
        gdk_window_remove_filter (gdkwindow, (GdkFilterFunc) screen_xevent_filter, queue->priv->screen);
        for (i = 0; i < queue->priv->screen->n_stacks; i++) {
                g_clear_object (&queue->priv->screen->stacks[i]);
        }

        g_free (queue->priv->screen->stacks);
        queue->priv->screen->stacks = NULL;

        g_free (queue->priv->screen);
        queue->priv->screen = NULL;
}

static void
nd_queue_finalize (GObject *object)
{
        NdQueue *queue;

        g_return_if_fail (object != NULL);
        g_return_if_fail (ND_IS_QUEUE (object));

        queue = ND_QUEUE (object);

        g_return_if_fail (queue->priv != NULL);

        if (queue->priv->update_id != 0) {
                g_source_remove (queue->priv->update_id);
        }

        g_hash_table_destroy (queue->priv->notifications);
        g_hash_table_destroy (queue->priv->bubbles);
        g_queue_free (queue->priv->queue);

        destroy_screen (queue);

        g_clear_object (&queue->priv->numerable_icon);
        g_clear_object (&queue->priv->status_icon);

        G_OBJECT_CLASS (nd_queue_parent_class)->finalize (object);
}

NdNotification *
nd_queue_lookup (NdQueue *queue,
                 guint    id)
{
        NdNotification *notification;

        g_return_val_if_fail (ND_IS_QUEUE (queue), NULL);

        notification = g_hash_table_lookup (queue->priv->notifications, GUINT_TO_POINTER (id));

        return notification;
}

guint
nd_queue_length (NdQueue *queue)
{
        g_return_val_if_fail (ND_IS_QUEUE (queue), 0);

        return g_hash_table_size (queue->priv->notifications);
}

static NdStack *
get_stack_with_pointer (NdQueue *queue)
{
        GdkDisplay *display;
        GdkSeat *seat;
        GdkDevice *pointer;
        GdkScreen *screen;
        int        x, y;
        int        monitor_num;

        display = gdk_display_get_default ();
        seat = gdk_display_get_default_seat (display);
        pointer = gdk_seat_get_pointer (seat);

        gdk_device_get_position (pointer, &screen, &x, &y);
        monitor_num = gdk_screen_get_monitor_at_point (screen, x, y);

        if (monitor_num >= queue->priv->screen->n_stacks) {
                /* screw it - dump it on the last one we'll get
                   a monitors-changed signal soon enough*/
                monitor_num = queue->priv->screen->n_stacks - 1;
        }

        return queue->priv->screen->stacks[monitor_num];
}

static void
on_bubble_destroyed (GfBubble *bubble,
                     NdQueue  *queue)
{
        NdNotification *notification;

        g_debug ("Bubble destroyed");
        notification = gf_bubble_get_notification (bubble);

        nd_notification_set_is_queued (notification, FALSE);

        if (nd_notification_get_is_transient (notification)) {
                g_debug ("Bubble is transient");
                nd_notification_close (notification, ND_NOTIFICATION_CLOSED_EXPIRED);
        }

        queue_update (queue);
}

static void
maybe_show_notification (NdQueue *queue)
{
        gpointer        id;
        NdNotification *notification;
        GfBubble       *bubble;
        NdStack        *stack;
        GList          *list;

        /* FIXME: show one at a time if not busy or away */

        /* don't show bubbles when dock is showing */
        if (gtk_widget_get_visible (queue->priv->dock)) {
                g_debug ("Dock is showing");
                return;
        }

        stack = get_stack_with_pointer (queue);
        list = nd_stack_get_bubbles (stack);
        if (g_list_length (list) > 0) {
                /* already showing bubbles */
                g_debug ("Already showing bubbles");
                return;
        }

        id = g_queue_pop_tail (queue->priv->queue);
        if (id == NULL) {
                /* Nothing to do */
                g_debug ("No queued notifications");
                return;
        }

        notification = g_hash_table_lookup (queue->priv->notifications, id);
        g_assert (notification != NULL);

        bubble = gf_bubble_new_for_notification (notification);
        g_signal_connect (bubble, "destroy", G_CALLBACK (on_bubble_destroyed), queue);

        nd_stack_add_bubble (stack, bubble, TRUE);
}

static int
collate_notifications (NdNotification *a,
                       NdNotification *b)
{
        GTimeVal tva;
        GTimeVal tvb;

        nd_notification_get_update_time (a, &tva);
        nd_notification_get_update_time (b, &tvb);
        if (tva.tv_sec > tvb.tv_sec) {
                return 1;
        } else {
                return -1;
        }
}

static void
update_dock (NdQueue *queue)
{
        GtkWidget   *child;
        GList       *list;
        GList       *l;
        int          min_height;
        int          height;
        int          monitor_num;
        GdkScreen   *screen;
        GdkRectangle area;
        GtkStatusIcon *status_icon;
        gboolean visible;

        g_return_if_fail (queue);

        child = gtk_bin_get_child (GTK_BIN (queue->priv->dock_scrolled_window));
        if (child != NULL)
                gtk_container_remove (GTK_CONTAINER (queue->priv->dock_scrolled_window), child);

        child = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_add (GTK_CONTAINER (queue->priv->dock_scrolled_window),
                           child);

        gtk_container_set_focus_hadjustment (GTK_CONTAINER (child),
                                             gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (queue->priv->dock_scrolled_window)));
        gtk_container_set_focus_vadjustment (GTK_CONTAINER (child),
                                             gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (queue->priv->dock_scrolled_window)));

        list = g_hash_table_get_values (queue->priv->notifications);
        list = g_list_sort (list, (GCompareFunc)collate_notifications);

        for (l = list; l != NULL; l = l->next) {
                NdNotification    *n = l->data;
                NdNotificationBox *box;
                GtkWidget         *sep;

                box = nd_notification_box_new_for_notification (n);
                gtk_widget_show (GTK_WIDGET (box));
                gtk_box_pack_start (GTK_BOX (child), GTK_WIDGET (box), FALSE, FALSE, 0);

                sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
                gtk_widget_show (sep);
                gtk_box_pack_start (GTK_BOX (child), sep, FALSE, FALSE, 0);
        }
        gtk_widget_show (child);

        status_icon = queue->priv->status_icon;
        visible = FALSE;

        if (status_icon != NULL) {
                G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                visible = gtk_status_icon_get_visible (status_icon);
                G_GNUC_END_IGNORE_DEPRECATIONS
        }

        if (visible) {
                gtk_widget_get_preferred_height (child, &min_height, &height);

                G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gtk_status_icon_get_geometry (status_icon, &screen, &area, NULL);
                G_GNUC_END_IGNORE_DEPRECATIONS

                monitor_num = gdk_screen_get_monitor_at_point (screen, area.x, area.y);
                gdk_screen_get_monitor_geometry (screen, monitor_num, &area);
                height = MIN (height, (area.height / 2));
                gtk_widget_set_size_request (queue->priv->dock_scrolled_window,
                                             WIDTH,
                                             height);
        }

        g_list_free (list);
}

static void
show_all_cb (GtkWidget *widget,
             gpointer   user_data)
{
  gtk_widget_show_all (widget);
}

static gboolean
popup_dock (NdQueue *queue,
            guint    time)
{
        GdkRectangle   area;
        GtkOrientation orientation;
        GdkDisplay    *display;
        GdkScreen     *screen;
        gboolean       res;
        int            x;
        int            y;
        int            monitor_num;
        GdkRectangle   monitor;
        GtkRequisition dock_req;
        GtkStatusIcon *status_icon;
        GdkWindow *window;
        GdkSeat *seat;
        GdkSeatCapabilities capabilities;
        GdkGrabStatus status;

        update_dock (queue);

        status_icon = queue->priv->status_icon;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        res = gtk_status_icon_get_geometry (status_icon, &screen, &area, &orientation);
        G_GNUC_END_IGNORE_DEPRECATIONS

        if (! res) {
                g_warning ("Unable to determine geometry of status icon");
                return FALSE;
        }

        /* position roughly */
        gtk_window_set_screen (GTK_WINDOW (queue->priv->dock), screen);

        monitor_num = gdk_screen_get_monitor_at_point (screen, area.x, area.y);
        gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

        gtk_container_foreach (GTK_CONTAINER (queue->priv->dock),
                               show_all_cb, NULL);
        gtk_widget_get_preferred_size (queue->priv->dock, &dock_req, NULL);

        if (orientation == GTK_ORIENTATION_VERTICAL) {
                if (area.x + area.width + dock_req.width <= monitor.x + monitor.width) {
                        x = area.x + area.width;
                } else {
                        x = area.x - dock_req.width;
                }
                if (area.y + dock_req.height <= monitor.y + monitor.height) {
                        y = area.y;
                } else {
                        y = monitor.y + monitor.height - dock_req.height;
                }
        } else {
                if (area.y + area.height + dock_req.height <= monitor.y + monitor.height) {
                        y = area.y + area.height;
                } else {
                        y = area.y - dock_req.height;
                }
                if (area.x + dock_req.width <= monitor.x + monitor.width) {
                        x = area.x;
                } else {
                        x = monitor.x + monitor.width - dock_req.width;
                }
        }

        gtk_window_move (GTK_WINDOW (queue->priv->dock), x, y);

        /* FIXME: without this, the popup window appears as a square
         * after changing the orientation
         */
        gtk_window_resize (GTK_WINDOW (queue->priv->dock), 1, 1);

        gtk_widget_show_all (queue->priv->dock);

        /* grab focus */
        gtk_grab_add (queue->priv->dock);

        display = gtk_widget_get_display (queue->priv->dock);
        window = gtk_widget_get_window (queue->priv->dock);
        seat = gdk_display_get_default_seat (display);

        capabilities = GDK_SEAT_CAPABILITY_POINTER |
                       GDK_SEAT_CAPABILITY_KEYBOARD;

        status = gdk_seat_grab (seat, window, capabilities, TRUE, NULL,
                                NULL, NULL, NULL);

        if (status != GDK_GRAB_SUCCESS) {
                ungrab (queue, time);
                return FALSE;
        }

        gtk_widget_grab_focus (queue->priv->dock);

        return TRUE;
}

static void
show_dock (NdQueue *queue)
{
        /* clear the bubble queue since the user will be looking at a
           full list now */
        clear_stacks (queue);
        g_queue_clear (queue->priv->queue);

        popup_dock (queue, GDK_CURRENT_TIME);
}

static void
on_status_icon_popup_menu (GtkStatusIcon *status_icon,
                           guint          button,
                           guint          activate_time,
                           NdQueue       *queue)
{
        show_dock (queue);
}

static void
on_status_icon_activate (GtkStatusIcon *status_icon,
                         NdQueue       *queue)
{
        show_dock (queue);
}

static void
on_status_icon_visible_notify (GtkStatusIcon *icon,
                               GParamSpec    *pspec,
                               NdQueue       *queue)
{
        gboolean visible;

        g_object_get (icon, "visible", &visible, NULL);
        if (! visible) {
                if (queue->priv->dock != NULL) {
                        gtk_widget_hide (queue->priv->dock);
                }
        }
}

static gboolean
update_idle (NdQueue *queue)
{
        int num;

        num = g_hash_table_size (queue->priv->notifications);

        /* Show the status icon when their are stored notifications */
        if (num > 0) {
                if (gtk_widget_get_visible (queue->priv->dock)) {
                        update_dock (queue);
                }

                if (queue->priv->status_icon == NULL) {
                        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                        queue->priv->status_icon = gtk_status_icon_new ();
                        gtk_status_icon_set_title (queue->priv->status_icon,
                                                   _("Notifications"));
                        G_GNUC_END_IGNORE_DEPRECATIONS

                        g_signal_connect (queue->priv->status_icon,
                                          "activate",
                                          G_CALLBACK (on_status_icon_activate),
                                          queue);
                        g_signal_connect (queue->priv->status_icon,
                                          "popup-menu",
                                          G_CALLBACK (on_status_icon_popup_menu),
                                          queue);
                        g_signal_connect (queue->priv->status_icon,
                                          "notify::visible",
                                          G_CALLBACK (on_status_icon_visible_notify),
                                          queue);
                }

                if (queue->priv->numerable_icon == NULL) {
                        GIcon *icon;
                        /* FIXME: use a more appropriate icon here */
                        icon = g_themed_icon_new ("mail-message-new");

                        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                        queue->priv->numerable_icon = gtk_numerable_icon_new (icon);
                        G_GNUC_END_IGNORE_DEPRECATIONS

                        g_object_unref (icon);
                }

                G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gtk_numerable_icon_set_count (GTK_NUMERABLE_ICON (queue->priv->numerable_icon), num);
                gtk_status_icon_set_from_gicon (queue->priv->status_icon,
                                                queue->priv->numerable_icon);
                gtk_status_icon_set_visible (queue->priv->status_icon, TRUE);
                G_GNUC_END_IGNORE_DEPRECATIONS

                maybe_show_notification (queue);
        } else {
                if (gtk_widget_get_visible (queue->priv->dock)) {
                        popdown_dock (queue);
                }

                if (queue->priv->status_icon != NULL) {
                        g_object_unref (queue->priv->status_icon);
                        queue->priv->status_icon = NULL;
                }
        }

        queue->priv->update_id = 0;
        return FALSE;
}

static void
queue_update (NdQueue *queue)
{
        if (queue->priv->update_id > 0) {
                g_source_remove (queue->priv->update_id);
        }

        queue->priv->update_id = g_idle_add ((GSourceFunc)update_idle, queue);
}

static void
_nd_queue_remove (NdQueue        *queue,
                  NdNotification *notification)
{
        guint id;

        id = nd_notification_get_id (notification);
        g_debug ("Removing id %u", id);

        /* FIXME: withdraw currently showing bubbles */

        g_signal_handlers_disconnect_by_func (notification, G_CALLBACK (on_notification_close), queue);

        if (queue->priv->queue != NULL) {
                g_queue_remove (queue->priv->queue, GUINT_TO_POINTER (id));
        }
        g_hash_table_remove (queue->priv->notifications, GUINT_TO_POINTER (id));

        /* FIXME: should probably only emit this when it really removes something */
        g_signal_emit (queue, signals[CHANGED], 0);

        queue_update (queue);
}

static void
on_notification_close (NdNotification *notification,
                       int             reason,
                       NdQueue        *queue)
{
        g_debug ("Notification closed - removing from queue");
        _nd_queue_remove (queue, notification);
}

void
nd_queue_remove_for_id (NdQueue *queue,
                        guint    id)
{
        NdNotification *notification;

        g_return_if_fail (ND_IS_QUEUE (queue));

        notification = g_hash_table_lookup (queue->priv->notifications, GUINT_TO_POINTER (id));
        if (notification != NULL) {
                _nd_queue_remove (queue, notification);
        }
}

void
nd_queue_add (NdQueue        *queue,
              NdNotification *notification)
{
        guint id;

        g_return_if_fail (ND_IS_QUEUE (queue));

        id = nd_notification_get_id (notification);
        g_debug ("Adding id %u", id);
        g_hash_table_insert (queue->priv->notifications, GUINT_TO_POINTER (id), g_object_ref (notification));
        g_queue_push_head (queue->priv->queue, GUINT_TO_POINTER (id));

        g_signal_connect (notification, "closed", G_CALLBACK (on_notification_close), queue);

        /* FIXME: should probably only emit this when it really adds something */
        g_signal_emit (queue, signals[CHANGED], 0);

        queue_update (queue);
}

NdQueue *
nd_queue_new (void)
{
        if (queue_object != NULL) {
                g_object_ref (queue_object);
        } else {
                queue_object = g_object_new (ND_TYPE_QUEUE, NULL);
                g_object_add_weak_pointer (queue_object,
                                           (gpointer *) &queue_object);
        }

        return ND_QUEUE (queue_object);
}
