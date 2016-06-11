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

#include <gdk/gdkx.h>

#include "gf-input-source.h"
#include "gf-input-source-popup.h"

struct _GfInputSourcePopup
{
  GfPopupWindow  parent;

  GList         *mru_sources;
  gboolean       backward;
  guint          keyval;
  guint          modifiers;

  GtkWidget     *sources_box;

  gint           selected_index;
  GtkWidget     *selected_name;
};

enum
{
  PROP_0,

  PROP_MRU_SOURCES,
  PROP_BACKWARD,
  PROP_KEYVAL,
  PROP_MODIFIERS,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfInputSourcePopup, gf_input_source_popup, GF_TYPE_POPUP_WINDOW)

static gboolean
is_mouse_over_input_source (GfInputSourcePopup *popup,
                            gdouble             x,
                            gdouble             y,
                            gint               *index)
{
  gboolean mouse_over;
  GList *sources;
  GList *l;

  mouse_over = FALSE;
  sources = gtk_container_get_children (GTK_CONTAINER (popup->sources_box));

  for (l = sources; l != NULL; l = g_list_next (l))
    {
      GtkWidget *widget;
      GtkAllocation rect;

      widget = GTK_WIDGET (l->data);

      gtk_widget_get_allocation (widget, &rect);

      if (x >= rect.x && x <= rect.x + rect.width &&
          y >= rect.y && y <= rect.y + rect.height)
        {
          gpointer pointer;

          pointer = g_object_get_data (G_OBJECT (widget), "index");

          *index = GPOINTER_TO_INT (pointer);
          mouse_over = TRUE;

          break;
        }
    }

  g_list_free (sources);

  return mouse_over;
}

static void
selected_source_changed (GfInputSourcePopup *popup)
{
  GtkWidget *widget;
  GfInputSource *source;
  const gchar *display_name;

  widget = GTK_WIDGET (popup);
  source = (GfInputSource *) g_list_nth_data (popup->mru_sources,
                                              popup->selected_index);

  display_name = gf_input_source_get_display_name (source);
  gtk_label_set_text (GTK_LABEL (popup->selected_name), display_name);

  gtk_widget_queue_draw (widget);
}

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   user_data)
{
  GfInputSourcePopup *popup;
  GtkStyleContext *context;
  gpointer index;

  popup = GF_INPUT_SOURCE_POPUP (user_data);
  context = gtk_widget_get_style_context (widget);
  index = g_object_get_data (G_OBJECT (widget), "index");

  if (popup->selected_index == GPOINTER_TO_INT (index))
    gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);
  else
    gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);

  return FALSE;
}

static void
setup_popup_window (GfInputSourcePopup *popup)
{
  GfInputSource *selected_source;
  GtkWidget *vertical_box;
  gint index;
  GList *l;
  const gchar *display_name;

  selected_source = (GfInputSource *) g_list_nth_data (popup->mru_sources,
                                                       popup->selected_index);

  gtk_container_set_border_width (GTK_CONTAINER (popup), 12);

  vertical_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_add (GTK_CONTAINER (popup), vertical_box);

  popup->sources_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vertical_box), popup->sources_box, TRUE, TRUE, 0);

  index = 0;
  for (l = popup->mru_sources; l != NULL; l = g_list_next (l))
    {
      GfInputSource *source;
      const gchar *short_name;
      GtkWidget *label;

      source = (GfInputSource *) l->data;
      short_name = gf_input_source_get_short_name (source);
      label = gtk_label_new (short_name);

      g_object_set_data (G_OBJECT (label), "index", GINT_TO_POINTER (index));
      g_signal_connect (label, "draw", G_CALLBACK (draw_cb), popup);

      gtk_widget_set_name (label, "gf-input-source");

      gtk_box_pack_start (GTK_BOX (popup->sources_box), label, FALSE, FALSE, 0);

      index++;
    }

  display_name = gf_input_source_get_display_name (selected_source);
  popup->selected_name = gtk_label_new (display_name);
  gtk_widget_set_halign (popup->selected_name, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (vertical_box), popup->selected_name, FALSE,
                      FALSE, 0);

  gtk_widget_show_all (vertical_box);
}

static void
ungrab (GfInputSourcePopup *popup)
{
  GdkDisplay *display;
  GdkSeat *seat;

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);

  gdk_seat_ungrab (seat);
}

static void
activate_selected_input_source (GfInputSourcePopup *popup)
{
  GfPopupWindow *popup_window;
  GfInputSource *source;

  popup_window = GF_POPUP_WINDOW (popup);
  source = (GfInputSource *) g_list_nth_data (popup->mru_sources,
                                              popup->selected_index);

  ungrab (popup);

  gf_popup_window_fade_start (popup_window);
  gf_input_source_activate (source, TRUE);
}

static void
gf_input_source_popup_constructed (GObject *object)
{
  GfInputSourcePopup *popup;

  popup = GF_INPUT_SOURCE_POPUP (object);

  G_OBJECT_CLASS (gf_input_source_popup_parent_class)->constructed (object);

  if (popup->backward)
    popup->selected_index = g_list_length (popup->mru_sources) - 1;
  else
    popup->selected_index = 1;

  setup_popup_window (popup);
}

static void
gf_input_source_popup_dispose (GObject *object)
{
  GfInputSourcePopup *popup;

  popup = GF_INPUT_SOURCE_POPUP (object);

  if (popup->mru_sources != NULL)
    {
      g_list_free (popup->mru_sources);
      popup->mru_sources = NULL;
    }

  G_OBJECT_CLASS (gf_input_source_popup_parent_class)->dispose (object);
}

static void
gf_input_source_popup_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GfInputSourcePopup *popup;

  popup = GF_INPUT_SOURCE_POPUP (object);

  switch (prop_id)
    {
      case PROP_MRU_SOURCES:
        popup->mru_sources = g_list_copy (g_value_get_pointer (value));
        break;

      case PROP_BACKWARD:
        popup->backward = g_value_get_boolean (value);
        break;

      case PROP_KEYVAL:
        popup->keyval = g_value_get_uint (value);
        break;

      case PROP_MODIFIERS:
        popup->modifiers = g_value_get_uint (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static gboolean
gf_input_source_popup_button_press_event (GtkWidget      *widget,
                                          GdkEventButton *event)
{
  GfInputSourcePopup *popup;
  GfPopupWindow *popup_window;
  gint index;

  popup = GF_INPUT_SOURCE_POPUP (widget);
  popup_window = GF_POPUP_WINDOW (widget);

  if (!is_mouse_over_input_source (popup, event->x, event->y, &index))
    {
      gf_popup_window_fade_start (popup_window);

      return TRUE;
    }

  popup->selected_index = index;
  selected_source_changed (popup);

  activate_selected_input_source (popup);

  return TRUE;
}

static gboolean
gf_input_source_popup_key_press_event (GtkWidget   *widget,
                                       GdkEventKey *event)
{
  GfInputSourcePopup *popup;
  gint index;
  gint last_index;

  popup = GF_INPUT_SOURCE_POPUP (widget);
  index = popup->selected_index;

  if (event->keyval == GDK_KEY_Left)
    {
      index--;
    }
  else if (event->keyval == GDK_KEY_Right)
    {
      index++;
    }
  else if (event->keyval == popup->keyval)
    {
      if (popup->backward)
        index--;
      else
        index++;
    }

  last_index = g_list_length (popup->mru_sources) - 1;
  if (index > last_index)
    index = 0;
  else if (index < 0)
    index = last_index;

  popup->selected_index = index;
  selected_source_changed (popup);

  return TRUE;
}

static gboolean
gf_input_source_popup_key_release_event (GtkWidget   *widget,
                                         GdkEventKey *event)
{
  GfInputSourcePopup *popup;
  GdkModifierType modifiers;

  if (!event->is_modifier)
    return TRUE;

  popup = GF_INPUT_SOURCE_POPUP (widget);
  modifiers = gtk_accelerator_get_default_mod_mask ();

  if ((event->state & modifiers) != popup->modifiers)
    return TRUE;

  activate_selected_input_source (popup);

  return TRUE;
}

static gboolean
gf_input_source_popup_motion_notify_event (GtkWidget      *widget,
                                           GdkEventMotion *event)
{
  GfInputSourcePopup *popup;
  gint index;

  popup = GF_INPUT_SOURCE_POPUP (widget);

  if (!is_mouse_over_input_source (popup, event->x, event->y, &index))
    return TRUE;

  popup->selected_index = index;
  selected_source_changed (popup);

  return TRUE;
}

static void
gf_input_source_popup_show (GtkWidget *widget)
{
  GdkDisplay *display;
  GdkSeat *seat;
  GdkWindow *window;
  GdkSeatCapabilities capabilities;
  GdkGrabStatus status;

  GTK_WIDGET_CLASS (gf_input_source_popup_parent_class)->show (widget);

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);
  window = gtk_widget_get_window (widget);

  capabilities = GDK_SEAT_CAPABILITY_POINTER |
                 GDK_SEAT_CAPABILITY_KEYBOARD;

  status = gdk_seat_grab (seat, window, capabilities, FALSE, NULL,
                          NULL, NULL, NULL);

  if (status != GDK_GRAB_SUCCESS)
    return;
}

static void
gf_input_source_popup_class_init (GfInputSourcePopupClass *popup_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (popup_class);
  widget_class = GTK_WIDGET_CLASS (popup_class);

  object_class->constructed = gf_input_source_popup_constructed;
  object_class->dispose = gf_input_source_popup_dispose;
  object_class->set_property = gf_input_source_popup_set_property;

  widget_class->button_press_event = gf_input_source_popup_button_press_event;
  widget_class->key_press_event = gf_input_source_popup_key_press_event;
  widget_class->key_release_event = gf_input_source_popup_key_release_event;
  widget_class->motion_notify_event = gf_input_source_popup_motion_notify_event;
  widget_class->show = gf_input_source_popup_show;

  properties[PROP_MRU_SOURCES] =
    g_param_spec_pointer ("mru-sources", "mru-sources", "mru-sources",
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_BACKWARD] =
    g_param_spec_boolean ("backward", "backward", "backward", FALSE,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                          G_PARAM_STATIC_STRINGS);

  properties[PROP_KEYVAL] =
    g_param_spec_uint ("keyval", "keyval", "keyval", 0, G_MAXUINT, 0,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                       G_PARAM_STATIC_STRINGS);

  properties[PROP_MODIFIERS] =
    g_param_spec_uint ("modifiers", "modifiers", "modifiers", 0, G_MAXUINT, 0,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gf_input_source_popup_init (GfInputSourcePopup *popup)
{
  GtkWindow *window;
  GtkWidget *widget;
  gint events;

  window = GTK_WINDOW (popup);
  widget = GTK_WIDGET (popup);

  events = GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
           GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK;

  gtk_window_set_position (window, GTK_WIN_POS_CENTER_ALWAYS);

  gtk_widget_add_events (widget, events);
  gtk_widget_set_name (widget, "gf-input-source-popup");
}

GtkWidget *
gf_input_source_popup_new (GList    *mru_sources,
                           gboolean  backward,
                           guint     keyval,
                           guint     modifiers)
{
  return g_object_new (GF_TYPE_INPUT_SOURCE_POPUP,
                       "type", GTK_WINDOW_POPUP,
                       "mru-sources", mru_sources,
                       "backward", backward,
                       "keyval", keyval,
                       "modifiers", modifiers,
                       NULL);
}
