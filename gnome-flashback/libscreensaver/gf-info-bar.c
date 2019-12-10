/*
 * Copyright (C) 2004-2008 William Jon McCann
 * Copyright (C) 2008-2011 Red Hat, Inc
 * Copyright (C) 2019 Alberts Muktupāvels
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
#include "gf-info-bar.h"

#define INFO_BAR_SECONDS 30

struct _GfInfoBar
{
  GtkInfoBar parent;

  GtkWidget *content_box;

  guint      timeout_id;
};

G_DEFINE_TYPE (GfInfoBar, gf_info_bar, GTK_TYPE_INFO_BAR)

static void
update_content (GfInfoBar  *self,
                const char *summary,
                const char *body,
                const char *icon)
{
  GtkWidget *content_area;
  GtkWidget *image;
  GtkWidget *vbox;
  char *markup;
  GtkWidget *label;

  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (self));

  if (self->content_box != NULL)
    gtk_container_remove (GTK_CONTAINER (content_area), self->content_box);

  self->content_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (content_area), self->content_box, TRUE, FALSE, 0);
  gtk_widget_show (self->content_box);

  image = gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (self->content_box), image, FALSE, FALSE, 0);
  gtk_widget_set_valign (image, GTK_ALIGN_START);
  gtk_widget_show (image);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (self->content_box), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  markup = g_strdup_printf ("<b>%s</b>", summary);
  label = gtk_label_new (markup);
  g_free (markup);

  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  if (body != NULL)
    {
      markup = g_strdup_printf ("<small>%s</small>", body);
      label = gtk_label_new (markup);
      g_free (markup);

      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
      gtk_widget_show (label);
    }
}

static gboolean
message_timeout_cb (gpointer user_data)
{
  GfInfoBar *self;

  self = GF_INFO_BAR (user_data);
  self->timeout_id = 0;

  gtk_widget_hide (GTK_WIDGET (self));

  return G_SOURCE_REMOVE;
}

static void
gf_info_bar_dispose (GObject *object)
{
  GfInfoBar *self;

  self = GF_INFO_BAR (object);

  if (self->timeout_id != 0)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
    }

  G_OBJECT_CLASS (gf_info_bar_parent_class)->dispose (object);
}

static void
gf_info_bar_class_init (GfInfoBarClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_info_bar_dispose;
}

static void
gf_info_bar_init (GfInfoBar *self)
{
}

GtkWidget *
gf_info_bar_new (void)
{
  return g_object_new (GF_TYPE_INFO_BAR,
                       NULL);
}

void
gf_info_bar_show_message (GfInfoBar  *self,
                          const char *summary,
                          const char *body,
                          const char *icon)
{
  update_content (self, summary, body, icon);
  gtk_widget_show (GTK_WIDGET (self));

  if (self->timeout_id != 0)
    g_source_remove (self->timeout_id);

  self->timeout_id = g_timeout_add_seconds (INFO_BAR_SECONDS,
                                            message_timeout_cb,
                                            self);
}
