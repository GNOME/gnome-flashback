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

#include <ibus-1.0/ibus.h>

#include "gf-candidate-area.h"
#include "gf-candidate-box.h"

#define MAX_CANDIDATES_PER_PAGE 16

const gchar* DEFAULT_INDEX_LABELS[] =
  {
    "1", "2", "3", "4", "5", "6", "7", "8",
    "9", "0", "a", "b", "c", "d", "e", "f"
  };

struct _GfCandidateArea
{
  GtkBox     parent;

  GtkWidget *button_box;
  GtkWidget *prev_button;
  GtkWidget *next_button;

  GtkWidget *candidate_box;
  GSList    *candidate_boxes;

  gint       orientation;
};

enum
{
  CANDIDATE_CLICKED,
  PREV_PAGE,
  NEXT_PAGE,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfCandidateArea, gf_candidate_area, GTK_TYPE_BOX)

static gboolean
button_clicked_cb (GtkWidget       *widget,
                   GdkEvent        *event,
                   GfCandidateArea *area)
{
  guint index;

  index = gf_candidate_box_get_index (GF_CANDIDATE_BOX (widget));

  g_signal_emit (area, signals[CANDIDATE_CLICKED], 0, index, event);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
prev_button_clicked_cb (GtkButton       *button,
                        GfCandidateArea *area)
{
  g_signal_emit (area, signals[PREV_PAGE], 0);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
next_button_clicked_cb (GtkButton       *button,
                        GfCandidateArea *area)
{
  g_signal_emit (area, signals[NEXT_PAGE], 0);

  return GDK_EVENT_PROPAGATE;
}

static void
gf_candidate_area_finalize (GObject *object)
{
  GfCandidateArea *area;

  area = GF_CANDIDATE_AREA (object);

  g_slist_free (area->candidate_boxes);

  G_OBJECT_CLASS (gf_candidate_area_parent_class)->finalize (object);
}

static void
gf_candidate_area_class_init (GfCandidateAreaClass *area_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (area_class);

  object_class->finalize = gf_candidate_area_finalize;

  signals[CANDIDATE_CLICKED] =
    g_signal_new ("candidate-clicked", GF_TYPE_CANDIDATE_AREA,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE,
                  2, G_TYPE_UINT, GDK_TYPE_EVENT);

  signals[PREV_PAGE] =
    g_signal_new ("previous-page", GF_TYPE_CANDIDATE_AREA,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals[NEXT_PAGE] =
    g_signal_new ("next-page", GF_TYPE_CANDIDATE_AREA,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gf_candidate_area_init (GfCandidateArea *area)
{
  guint i;
  GtkIconSize size;
  GtkWidget *image;

  area->candidate_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_container_add (GTK_CONTAINER (area), area->candidate_box);
  gtk_widget_show (area->candidate_box);

  for (i = 0; i < MAX_CANDIDATES_PER_PAGE; i++)
    {
      GtkWidget *box;

      box = gf_candidate_box_new (i);
      gtk_container_add (GTK_CONTAINER (area->candidate_box), box);
      gtk_widget_show (box);

      area->candidate_boxes = g_slist_append (area->candidate_boxes, box);

      g_signal_connect (box, "button-release-event",
                        G_CALLBACK (button_clicked_cb), area);
    }

  area->button_box = gtk_button_box_new (0);
  gtk_container_add (GTK_CONTAINER (area), area->button_box);
  gtk_widget_show (area->button_box);

  gtk_button_box_set_layout (GTK_BUTTON_BOX (area->button_box), GTK_BUTTONBOX_EXPAND);

  size = GTK_ICON_SIZE_BUTTON;

  area->prev_button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (area->button_box), area->prev_button);
  gtk_widget_show (area->prev_button);

  image = gtk_image_new_from_icon_name ("go-previous-symbolic", size);
  gtk_button_set_image (GTK_BUTTON (area->prev_button), image);

  g_signal_connect (area->prev_button, "clicked",
                    G_CALLBACK (prev_button_clicked_cb), area);

  area->next_button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (area->button_box), area->next_button);
  gtk_widget_show (area->next_button);

  image = gtk_image_new_from_icon_name ("go-next-symbolic", size);
  gtk_button_set_image (GTK_BUTTON (area->next_button), image);

  g_signal_connect (area->next_button, "clicked",
                    G_CALLBACK (next_button_clicked_cb), area);

  area->orientation = IBUS_ORIENTATION_HORIZONTAL;
}

GtkWidget *
gf_candidate_area_new (void)
{
  return g_object_new (GF_TYPE_CANDIDATE_AREA,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "spacing", 6,
                       NULL);
}

void
gf_candidate_area_set_orientation (GfCandidateArea *area,
                                   gint             orientation)
{
  GtkIconSize size;
  GtkWidget *image;

  if (area->orientation == orientation)
    return;

  area->orientation = orientation;
  size = GTK_ICON_SIZE_BUTTON;

  if (area->orientation == IBUS_ORIENTATION_HORIZONTAL)
    {
      gtk_orientable_set_orientation (GTK_ORIENTABLE (area),
                                      GTK_ORIENTATION_HORIZONTAL);

      gtk_orientable_set_orientation (GTK_ORIENTABLE (area->candidate_box),
                                      GTK_ORIENTATION_HORIZONTAL);

      image = gtk_image_new_from_icon_name ("go-previous-symbolic", size);
      gtk_button_set_image (GTK_BUTTON (area->prev_button), image);

      image = gtk_image_new_from_icon_name ("go-next-symbolic", size);
      gtk_button_set_image (GTK_BUTTON (area->next_button), image);
    }
  else
    {
      gtk_orientable_set_orientation (GTK_ORIENTABLE (area),
                                      GTK_ORIENTATION_VERTICAL);

      gtk_orientable_set_orientation (GTK_ORIENTABLE (area->candidate_box),
                                      GTK_ORIENTATION_VERTICAL);

      image = gtk_image_new_from_icon_name ("go-up-symbolic", size);
      gtk_button_set_image (GTK_BUTTON (area->prev_button), image);

      image = gtk_image_new_from_icon_name ("go-down-symbolic", size);
      gtk_button_set_image (GTK_BUTTON (area->next_button), image);
    }
}

void
gf_candidate_area_set_candidates (GfCandidateArea *area,
                                  GSList          *indexes,
                                  GSList          *candidates,
                                  guint            cursor_position,
                                  gboolean         cursor_visible)
{
  guint i;

  for (i = 0; i < MAX_CANDIDATES_PER_PAGE; i++)
    {
      GtkWidget *candidate_box;
      gboolean visible;
      const gchar *index_text;
      const gchar *candidate_text;

      candidate_box = g_slist_nth_data (area->candidate_boxes, i);
      visible = i < g_slist_length (candidates);

      gtk_widget_set_visible (candidate_box, visible);

      if (!visible)
        continue;

      if (indexes && g_slist_nth_data (indexes, i))
          index_text = g_slist_nth_data (indexes, i);
      else
          index_text = DEFAULT_INDEX_LABELS[i];

      candidate_text = g_slist_nth_data (candidates, i);

      gf_candidate_box_set_labels (GF_CANDIDATE_BOX (candidate_box),
                                   index_text, candidate_text);

      gf_candidate_box_set_selected (GF_CANDIDATE_BOX (candidate_box),
                                     cursor_visible && i == cursor_position);
    }
}

void
gf_candidate_area_update_buttons (GfCandidateArea *area,
                                  gboolean         wraps_around,
                                  gint             page,
                                  gint             n_pages)
{
  gboolean sensitive;

  gtk_widget_set_visible (area->button_box, n_pages > 1);

  if (n_pages < 2)
    return;

  sensitive = wraps_around || page > 0;
  gtk_widget_set_sensitive (area->prev_button, sensitive);

  sensitive = wraps_around || page < n_pages - 1;
  gtk_widget_set_sensitive (area->next_button, sensitive);
}
