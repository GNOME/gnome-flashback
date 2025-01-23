/*
 * Copyright (C) 2015 Alberts Muktupāvels
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

#include <math.h>

#include "gf-candidate-area.h"
#include "gf-candidate-popup.h"

struct _GfCandidatePopup
{
  GfPopupWindow     parent;

  IBusPanelService *service;

  GtkWidget        *pre_edit_text;
  GtkWidget        *aux_text;
  GtkWidget        *candidate_area;

  GdkRectangle      cursor;
};

G_DEFINE_TYPE (GfCandidatePopup, gf_candidate_popup, GF_TYPE_POPUP_WINDOW)

static void
update_size_and_position (GfCandidatePopup *popup)
{
  GtkWidget *widget;
  GtkWindow *window;
  GtkRequisition size;
  GdkRectangle *cursor;
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkRectangle rect;
  gint width;
  gint height;
  gint x;
  gint y;

  widget = GTK_WIDGET (popup);
  window = GTK_WINDOW (popup);

  gtk_widget_get_preferred_size (widget, NULL, &size);
  gtk_window_resize (window, size.width, size.height);

  cursor = &popup->cursor;

  display = gdk_display_get_default ();
  monitor = gdk_display_get_monitor_at_point (display, cursor->x, cursor->y);

  gdk_monitor_get_geometry (monitor, &rect);
  gtk_window_get_size (window, &width, &height);

  x = cursor->x;
  if (x + width > rect.width)
    x = x - ((x + width) - rect.width);

  if (cursor->y + height + cursor->height > rect.height)
    y = cursor->y - height;
  else
    y = cursor->y + cursor->height;

  gtk_window_move (window, x, y);
}

static void
set_cursor_location_cb (IBusPanelService *service,
                        gint              x,
                        gint              y,
                        gint              w,
                        gint              h,
                        GfCandidatePopup *popup)
{
  popup->cursor.x = x;
  popup->cursor.y = y;
  popup->cursor.width = w;
  popup->cursor.height = h;

  update_size_and_position (popup);
}

static void
update_preedit_text_cb (IBusPanelService *service,
                        IBusText         *text,
                        guint             cursor_pos,
                        gboolean          visible,
                        GfCandidatePopup *popup)
{
  const gchar *preedit_text;
  IBusAttrList *attributes;
  IBusAttribute *attribute;
  guint i;

  gtk_widget_set_visible (popup->pre_edit_text, visible);

  preedit_text = ibus_text_get_text (text);
  gtk_label_set_text (GTK_LABEL (popup->pre_edit_text), preedit_text);

  attributes = ibus_text_get_attributes (text);

  if (attributes == NULL)
    return;

  for (i = 0; (attribute = ibus_attr_list_get (attributes, i)) != NULL; i++)
    {
      guint start;
      guint end;

      if (ibus_attribute_get_attr_type (attribute) != IBUS_ATTR_TYPE_BACKGROUND)
        continue;

      start = ibus_attribute_get_start_index (attribute);
      end = ibus_attribute_get_end_index (attribute);

      gtk_label_select_region (GTK_LABEL (popup->pre_edit_text), start, end);
    }

  update_size_and_position (popup);
}

static void
show_preedit_text_cb (IBusPanelService *service,
                      GfCandidatePopup *popup)
{
  gtk_widget_show (popup->pre_edit_text);
}

static void
hide_preedit_text_cb (IBusPanelService *service,
                      GfCandidatePopup *popup)
{
  gtk_widget_hide (popup->pre_edit_text);
}

static void
update_auxiliary_text_cb (IBusPanelService *service,
                          IBusText         *text,
                          gboolean          visible,
                          GfCandidatePopup *popup)
{
  const gchar *auxiliary_text;

  gtk_widget_set_visible (popup->aux_text, visible);

  auxiliary_text = ibus_text_get_text (text);
  gtk_label_set_text (GTK_LABEL (popup->aux_text), auxiliary_text);

  update_size_and_position (popup);
}

static void
show_auxiliary_text_cb (IBusPanelService *service,
                        GfCandidatePopup *popup)
{
  gtk_widget_show (popup->aux_text);
}

static void
hide_auxiliary_text_cb (IBusPanelService *service,
                        GfCandidatePopup *popup)
{
  gtk_widget_hide (popup->aux_text);
}

static void
update_lookup_table_cb (IBusPanelService *service,
                        IBusLookupTable  *lookup_table,
                        gboolean          visible,
                        GfCandidatePopup *popup)
{
  guint n_candidates;
  guint cursor_position;
  guint page_size;
  guint n_pages;
  guint page;
  guint start_index;
  guint end_index;
  guint i;
  GSList *indexes;
  GSList *candidates;
  GfCandidateArea *area;
  gint orientation;
  gboolean is_round;

  gtk_widget_set_visible (GTK_WIDGET (popup), visible);

  n_candidates = ibus_lookup_table_get_number_of_candidates (lookup_table);
  cursor_position = ibus_lookup_table_get_cursor_pos (lookup_table);
  page_size = ibus_lookup_table_get_page_size (lookup_table);

  n_pages = (guint) ceil ((gdouble) n_candidates / page_size);

  page = 0;
  if (cursor_position != 0)
    page = (guint) floor ((gdouble) cursor_position / page_size);

  start_index = page * page_size;
  end_index = MIN ((page + 1) * page_size, n_candidates);

  i = 0;
  indexes = NULL;
  candidates = NULL;

  while (TRUE)
    {
      IBusText *ibus_text;
      const gchar *text;

      ibus_text = ibus_lookup_table_get_label (lookup_table, i++);

      if (ibus_text == NULL)
        break;

      text = ibus_text_get_text (ibus_text);

      indexes = g_slist_append (indexes, g_strdup (text));
    }

  for (i = start_index; i < end_index; i++)
    {
      IBusText *ibus_text;
      const gchar *text;

      ibus_text = ibus_lookup_table_get_candidate (lookup_table, i);
      text = ibus_text_get_text (ibus_text);

      candidates = g_slist_append (candidates, g_strdup (text));
    }

  area = GF_CANDIDATE_AREA (popup->candidate_area);

  gf_candidate_area_set_candidates (area, indexes, candidates,
                                    cursor_position % page_size, visible);

  g_slist_free_full (indexes, g_free);
  g_slist_free_full (candidates, g_free);

  orientation = ibus_lookup_table_get_orientation (lookup_table);
  gf_candidate_area_set_orientation (area, orientation);

  is_round = ibus_lookup_table_is_round (lookup_table);
  gf_candidate_area_update_buttons (area, is_round, page, n_pages);

  update_size_and_position (popup);
}

static void
show_lookup_table_cb (IBusPanelService *service,
                      GfCandidatePopup *popup)
{
  gtk_widget_show (GTK_WIDGET (popup));
}

static void
hide_lookup_table_cb (IBusPanelService *service,
                      GfCandidatePopup *popup)
{
  gtk_widget_hide (GTK_WIDGET (popup));
}

static void
focus_out_cb (IBusPanelService *service,
              const gchar      *input_context_path,
              gpointer          user_data)
{
}

static void
previous_page_cb (GfCandidateArea  *area,
                  GfCandidatePopup *popup)
{
  ibus_panel_service_page_up (popup->service);
}

static void
next_page_cb (GfCandidateArea  *area,
              GfCandidatePopup *popup)
{
  ibus_panel_service_page_down (popup->service);
}

static void
candidate_clicked_cb (GfCandidateArea  *area,
                      guint             index,
                      GdkEvent         *event,
                      GfCandidatePopup *popup)
{
  guint button;
  GdkModifierType state;

  gdk_event_get_button (event, &button);
  gdk_event_get_state (event, &state);

  ibus_panel_service_candidate_clicked (popup->service, index, button, state);
}

static void
gf_candidate_popup_dispose (GObject *object)
{
  GfCandidatePopup *popup;

  popup = GF_CANDIDATE_POPUP (object);

  g_clear_object (&popup->service);

  G_OBJECT_CLASS (gf_candidate_popup_parent_class)->dispose (object);
}

static void
gf_candidate_popup_class_init (GfCandidatePopupClass *popup_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (popup_class);

  object_class->dispose = gf_candidate_popup_dispose;
}

static void
gf_candidate_popup_init (GfCandidatePopup *popup)
{
  GtkWindow *window;
  GtkWidget *layout;

  window = GTK_WINDOW (popup);

  gtk_window_set_focus_on_map (window, TRUE);
  gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_NORMAL);

  layout = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (popup), layout);
  gtk_widget_show (layout);

  popup->pre_edit_text = gtk_label_new (NULL);
  gtk_container_add (GTK_CONTAINER (layout), popup->pre_edit_text);
  gtk_label_set_xalign (GTK_LABEL (popup->pre_edit_text), 0.0);

  popup->aux_text = gtk_label_new (NULL);
  gtk_container_add (GTK_CONTAINER (layout), popup->aux_text);
  gtk_label_set_xalign (GTK_LABEL (popup->aux_text), 0.0);

  popup->candidate_area = gf_candidate_area_new();
  gtk_container_add (GTK_CONTAINER (layout), popup->candidate_area);
  gtk_widget_show (popup->candidate_area);

  g_signal_connect (popup->candidate_area, "previous-page",
                    G_CALLBACK (previous_page_cb), popup);
  g_signal_connect (popup->candidate_area, "next-page",
                    G_CALLBACK (next_page_cb), popup);
  g_signal_connect (popup->candidate_area, "candidate-clicked",
                    G_CALLBACK (candidate_clicked_cb), popup);

  gtk_widget_set_name (GTK_WIDGET (popup), "gf-candidate-popup");
  gtk_container_set_border_width (GTK_CONTAINER (popup), 10);
}

GfCandidatePopup *
gf_candidate_popup_new (void)
{
  return g_object_new (GF_TYPE_CANDIDATE_POPUP,
                       "type", GTK_WINDOW_POPUP,
                       NULL);
}

void
gf_candidate_popup_set_panel_service (GfCandidatePopup *popup,
                                      IBusPanelService *service)
{
  g_clear_object (&popup->service);

  if (service == NULL)
    return;

  popup->service = g_object_ref (service);

  g_signal_connect (service, "set-cursor-location",
                    G_CALLBACK (set_cursor_location_cb), popup);
  g_signal_connect (service, "update-preedit-text",
                    G_CALLBACK (update_preedit_text_cb), popup);
  g_signal_connect (service, "show-preedit-text",
                    G_CALLBACK (show_preedit_text_cb), popup);
  g_signal_connect (service, "hide-preedit-text",
                    G_CALLBACK (hide_preedit_text_cb), popup);
  g_signal_connect (service, "update-auxiliary-text",
                    G_CALLBACK (update_auxiliary_text_cb), popup);
  g_signal_connect (service, "show-auxiliary-text",
                    G_CALLBACK (show_auxiliary_text_cb), popup);
  g_signal_connect (service, "hide-auxiliary-text",
                    G_CALLBACK (hide_auxiliary_text_cb), popup);
  g_signal_connect (service, "update-lookup-table",
                    G_CALLBACK (update_lookup_table_cb), popup);
  g_signal_connect (service, "show-lookup-table",
                    G_CALLBACK (show_lookup_table_cb), popup);
  g_signal_connect (service, "hide-lookup-table",
                    G_CALLBACK (hide_lookup_table_cb), popup);
  g_signal_connect (service, "focus-out",
                    G_CALLBACK (focus_out_cb), popup);
}
