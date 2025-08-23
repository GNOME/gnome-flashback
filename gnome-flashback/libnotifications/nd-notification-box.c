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
#include <glib/gi18n.h>

#include "nd-notification.h"
#include "nd-notification-box.h"

#define IMAGE_SIZE    48
#define BODY_X_OFFSET (IMAGE_SIZE + 8)
#define WIDTH         400

struct NdNotificationBoxPrivate
{
        NdNotification *notification;

        GtkWidget      *icon;
        GtkWidget      *close_button;
        GtkWidget      *summary_label;
        GtkWidget      *body_label;

        GtkWidget      *main_hbox;
        GtkWidget      *content_hbox;
        GtkWidget      *actions_box;
        GtkWidget      *last_sep;
};

static void     nd_notification_box_finalize    (GObject                *object);

G_DEFINE_TYPE_WITH_PRIVATE (NdNotificationBox, nd_notification_box, GTK_TYPE_EVENT_BOX)

NdNotification *
nd_notification_box_get_notification (NdNotificationBox *notification_box)
{
        g_return_val_if_fail (ND_IS_NOTIFICATION_BOX (notification_box), NULL);

        return notification_box->priv->notification;
}

static gboolean
nd_notification_box_button_release_event (GtkWidget      *widget,
                                          GdkEventButton *event)
{
        NdNotificationBox *notification_box = ND_NOTIFICATION_BOX (widget);

        nd_notification_action_invoked (notification_box->priv->notification,
                                        "default",
                                        event->time);

        return FALSE;
}

static void
nd_notification_box_class_init (NdNotificationBoxClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = nd_notification_box_finalize;
        widget_class->button_release_event = nd_notification_box_button_release_event;
}

static void
on_close_button_clicked (GtkButton         *button,
                         NdNotificationBox *notification_box)
{
        nd_notification_close (notification_box->priv->notification, ND_NOTIFICATION_CLOSED_USER);
}

static void
on_action_clicked (GtkButton         *button,
                   GdkEventButton    *event,
                   NdNotificationBox *notification_box)
{
        const char *key = g_object_get_data (G_OBJECT (button), "_action_key");

        nd_notification_action_invoked (notification_box->priv->notification,
                                        key,
                                        event->time);
}

static GtkWidget *
create_notification_action (NdNotificationBox *box,
                            NdNotification    *notification,
                            const char        *text,
                            const char        *key)
{
        GtkWidget *button;
        GtkWidget *hbox;
        GdkPixbuf *pixbuf;
        char      *buf;

        button = gtk_button_new ();
        gtk_widget_show (button);
        gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
        gtk_container_set_border_width (GTK_CONTAINER (button), 0);

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_show (hbox);
        gtk_container_add (GTK_CONTAINER (button), hbox);

        pixbuf = NULL;
        /* try to load an icon if requested */
        if (nd_notification_get_action_icons (box->priv->notification)) {
                pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (box))),
                                                   key,
                                                   20,
                                                   GTK_ICON_LOOKUP_USE_BUILTIN,
                                                   NULL);
        }

        if (pixbuf != NULL) {
                GtkWidget *image;

                image = gtk_image_new_from_pixbuf (pixbuf);
                g_object_unref (pixbuf);
                atk_object_set_name (gtk_widget_get_accessible (GTK_WIDGET (button)),
                                     text);
                gtk_widget_show (image);
                gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
                gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
                gtk_widget_set_valign (image, GTK_ALIGN_CENTER);
        } else {
                GtkWidget *label;

                label = gtk_label_new (NULL);
                gtk_widget_show (label);
                gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
                gtk_label_set_xalign (GTK_LABEL (label), 0.0);
                buf = g_strdup_printf ("<small>%s</small>", text);
                gtk_label_set_markup (GTK_LABEL (label), buf);
                g_free (buf);
        }

        g_object_set_data_full (G_OBJECT (button),
                                "_action_key", g_strdup (key), g_free);
        g_signal_connect (G_OBJECT (button),
                          "button-release-event",
                          G_CALLBACK (on_action_clicked),
                          box);
        return button;
}

static void
remove_item (GtkWidget *item,
             gpointer   data)
{
        gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (item)),
                              item);
}

static void
update_notification_box (NdNotificationBox *notification_box)
{
        gboolean       have_icon;
        gboolean       have_body;
        const char    *body;
        gboolean       have_actions;
        GIcon         *icon;
        char         **actions;
        int            i;
        char          *str;
        char          *quoted;
        GtkRequisition req;
        int            summary_width;

        /* Add content */

        have_icon = FALSE;
        have_body = FALSE;
        have_actions = FALSE;

        /* image */
        icon = nd_notification_get_icon (notification_box->priv->notification);
        if (icon != NULL) {
                gtk_image_set_from_gicon (GTK_IMAGE (notification_box->priv->icon), icon, GTK_ICON_SIZE_DIALOG);
                gtk_image_set_pixel_size (GTK_IMAGE (notification_box->priv->icon), IMAGE_SIZE);
                have_icon = TRUE;
        }

        /* summary */
        quoted = g_markup_escape_text (nd_notification_get_summary (notification_box->priv->notification), -1);
        str = g_strdup_printf ("<b><big>%s</big></b>", quoted);
        g_free (quoted);

        gtk_label_set_markup (GTK_LABEL (notification_box->priv->summary_label), str);
        g_free (str);

        gtk_widget_get_preferred_size (notification_box->priv->close_button, NULL, &req);
        /* -1: main_vbox border width
           -10: vbox border width
           -6: spacing for hbox */
        summary_width = WIDTH - (1*2) - (10*2) - BODY_X_OFFSET - req.width - (6*2);

        gtk_widget_set_size_request (notification_box->priv->summary_label,
                                     summary_width,
                                     -1);

        /* body */
        body = nd_notification_get_body (notification_box->priv->notification);
        if (validate_markup (body))
                gtk_label_set_markup (GTK_LABEL (notification_box->priv->body_label), body);
        else {
                gchar *tmp;

                tmp = g_markup_escape_text (body, -1);
                gtk_label_set_text (GTK_LABEL (notification_box->priv->body_label), body);
                g_free (tmp);
        }

        if (body != NULL && *body != '\0') {
                gtk_widget_set_size_request (notification_box->priv->body_label,
                                             summary_width,
                                             -1);
                have_body = TRUE;
        }

        /* actions */
        gtk_container_foreach (GTK_CONTAINER (notification_box->priv->actions_box), remove_item, NULL);
        actions = nd_notification_get_actions (notification_box->priv->notification);
        for (i = 0; actions[i] != NULL; i += 2) {
                char *l = actions[i + 1];

                if (l == NULL) {
                        g_warning ("Label not found for action %s. "
                                   "The protocol specifies that a label must "
                                   "follow an action in the actions array",
                                   actions[i]);

                        break;
                }

                if (strcasecmp (actions[i], "default") != 0) {
                        GtkWidget *button;

                        button = create_notification_action (notification_box,
                                                             notification_box->priv->notification,
                                                             l,
                                                             actions[i]);
                        gtk_box_pack_start (GTK_BOX (notification_box->priv->actions_box), button, FALSE, FALSE, 0);

                        have_actions = TRUE;
                }
        }

        if (have_icon || have_body || have_actions) {
                gtk_widget_show (notification_box->priv->content_hbox);
        } else {
                gtk_widget_hide (notification_box->priv->content_hbox);
        }
}

static void
nd_notification_box_init (NdNotificationBox *notification_box)
{
        GtkWidget     *box;
        GtkWidget     *image;
        GtkWidget     *vbox;
        AtkObject     *atkobj;

        notification_box->priv = nd_notification_box_get_instance_private (notification_box);
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_container_add (GTK_CONTAINER (notification_box), box);
        gtk_widget_show (box);

        /* Add icon */

        notification_box->priv->icon = gtk_image_new ();
        gtk_widget_set_valign (notification_box->priv->icon, GTK_ALIGN_START);
        gtk_widget_set_margin_top (notification_box->priv->icon, 5);
        gtk_widget_set_size_request (notification_box->priv->icon,
                                     BODY_X_OFFSET, -1);
        gtk_widget_show (notification_box->priv->icon);

        gtk_box_pack_start (GTK_BOX (box), notification_box->priv->icon,
                            FALSE, FALSE, 0);

        /* Add vbox */

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_show (vbox);
        gtk_box_pack_start (GTK_BOX (box), vbox, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);

        /* Add the close button */

        notification_box->priv->close_button = gtk_button_new ();
        gtk_widget_set_valign (notification_box->priv->close_button,
                               GTK_ALIGN_START);
        gtk_widget_show (notification_box->priv->close_button);

        gtk_box_pack_start (GTK_BOX (box), notification_box->priv->close_button,
                            FALSE, FALSE, 0);

        gtk_button_set_relief (GTK_BUTTON (notification_box->priv->close_button), GTK_RELIEF_NONE);
        gtk_container_set_border_width (GTK_CONTAINER (notification_box->priv->close_button), 0);
        g_signal_connect (G_OBJECT (notification_box->priv->close_button),
                          "clicked",
                          G_CALLBACK (on_close_button_clicked),
                          notification_box);

        atkobj = gtk_widget_get_accessible (notification_box->priv->close_button);
        atk_action_set_description (ATK_ACTION (atkobj), 0,
                                    _("Closes the notification."));
        atk_object_set_name (atkobj, "");
        atk_object_set_description (atkobj, _("Closes the notification."));

        image = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
        gtk_widget_show (image);
        gtk_container_add (GTK_CONTAINER (notification_box->priv->close_button), image);

        /* center vbox */
        notification_box->priv->summary_label = gtk_label_new (NULL);
        gtk_widget_show (notification_box->priv->summary_label);
        gtk_box_pack_start (GTK_BOX (vbox), notification_box->priv->summary_label, TRUE, TRUE, 0);
        gtk_label_set_xalign (GTK_LABEL (notification_box->priv->summary_label), 0.0);
        gtk_label_set_yalign (GTK_LABEL (notification_box->priv->summary_label), 0.0);
        gtk_label_set_line_wrap (GTK_LABEL (notification_box->priv->summary_label), TRUE);
        gtk_label_set_line_wrap_mode (GTK_LABEL (notification_box->priv->summary_label), PANGO_WRAP_WORD_CHAR);

        atkobj = gtk_widget_get_accessible (notification_box->priv->summary_label);
        atk_object_set_description (atkobj, _("Notification summary text."));

        notification_box->priv->content_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_show (notification_box->priv->content_hbox);
        gtk_box_pack_start (GTK_BOX (vbox), notification_box->priv->content_hbox, FALSE, FALSE, 0);

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

        gtk_widget_show (vbox);
        gtk_box_pack_start (GTK_BOX (notification_box->priv->content_hbox), vbox, TRUE, TRUE, 0);

        notification_box->priv->body_label = gtk_label_new (NULL);
        gtk_widget_show (notification_box->priv->body_label);
        gtk_box_pack_start (GTK_BOX (vbox), notification_box->priv->body_label, TRUE, TRUE, 0);
        gtk_label_set_xalign (GTK_LABEL (notification_box->priv->body_label), 0.0);
        gtk_label_set_yalign (GTK_LABEL (notification_box->priv->body_label), 0.0);
        gtk_label_set_line_wrap (GTK_LABEL (notification_box->priv->body_label), TRUE);
        gtk_label_set_line_wrap_mode (GTK_LABEL (notification_box->priv->body_label), PANGO_WRAP_WORD_CHAR);

        atkobj = gtk_widget_get_accessible (notification_box->priv->body_label);
        atk_object_set_description (atkobj, _("Notification body text."));

        notification_box->priv->actions_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_halign (notification_box->priv->actions_box, GTK_ALIGN_END);
        gtk_widget_show (notification_box->priv->actions_box);

        gtk_box_pack_start (GTK_BOX (vbox), notification_box->priv->actions_box, FALSE, TRUE, 0);
}

static void
on_notification_changed (NdNotification    *notification,
                         NdNotificationBox *notification_box)
{
        update_notification_box (notification_box);
}

static void
nd_notification_box_finalize (GObject *object)
{
        NdNotificationBox *notification_box;

        g_return_if_fail (object != NULL);
        g_return_if_fail (ND_IS_NOTIFICATION_BOX (object));

        notification_box = ND_NOTIFICATION_BOX (object);

        g_return_if_fail (notification_box->priv != NULL);

        g_signal_handlers_disconnect_by_func (notification_box->priv->notification, G_CALLBACK (on_notification_changed), notification_box);

        g_object_unref (notification_box->priv->notification);

        G_OBJECT_CLASS (nd_notification_box_parent_class)->finalize (object);
}

NdNotificationBox *
nd_notification_box_new_for_notification (NdNotification *notification)
{
        NdNotificationBox *notification_box;

        notification_box = g_object_new (ND_TYPE_NOTIFICATION_BOX,
                                         "visible-window", FALSE,
                                         NULL);
        notification_box->priv->notification = g_object_ref (notification);
        g_signal_connect (notification, "changed", G_CALLBACK (on_notification_changed), notification_box);
        update_notification_box (notification_box);

        return notification_box;
}
