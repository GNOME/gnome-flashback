/*
 * Copyright (C) 2010 Red Hat, Inc.
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

#include "config.h"

#include <string.h>
#include <strings.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "gf-bubble.h"
#include "nd-notification.h"

#define EXPIRATION_TIME_DEFAULT -1
#define EXPIRATION_TIME_NEVER_EXPIRES 0
#define TIMEOUT_SEC   5
#define WIDTH         400
#define IMAGE_SIZE    48
#define BODY_X_OFFSET (IMAGE_SIZE + 8)

typedef struct
{
  NdNotification *notification;

  GtkWidget      *icon;
  GtkWidget      *content_hbox;
  GtkWidget      *summary_label;
  GtkWidget      *close_button;
  GtkWidget      *body_label;
  GtkWidget      *actions_box;

  gboolean        url_clicked_lock;

  guint           timeout_id;
  gulong          changed_id;
  gulong          closed_id;
} GfBubblePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GfBubble, gf_bubble, GF_TYPE_POPUP_WINDOW)

static void
close_button_clicked_cb (GtkButton *button,
                         GfBubble  *bubble)
{
  GfBubblePrivate *priv;

  priv = gf_bubble_get_instance_private (bubble);

  nd_notification_close (priv->notification, ND_NOTIFICATION_CLOSED_USER);
  gtk_widget_destroy (GTK_WIDGET (bubble));
}

static gboolean
activate_link_cb (GtkLabel *label,
                  gchar    *uri,
                  GfBubble *bubble)
{
  GfBubblePrivate *priv;
  GError *error;

  priv = gf_bubble_get_instance_private (bubble);

  priv->url_clicked_lock = TRUE;

  error = NULL;
  if (!gtk_show_uri_on_window (GTK_WINDOW (bubble),
                               uri,
                               gtk_get_current_event_time (),
                               &error))
    {
      g_warning ("Could not show link: %s", error->message);
      g_error_free (error);
    }

  return TRUE;
}

static void
button_release_event_cb (GtkButton      *button,
                         GdkEventButton *event,
                         GfBubble       *bubble)
{
  GfBubblePrivate *priv;
  const gchar *key;
  gboolean transient;
  gboolean resident;

  priv = gf_bubble_get_instance_private (bubble);

  key = g_object_get_data (G_OBJECT (button), "_action_key");
  nd_notification_action_invoked (priv->notification, key, event->time);

  transient = nd_notification_get_is_transient (priv->notification);
  resident = nd_notification_get_is_resident (priv->notification);

  if (transient || !resident)
    gtk_widget_destroy (GTK_WIDGET (bubble));
}

static void
add_notification_action (GfBubble    *bubble,
                         const gchar *text,
                         const gchar *key)
{
  GfBubblePrivate *priv;
  GtkWidget *button;
  GtkWidget *hbox;
  GdkPixbuf *pixbuf;

  priv = gf_bubble_get_instance_private (bubble);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (priv->actions_box), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_container_set_border_width (GTK_CONTAINER (button), 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (button), hbox);
  gtk_widget_show (hbox);

  pixbuf = NULL;
  if (nd_notification_get_action_icons (priv->notification))
    pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), key, 20,
                                       GTK_ICON_LOOKUP_USE_BUILTIN, NULL);

  if (pixbuf != NULL)
    {
      GtkWidget *image;
      AtkObject *atkobj;

      image = gtk_image_new_from_pixbuf (pixbuf);
      gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
      gtk_widget_show (image);

      gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (image, GTK_ALIGN_CENTER);

      atkobj = gtk_widget_get_accessible (GTK_WIDGET (button));
      atk_object_set_name (atkobj, text);

      g_object_unref (pixbuf);
    }
  else
    {
      GtkWidget *label;
      gchar *buf;

      label = gtk_label_new (NULL);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      gtk_widget_show (label);

      gtk_label_set_xalign (GTK_LABEL (label), 0.0);

      buf = g_strdup_printf ("<small>%s</small>", text);
      gtk_label_set_markup (GTK_LABEL (label), buf);
      g_free (buf);
    }

  g_object_set_data_full (G_OBJECT (button), "_action_key",
                          g_strdup (key), g_free);

  g_signal_connect (button, "button-release-event",
                    G_CALLBACK (button_release_event_cb), bubble);

  gtk_widget_show (priv->actions_box);
}

static gboolean
timeout_bubble (gpointer user_data)
{
  GfBubble *bubble;
  GfBubblePrivate *priv;

  bubble = GF_BUBBLE (user_data);
  priv = gf_bubble_get_instance_private (bubble);
  priv->timeout_id = 0;

  gtk_widget_destroy (GTK_WIDGET (bubble));

  return G_SOURCE_REMOVE;
}

static void
add_timeout (GfBubble *bubble)
{
  GfBubblePrivate *priv;
  gint timeout;

  priv = gf_bubble_get_instance_private (bubble);

  if (priv->timeout_id != 0)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }

  timeout = nd_notification_get_timeout (priv->notification);

  if (timeout == EXPIRATION_TIME_NEVER_EXPIRES)
    return;

  if (timeout == EXPIRATION_TIME_DEFAULT)
    timeout = TIMEOUT_SEC * 1000;

  priv->timeout_id = g_timeout_add (timeout, timeout_bubble, bubble);
}

static void
destroy_widget (GtkWidget *widget,
                gpointer   user_data)
{
  gtk_widget_destroy (widget);
}

static void
update_bubble (GfBubble *bubble)
{
  GfBubblePrivate *priv;
  const gchar *summary;
  const gchar *body;
  gchar *quoted;
  gchar *str;
  gboolean have_body;
  gchar **actions;
  gint i;
  gboolean have_actions;
  GIcon *icon;
  gboolean have_icon;

  priv = gf_bubble_get_instance_private (bubble);

  gtk_widget_show_all (GTK_WIDGET (bubble));

  /* Summary label */

  summary = nd_notification_get_summary (priv->notification);

  quoted = g_markup_escape_text (summary, -1);
  str = g_strdup_printf ("<b><big>%s</big></b>", quoted);
  g_free (quoted);

  gtk_label_set_markup (GTK_LABEL (priv->summary_label), str);
  g_free (str);

  /* Body label */

  body = nd_notification_get_body (priv->notification);

  if (validate_markup (body))
    {
      gtk_label_set_markup (GTK_LABEL (priv->body_label), body);
    }
  else
    {
      str = g_markup_escape_text (body, -1);

      gtk_label_set_text (GTK_LABEL (priv->body_label), str);
      g_free (str);
    }

  have_body = body && *body != '\0';
  gtk_widget_set_visible (priv->body_label, have_body);

  /* Actions */

  have_actions = FALSE;
  gtk_container_foreach (GTK_CONTAINER (priv->actions_box),
                         destroy_widget, NULL);

  actions = nd_notification_get_actions (priv->notification);

  for (i = 0; actions[i] != NULL; i += 2)
    {
      gchar *l;

      l = actions[i + 1];

      if (l == NULL)
        {
          g_warning ("Label not found for action - %s. The protocol specifies "
                     "that a label must follow an action in the actions array",
                     actions[i]);
        }
      else if (strcasecmp (actions[i], "default") != 0)
        {
          have_actions = TRUE;

          add_notification_action (bubble, l, actions[i]);
        }
    }

  gtk_widget_set_visible (priv->actions_box, have_actions);

  /* Icon */

  icon = nd_notification_get_icon (priv->notification);
  have_icon = FALSE;

  if (icon != NULL)
    {
      gtk_image_set_from_gicon (GTK_IMAGE (priv->icon), icon, GTK_ICON_SIZE_DIALOG);
      gtk_image_set_pixel_size (GTK_IMAGE (priv->icon), IMAGE_SIZE);

      have_icon = TRUE;
      gtk_widget_set_visible (priv->icon, have_icon);
      gtk_widget_set_size_request (priv->icon, BODY_X_OFFSET, -1);
    }

  if (have_body || have_actions || have_icon)
    gtk_widget_show (priv->content_hbox);
  else
    gtk_widget_hide (priv->content_hbox);

  add_timeout (bubble);
}

static void
notification_changed_cb (NdNotification *notification,
                         GfBubble       *bubble)
{
  update_bubble (bubble);
}

static void
notification_closed_cb (NdNotification *notification,
                        gint            reason,
                        GfBubble       *bubble)
{
  if (reason == ND_NOTIFICATION_CLOSED_API)
    gtk_widget_destroy (GTK_WIDGET (bubble));
}

static void
gf_bubble_dispose (GObject *object)
{
  GfBubble *bubble;
  GfBubblePrivate *priv;

  bubble = GF_BUBBLE (object);
  priv = gf_bubble_get_instance_private (bubble);

  if (priv->timeout_id != 0)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }

  if (priv->changed_id != 0)
    {
      g_signal_handler_disconnect (priv->notification, priv->changed_id);
      priv->changed_id = 0;
    }

  if (priv->closed_id != 0)
    {
      g_signal_handler_disconnect (priv->notification, priv->closed_id);
      priv->closed_id = 0;
    }

  G_OBJECT_CLASS (gf_bubble_parent_class)->dispose (object);
}

static void
gf_bubble_finalize (GObject *object)
{
  GfBubble *bubble;
  GfBubblePrivate *priv;

  bubble = GF_BUBBLE (object);
  priv = gf_bubble_get_instance_private (bubble);

  g_clear_object (&priv->notification);

  G_OBJECT_CLASS (gf_bubble_parent_class)->finalize (object);
}

static gboolean
gf_bubble_button_release_event (GtkWidget      *widget,
                                GdkEventButton *event)
{
  GfBubble *bubble;
  GfBubblePrivate *priv;
  gboolean retval;

  bubble = GF_BUBBLE (widget);
  priv = gf_bubble_get_instance_private (bubble);

  retval = GTK_WIDGET_CLASS (gf_bubble_parent_class)->button_release_event (widget,
                                                                            event);

  if (priv->url_clicked_lock)
    {
      priv->url_clicked_lock = FALSE;
      return retval;
    }

  nd_notification_action_invoked (priv->notification, "default", event->time);
  gtk_widget_destroy (widget);

  return retval;
}

static void
gf_bubble_get_preferred_width (GtkWidget *widget,
                               gint      *min_width,
                               gint      *nat_width)
{
  GTK_WIDGET_CLASS (gf_bubble_parent_class)->get_preferred_width (widget,
                                                                  min_width,
                                                                  nat_width);

  *nat_width = WIDTH;
}

static gboolean
gf_bubble_motion_notify_event (GtkWidget      *widget,
                               GdkEventMotion *event)
{
  GfBubble *bubble;
  gboolean retval;

  bubble = GF_BUBBLE (widget);

  retval = GTK_WIDGET_CLASS (gf_bubble_parent_class)->motion_notify_event (widget,
                                                                           event);

  add_timeout (bubble);

  return retval;
}

static void
gf_bubble_realize (GtkWidget *widget)
{
  GfBubble *bubble;

  bubble = GF_BUBBLE (widget);

  GTK_WIDGET_CLASS (gf_bubble_parent_class)->realize (widget);

  add_timeout (bubble);
}

static void
gf_bubble_class_init (GfBubbleClass *bubble_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (bubble_class);
  widget_class = GTK_WIDGET_CLASS (bubble_class);

  object_class->dispose = gf_bubble_dispose;
  object_class->finalize = gf_bubble_finalize;

  widget_class->button_release_event = gf_bubble_button_release_event;
  widget_class->get_preferred_width = gf_bubble_get_preferred_width;
  widget_class->motion_notify_event = gf_bubble_motion_notify_event;
  widget_class->realize = gf_bubble_realize;
}

static void
gf_bubble_init (GfBubble *bubble)
{
  GfBubblePrivate *priv;
  GtkWidget *widget;
  AtkObject *atkobj;
  gint events;
  GtkWidget *main_vbox;
  GtkWidget *main_hbox;
  GtkWidget *vbox;
  GtkWidget *image;

  priv = gf_bubble_get_instance_private (bubble);

  widget = GTK_WIDGET (bubble);
  atkobj = gtk_widget_get_accessible (widget);

  atk_object_set_role (atkobj, ATK_ROLE_ALERT);

  events = GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
           GDK_POINTER_MOTION_MASK;

  gtk_widget_add_events (widget, events);
  gtk_widget_set_name (widget, "gf-bubble");

  /* Main vbox */

  main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (bubble), main_vbox);
  gtk_widget_show (main_vbox);

  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);

  /* Main hbox */

  main_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox), main_hbox, FALSE, FALSE, 0);
  gtk_widget_show (main_hbox);

  /* Icon */

  priv->icon = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (main_hbox), priv->icon, FALSE, FALSE, 0);
  gtk_widget_show (priv->icon);

  gtk_widget_set_margin_top (priv->icon, 5);
  gtk_widget_set_size_request (priv->icon, BODY_X_OFFSET, -1);
  gtk_widget_set_valign (priv->icon, GTK_ALIGN_START);

  /* Vbox */

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (main_hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);

  /* Close button */

  priv->close_button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (main_hbox), priv->close_button, FALSE, FALSE, 0);
  gtk_widget_show (priv->close_button);

  gtk_button_set_relief (GTK_BUTTON (priv->close_button), GTK_RELIEF_NONE);
  gtk_widget_set_valign (priv->close_button, GTK_ALIGN_START);

  g_signal_connect (priv->close_button, "clicked",
                    G_CALLBACK (close_button_clicked_cb), bubble);

  atkobj = gtk_widget_get_accessible (priv->close_button);
  atk_object_set_description (atkobj, _("Closes the notification."));
  atk_object_set_name (atkobj, "");

  atk_action_set_description (ATK_ACTION (atkobj), 0,
                              _("Closes the notification."));

  image = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (priv->close_button), image);
  gtk_widget_show (image);

  /* Summary label */

  priv->summary_label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbox), priv->summary_label, TRUE, TRUE, 0);
  gtk_widget_show (priv->summary_label);

  gtk_label_set_line_wrap (GTK_LABEL (priv->summary_label), TRUE);
  gtk_label_set_line_wrap_mode (GTK_LABEL (priv->summary_label),
                                PANGO_WRAP_WORD_CHAR);

  gtk_label_set_xalign (GTK_LABEL (priv->summary_label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (priv->summary_label), 0.0);

  atkobj = gtk_widget_get_accessible (priv->summary_label);
  atk_object_set_description (atkobj, _("Notification summary text."));

  /* Content hbox */

  priv->content_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), priv->content_hbox, FALSE, FALSE, 0);
  gtk_widget_show (priv->content_hbox);

  /* Vbox */

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (priv->content_hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  /* Body label */

  priv->body_label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbox), priv->body_label, TRUE, TRUE, 0);
  gtk_widget_show (priv->body_label);

  gtk_label_set_line_wrap (GTK_LABEL (priv->body_label), TRUE);
  gtk_label_set_line_wrap_mode (GTK_LABEL (priv->body_label),
                                PANGO_WRAP_WORD_CHAR);

  gtk_label_set_xalign (GTK_LABEL (priv->body_label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (priv->body_label), 0.0);

  g_signal_connect (priv->body_label, "activate-link",
                    G_CALLBACK (activate_link_cb), bubble);

  atkobj = gtk_widget_get_accessible (priv->body_label);
  atk_object_set_description (atkobj, _("Notification summary text."));

  /* Actions box */

  priv->actions_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), priv->actions_box, FALSE, TRUE, 0);
  gtk_widget_show (priv->actions_box);

  gtk_widget_set_halign (priv->actions_box, GTK_ALIGN_END);
}

GfBubble *
gf_bubble_new_for_notification (NdNotification *notification)
{
  GfBubble *bubble;
  GfBubblePrivate *priv;

  bubble = g_object_new (GF_TYPE_BUBBLE,
                         "resizable", FALSE,
                         "title", _("Notification"),
                         "type", GTK_WINDOW_POPUP,
                         NULL);

  priv = gf_bubble_get_instance_private (bubble);

  priv->notification = g_object_ref (notification);

  priv->changed_id = g_signal_connect (notification, "changed",
                                       G_CALLBACK (notification_changed_cb),
                                       bubble);

  priv->closed_id = g_signal_connect (notification, "closed",
                                      G_CALLBACK (notification_closed_cb),
                                      bubble);

  update_bubble (bubble);

  return bubble;
}

NdNotification *
gf_bubble_get_notification (GfBubble *bubble)
{
  GfBubblePrivate *priv;

  g_return_val_if_fail (GF_IS_BUBBLE (bubble), NULL);

  priv = gf_bubble_get_instance_private (bubble);

  return priv->notification;
}
