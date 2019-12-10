/*
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
#include "gf-panel.h"

#include <libgnome-desktop/gnome-wall-clock.h>

struct _GfPanel
{
  GtkBox          parent;

  GnomeWallClock *clock;

  GtkWidget      *clock_label;
  GtkWidget      *name_label;
};

G_DEFINE_TYPE (GfPanel, gf_panel, GTK_TYPE_BOX)

static char *
get_user_display_name (void)
{
  const char *name;
  char *display_name;

  name = g_get_real_name ();
  if (name == NULL || name[0] == '\0' || g_strcmp0 (name, "Unknown") == 0)
    name = g_get_user_name ();

  display_name = NULL;
  if (name != NULL)
    display_name = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);

  return display_name;
}

static void
update_name (GfPanel *self)
{
  char *name;

  name = get_user_display_name ();
  gtk_label_set_label (GTK_LABEL (self->name_label), name);
  g_free (name);
}

static void
update_clock (GfPanel *self)
{
  const char *string;

  string = gnome_wall_clock_get_clock (self->clock);
  gtk_label_set_label (GTK_LABEL (self->clock_label), string);
}

static void
clock_changed_cb (GnomeWallClock *clock,
                  GParamSpec     *pspec,
                  GfPanel        *self)
{
  update_clock (self);
}

static void
gf_panel_dispose (GObject *object)
{
  GfPanel *self;

  self = GF_PANEL (object);

  g_clear_object (&self->clock);

  G_OBJECT_CLASS (gf_panel_parent_class)->dispose (object);
}

static void
gf_panel_class_init (GfPanelClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);

  object_class->dispose = gf_panel_dispose;

  gtk_widget_class_set_css_name (widget_class, "gf-screensaver-panel");
}

static void
gf_panel_init (GfPanel *self)
{
  GtkWidget *left_hbox;
  GtkWidget *right_hbox;
  GIcon *icon;
  GtkWidget *image;

  self->clock = gnome_wall_clock_new ();

  g_signal_connect (self->clock, "notify::clock",
                    G_CALLBACK (clock_changed_cb),
                    self);

  left_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (self), left_hbox, TRUE, TRUE, 0);
  gtk_widget_show (left_hbox);

  self->clock_label = gtk_label_new (NULL);
  gtk_box_set_center_widget (GTK_BOX (self), self->clock_label);
  gtk_widget_show (self->clock_label);

  right_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_end (GTK_BOX (self), right_hbox, TRUE, TRUE, 0);
  gtk_widget_show (right_hbox);

  self->name_label = gtk_label_new (NULL);
  gtk_box_pack_end (GTK_BOX (right_hbox), self->name_label, FALSE, FALSE, 0);
  gtk_widget_show (self->name_label);

  icon = g_themed_icon_new_with_default_fallbacks ("changes-prevent-symbolic");
  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
  g_object_unref (icon);

  gtk_box_pack_end (GTK_BOX (right_hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  update_clock (self);
  update_name (self);
}

GtkWidget *
gf_panel_new (void)
{
  return g_object_new (GF_TYPE_PANEL,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "spacing", 12,
                       NULL);
}
