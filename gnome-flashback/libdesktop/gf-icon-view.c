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
#include "gf-icon-view.h"

struct _GfIconView
{
  GtkEventBox  parent;

  GtkWidget   *fixed;
};

G_DEFINE_TYPE (GfIconView, gf_icon_view, GTK_TYPE_EVENT_BOX)

static void
gf_icon_view_class_init (GfIconViewClass *self_class)
{
}

static void
gf_icon_view_init (GfIconView *self)
{
  self->fixed = gtk_fixed_new ();
  gtk_container_add (GTK_CONTAINER (self), self->fixed);
  gtk_widget_show (self->fixed);
}

GtkWidget *
gf_icon_view_new (void)
{
  return g_object_new (GF_TYPE_ICON_VIEW, NULL);
}
