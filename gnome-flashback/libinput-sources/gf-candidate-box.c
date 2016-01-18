/*
 * Copyright (C) 2016 Sebastian Geiger
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

#include "gf-candidate-box.h"

struct _GfCandidateBox
{
  GtkEventBox  parent;

  GtkWidget   *index_label;
  GtkWidget   *candidate_label;

  guint        index;
};

enum
{
  PROP_0,

  PROP_INDEX,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfCandidateBox, gf_candidate_box, GTK_TYPE_EVENT_BOX)

static void
gf_candidate_box_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GfCandidateBox *box;

  box = GF_CANDIDATE_BOX (object);

  switch (property_id)
    {
      case PROP_INDEX:
        box->index = g_value_get_uint (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_candidate_box_class_init (GfCandidateBoxClass *box_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (box_class);

  object_class->set_property = gf_candidate_box_set_property;

  properties[PROP_INDEX] =
    g_param_spec_uint ("index", "index", "index", 0, G_MAXUINT, 0,
                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                      G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gf_candidate_box_init (GfCandidateBox *box)
{
  GtkWidget *hbox;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (box), hbox);
  gtk_widget_show (hbox);

  box->index_label = gtk_label_new (NULL);
  gtk_container_add (GTK_CONTAINER (hbox), box->index_label);
  gtk_widget_show (box->index_label);

  box->candidate_label = gtk_label_new (NULL);
  gtk_container_add (GTK_CONTAINER (hbox), box->candidate_label);
  gtk_widget_show (box->candidate_label);
}

GtkWidget*
gf_candidate_box_new (guint index)
{
  return g_object_new (GF_TYPE_CANDIDATE_BOX, "index", index, NULL);
}

void
gf_candidate_box_set_labels (GfCandidateBox *box,
                             const gchar    *index_label,
                             const gchar    *candidate_label)
{
  gtk_label_set_text (GTK_LABEL (box->index_label), index_label);
  gtk_label_set_text (GTK_LABEL (box->candidate_label), candidate_label);
}

guint
gf_candidate_box_get_index (GfCandidateBox *box)
{
  return box->index;
}
