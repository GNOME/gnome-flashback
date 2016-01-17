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

#include "gf-candidate-area.h"
#include "gf-candidate-popup.h"

struct _GfCandidatePopup
{
  GfPopupWindow     parent;

  IBusPanelService *service;

  GtkWidget        *pre_edit_text;
  GtkWidget        *aux_text;
  GtkWidget        *candidate_area;
};

G_DEFINE_TYPE (GfCandidatePopup, gf_candidate_popup, GF_TYPE_POPUP_WINDOW)

static void
set_cursor_location_cb (IBusPanelService *service,
                        gint              x,
                        gint              y,
                        gint              w,
                        gint              h,
                        gpointer          user_data)
{
}

static void
update_preedit_text_cb (IBusPanelService *service,
                        IBusText         *text,
                        guint             cursor_pos,
                        gboolean          visible,
                        gpointer          user_data)
{
}

static void
show_preedit_text_cb (IBusPanelService *service,
                      gpointer          user_data)
{
}

static void
hide_preedit_text_cb (IBusPanelService *service,
                      gpointer          user_data)
{
}

static void
update_auxiliary_text_cb (IBusPanelService *service,
                          IBusText         *text,
                          gboolean          visible,
                          gpointer          user_data)
{
}

static void
show_auxiliary_text_cb (IBusPanelService *service,
                        gpointer          user_data)
{
}

static void
hide_auxiliary_text_cb (IBusPanelService *service,
                        gpointer          user_data)
{
}

static void
update_lookup_table_cb (IBusPanelService *service,
                        IBusLookupTable  *lookup_table,
                        gboolean          visible,
                        gpointer          user_data)
{
}

static void
show_lookup_table_cb (IBusPanelService *service,
                      gpointer          user_data)
{
}

static void
hide_lookup_table_cb (IBusPanelService *service,
                      gpointer          user_data)
{
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

  layout = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (popup), layout);
  gtk_widget_show (layout);

  popup->pre_edit_text = gtk_label_new (NULL);
  gtk_container_add (GTK_CONTAINER (layout), popup->pre_edit_text);

  popup->aux_text = gtk_label_new (NULL);
  gtk_container_add (GTK_CONTAINER (layout), popup->aux_text);

  popup->candidate_area = gf_candidate_area_new();
  gtk_container_add (GTK_CONTAINER (layout), popup->candidate_area);
  gtk_widget_show (popup->candidate_area);

  g_signal_connect (popup->candidate_area, "previous-page",
                    G_CALLBACK (previous_page_cb), popup);
  g_signal_connect (popup->candidate_area, "next-page",
                    G_CALLBACK (next_page_cb), popup);
  g_signal_connect (popup->candidate_area, "candidate-clicked",
                    G_CALLBACK (candidate_clicked_cb), popup);
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
