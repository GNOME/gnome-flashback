/*
 * Copyright (C) 2008 William Jon McCann
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>
#include <pulse/pulseaudio.h>

#include "gf-sound-item.h"
#include "gvc-channel-bar.h"

struct _GfSoundItem
{
  SnItem           parent;

  gchar           *display_name;
  gchar          **icon_names;

  GvcMixerStream  *mixer_stream;
  gulong           notify_volume_id;
  gulong           notify_is_muted_id;

  GtkWidget       *dock;
  GtkWidget       *bar;
};

enum
{
  PROP_0,

  PROP_DISPLAY_NAME,
  PROP_ICON_NAMES,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfSoundItem, gf_sound_item, SN_TYPE_ITEM)

static void
ungrab (GfSoundItem *item)
{
  GdkDisplay *display;
  GdkSeat *seat;

  display = gtk_widget_get_display (item->dock);
  seat = gdk_display_get_default_seat (display);

  gdk_seat_ungrab (seat);

  gtk_grab_remove (item->dock);
  gtk_widget_hide (item->dock);
}

static gboolean
button_press_event_cb (GtkWidget      *widget,
                       GdkEventButton *event,
                       gpointer        user_data)
{
  GfSoundItem *item;

  if (event->type != GDK_BUTTON_PRESS)
    return GDK_EVENT_PROPAGATE;

  item = GF_SOUND_ITEM (user_data);

  ungrab (item);

  return GDK_EVENT_STOP;
}

static gboolean
key_release_event_cb (GtkWidget   *widget,
                      GdkEventKey *event,
                      gpointer     user_data)
{
  GfSoundItem *item;

  if (event->keyval != GDK_KEY_Escape)
    return GDK_EVENT_PROPAGATE;

  item = GF_SOUND_ITEM (user_data);

  ungrab (item);

  return GDK_EVENT_STOP;
}

static gboolean
scroll_event_cb (GtkWidget      *widget,
                 GdkEventScroll *event,
                 gpointer        user_data)
{
  GfSoundItem *item;

  item = GF_SOUND_ITEM (user_data);

  return gvc_channel_bar_scroll (GVC_CHANNEL_BAR (item->bar), event);
}

static void
update_icon (GfSoundItem *item)
{
  guint volume;
  gboolean is_muted;
  gdouble db;
  gboolean can_decibel;
  guint n;
  gdouble percent;
  const gchar *description;
  gchar *title;
  gchar *text;

  if (item->mixer_stream == NULL)
    return;

  volume = gvc_mixer_stream_get_volume (item->mixer_stream);
  is_muted = gvc_mixer_stream_get_is_muted (item->mixer_stream);
  db = gvc_mixer_stream_get_decibel (item->mixer_stream);
  can_decibel = gvc_mixer_stream_get_can_decibel (item->mixer_stream);

  if (volume == 0 || is_muted)
    {
      n = 0;
    }
  else
    {
      n = 3 * volume / PA_VOLUME_NORM + 1;

      if (n < 1)
        n = 1;
      else if (n > 3)
        n = 3;
    }

  sn_item_set_icon_name (SN_ITEM (item), item->icon_names[n]);

  percent = 100 * (float) volume / PA_VOLUME_NORM;
  description = gvc_mixer_stream_get_description (item->mixer_stream);

  if (is_muted)
    {
      title = g_strdup_printf ("<b>%s: %s</b>", item->display_name, _("Muted"));
      text = g_strdup_printf ("<small>%s</small>", description);
    }
  else if (can_decibel && (db > PA_DECIBEL_MININFTY))
    {
      title = g_strdup_printf ("<b>%s: %.0f%%</b>", item->display_name, percent);
      text = g_strdup_printf ("<small>%0.2f dB\n%s</small>", db, description);
    }
  else if (can_decibel)
    {
      title = g_strdup_printf ("<b>%s: %.0f%%</b>", item->display_name, percent);
      text = g_strdup_printf ("<small>-&#8734; dB\n%s</small>", description);
    }
  else
    {
      title = g_strdup_printf ("<b>%s: %.0f%%</b>", item->display_name, percent);
      text = g_strdup_printf ("<small>%s</small>", description);
    }

  sn_item_set_tooltip (SN_ITEM (item), NULL, NULL, title, text);

  g_free (title);
  g_free (text);
}

static void
value_changed_cb (GtkAdjustment *adjustment,
                  gpointer       user_data)
{
  GfSoundItem *item;
  gdouble volume;
  pa_volume_t volume_t;

  item = GF_SOUND_ITEM (user_data);

  volume = gtk_adjustment_get_value (adjustment);
  volume_t = (pa_volume_t) round (volume);

  if (gvc_mixer_stream_set_volume (item->mixer_stream, volume_t))
    gvc_mixer_stream_push_volume (item->mixer_stream);
}

static void
update_dock (GfSoundItem *item)
{
  GtkAdjustment *adjustment;
  gdouble value;
  gboolean is_muted;

  adjustment = gvc_channel_bar_get_adjustment (GVC_CHANNEL_BAR (item->bar));
  value = gvc_mixer_stream_get_volume (item->mixer_stream);

  g_signal_handlers_block_by_func (adjustment, value_changed_cb, item);
  gtk_adjustment_set_value (adjustment, value);
  g_signal_handlers_unblock_by_func (adjustment, value_changed_cb, item);

  is_muted = gvc_mixer_stream_get_is_muted (item->mixer_stream);
  gvc_channel_bar_set_is_muted (GVC_CHANNEL_BAR (item->bar), is_muted);
}

static void
bar_notify_is_muted_cb (GObject    *object,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  GfSoundItem *item;
  gboolean is_muted;

  item = GF_SOUND_ITEM (user_data);
  is_muted = gvc_channel_bar_get_is_muted (GVC_CHANNEL_BAR (item->bar));

  if (gvc_mixer_stream_get_is_muted (item->mixer_stream) == is_muted)
    return;

  gvc_mixer_stream_set_is_muted (item->mixer_stream, is_muted);
  gvc_mixer_stream_change_is_muted (item->mixer_stream, is_muted);
}

static void
gf_sound_item_constructed (GObject *object)
{
  GfSoundItem *item;
  GtkWidget *frame;
  GtkWidget *box;
  GtkAdjustment *adjustment;

  G_OBJECT_CLASS (gf_sound_item_parent_class)->constructed (object);

  item = GF_SOUND_ITEM (object);

  item->dock = gtk_window_new (GTK_WINDOW_POPUP);

  gtk_widget_set_name (item->dock, "gvc-stream-status-icon-popup-window");
  gtk_window_set_decorated (GTK_WINDOW (item->dock), FALSE);

  g_signal_connect (item->dock, "button-press-event",
                    G_CALLBACK (button_press_event_cb), item);
  g_signal_connect (item->dock, "key-release-event",
                    G_CALLBACK (key_release_event_cb), item);
  g_signal_connect (item->dock, "scroll-event",
                    G_CALLBACK (scroll_event_cb), item);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (item->dock), frame);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (box), 2);
  gtk_container_add (GTK_CONTAINER (frame), box);

  item->bar = gvc_channel_bar_new ();
  gtk_box_pack_start (GTK_BOX (box), item->bar, TRUE, FALSE, 0);

  gvc_channel_bar_set_orientation (GVC_CHANNEL_BAR (item->bar),
                                   GTK_ORIENTATION_VERTICAL);

  g_signal_connect (item->bar, "notify::is-muted",
                    G_CALLBACK (bar_notify_is_muted_cb), item);

  adjustment = gvc_channel_bar_get_adjustment (GVC_CHANNEL_BAR (item->bar));
  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (value_changed_cb), item);
}

static void
clear_mixer_stream (GfSoundItem *item)
{
  if (item->mixer_stream == NULL)
    return;

  g_signal_handler_disconnect (item->mixer_stream, item->notify_volume_id);
  g_signal_handler_disconnect (item->mixer_stream, item->notify_is_muted_id);

  g_clear_object (&item->mixer_stream);
}

static void
gf_sound_item_dispose (GObject *object)
{
  GfSoundItem *item;

  item = GF_SOUND_ITEM (object);

  if (item->dock != NULL)
    {
      gtk_widget_destroy (item->dock);
      item->dock = NULL;
    }

  clear_mixer_stream (item);

  G_OBJECT_CLASS (gf_sound_item_parent_class)->dispose (object);
}

static void
gf_sound_item_finalize (GObject *object)
{
  GfSoundItem *item;

  item = GF_SOUND_ITEM (object);

  g_free (item->display_name);
  g_strfreev (item->icon_names);

  G_OBJECT_CLASS (gf_sound_item_parent_class)->finalize (object);
}

static void
gf_sound_item_set_property (GObject       *object,
                            guint          property_id,
                            const GValue  *value,
                            GParamSpec    *pspec)
{
  GfSoundItem *item;

  item = GF_SOUND_ITEM (object);

  switch (property_id)
    {
      case PROP_DISPLAY_NAME:
        item->display_name = g_value_dup_string (value);
        break;

      case PROP_ICON_NAMES:
        item->icon_names = (gchar **) g_value_dup_boxed (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
toggled_cb (GtkMenuItem *item,
            gpointer     user_data)
{
  GfSoundItem *sound_item;
  gboolean is_muted;

  sound_item = GF_SOUND_ITEM (user_data);
  is_muted = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item));

  gvc_channel_bar_set_is_muted (GVC_CHANNEL_BAR (sound_item->bar), is_muted);
}

static void
activate_cb (GtkMenuItem *item,
             gpointer     user_data)
{
  GdkDisplay *display;
  GdkAppLaunchContext *context;
  GAppInfoCreateFlags flags;
  GError *error;
  GAppInfo *app_info;

  display = gdk_display_get_default ();
  context = gdk_display_get_app_launch_context (display);

  flags = G_APP_INFO_CREATE_NONE;
  error = NULL;

  app_info = g_app_info_create_from_commandline ("gnome-control-center sound",
                                                 "Sound preferences", flags,
                                                 &error);

  if (app_info)
    g_app_info_launch (app_info, NULL, G_APP_LAUNCH_CONTEXT (context), &error);

  if (error != NULL)
    {
      gchar *msg;
      GtkWidget *dialog;

      msg = g_strdup_printf (_("Failed to start Sound Preferences: %s"),
                             error->message);

      dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE, "%s", msg);

      g_error_free (error);
      g_free (msg);

      g_signal_connect (dialog, "response",
                        G_CALLBACK (gtk_widget_destroy), NULL);

      gtk_widget_show (dialog);
    }

  g_clear_object (&app_info);
  g_object_unref (context);
}

typedef struct
{
  gint x;
  gint y;
} MenuPosition;

static void
context_menu_position (GtkMenu  *menu,
                       gint     *x,
                       gint     *y,
                       gboolean *push_in,
                       gpointer  user_data)
{
  MenuPosition *position;

  position = (MenuPosition *) user_data;

  *x = position->x;
  *y = position->y;
}

static void
gf_sound_item_context_menu (SnItem *item,
                            gint    x,
                            gint    y)
{
  GfSoundItem *sound_item;
  GtkWidget *menu;
  gboolean is_muted;
  GtkWidget *menu_item;
  guint32 event_time;
  MenuPosition position;

  sound_item = GF_SOUND_ITEM (item);

  menu = gtk_menu_new ();
  is_muted = gvc_mixer_stream_get_is_muted (sound_item->mixer_stream);

  menu_item = gtk_check_menu_item_new_with_mnemonic (_("_Mute"));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item), is_muted);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
  gtk_widget_show (menu_item);

  g_signal_connect (menu_item, "toggled",
                    G_CALLBACK (toggled_cb), sound_item);

  menu_item = gtk_menu_item_new_with_mnemonic (_("_Sound Preferences"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
  gtk_widget_show (menu_item);

  g_signal_connect (menu_item, "activate",
                    G_CALLBACK (activate_cb), sound_item);

  position.x = x;
  position.y = y;

  event_time = gtk_get_current_event_time ();

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                  context_menu_position, &position,
                  0, event_time);
}

static void
gf_sound_item_activate (SnItem *item,
                        gint    x,
                        gint    y)
{
  GfSoundItem *sound_item;
  GtkWindow *gtk_window;
  GdkDisplay *display;
  GdkSeat *seat;
  GdkWindow *gdk_window;
  GdkSeatCapabilities capabilities;
  GdkGrabStatus status;

  sound_item = GF_SOUND_ITEM (item);

  update_dock (sound_item);

  gvc_channel_bar_set_orientation (GVC_CHANNEL_BAR (sound_item->bar),
                                   GTK_ORIENTATION_VERTICAL);

  gtk_widget_show_all (sound_item->dock);

  gtk_window = GTK_WINDOW (sound_item->dock);
  gtk_window_move (gtk_window, x, y);

  gtk_grab_add (sound_item->dock);

  display = gtk_widget_get_display (sound_item->dock);
  seat = gdk_display_get_default_seat (display);
  gdk_window = gtk_widget_get_window (sound_item->dock);

  capabilities = GDK_SEAT_CAPABILITY_POINTER | GDK_SEAT_CAPABILITY_KEYBOARD;

  status = gdk_seat_grab (seat, gdk_window, capabilities, TRUE,
                          NULL, NULL, NULL, NULL);

  if (status != GDK_GRAB_SUCCESS)
    {
      ungrab (sound_item);

      return;
    }

  gtk_widget_grab_focus (sound_item->dock);
}

static void
gf_sound_item_secondary_activate (SnItem *item,
                                  gint    x,
                                  gint    y)
{
}

static void
gf_sound_item_scroll (SnItem            *item,
                      gint               delta,
                      SnItemOrientation  orientation)
{
  GfSoundItem *sound_item;
  GdkEvent *event;
  GdkScrollDirection direction;

  sound_item = GF_SOUND_ITEM (item);

  if (orientation == SN_ITEM_ORIENTATION_HORIZONTAL)
    {
      if (delta < 0)
        direction = GDK_SCROLL_RIGHT;
      else
        direction = GDK_SCROLL_LEFT;
    }
  else
    {
      if (delta < 0)
        direction = GDK_SCROLL_DOWN;
      else
        direction = GDK_SCROLL_UP;
    }

  event = gdk_event_new (GDK_SCROLL);

  event->scroll.direction = direction;
  event->scroll.delta_x = delta;
  event->scroll.delta_y = delta;

  gvc_channel_bar_scroll (GVC_CHANNEL_BAR (sound_item->bar),
                          (GdkEventScroll *) event);
  gdk_event_free (event);
}

static void
gf_sound_item_class_init (GfSoundItemClass *item_class)
{
  GObjectClass *object_class;
  SnItemClass *sn_item_class;

  object_class = G_OBJECT_CLASS (item_class);
  sn_item_class = SN_ITEM_CLASS (item_class);

  object_class->constructed = gf_sound_item_constructed;
  object_class->dispose = gf_sound_item_dispose;
  object_class->finalize = gf_sound_item_finalize;
  object_class->set_property = gf_sound_item_set_property;

  sn_item_class->context_menu = gf_sound_item_context_menu;
  sn_item_class->activate = gf_sound_item_activate;
  sn_item_class->secondary_activate = gf_sound_item_secondary_activate;
  sn_item_class->scroll = gf_sound_item_scroll;

  properties[PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display Name",
                         "Name to display for this stream",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_ICON_NAMES] =
    g_param_spec_boxed ("icon-names",
                        "Icon Names",
                        "Name of icon to display for this stream",
                        G_TYPE_STRV,
                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gf_sound_item_init (GfSoundItem *item)
{
}

GfSoundItem *
gf_sound_item_new (SnItemCategory   category,
                   const gchar     *id,
                   const gchar     *title,
                   const gchar     *display_name,
                   const gchar    **icon_names)
{
  return g_object_new (GF_TYPE_SOUND_ITEM,
                       "version", 1,
                       "category", category,
                       "id", id,
                       "title", title,
                       "display-name", display_name,
                       "icon-names", icon_names,
                       NULL);
}

static void
notify_volume_cb (GObject    *object,
                  GParamSpec *pspec,
                  gpointer    user_data)
{
  GfSoundItem *item;

  item = GF_SOUND_ITEM (user_data);

  update_icon (item);
  update_dock (item);
}

static void
stream_notify_is_muted_cb (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
  GfSoundItem *item;

  item = GF_SOUND_ITEM (user_data);

  update_icon (item);
  update_dock (item);
}

void
gf_sound_item_set_mixer_stream (GfSoundItem    *item,
                                GvcMixerStream *stream)
{
  GtkAdjustment *adjustment;
  gdouble value;

  clear_mixer_stream (item);

  if (stream == NULL)
    return;

  item->mixer_stream = g_object_ref (stream);

  gvc_channel_bar_set_base_volume (GVC_CHANNEL_BAR (item->bar),
                                   gvc_mixer_stream_get_base_volume (stream));
  gvc_channel_bar_set_is_amplified (GVC_CHANNEL_BAR (item->bar),
                                    gvc_mixer_stream_get_can_decibel (stream));

  adjustment = gvc_channel_bar_get_adjustment (GVC_CHANNEL_BAR (item->bar));
  value = gvc_mixer_stream_get_volume (item->mixer_stream);

  g_signal_handlers_block_by_func (adjustment, value_changed_cb, item);
  gtk_adjustment_set_value (adjustment, value);
  g_signal_handlers_unblock_by_func (adjustment, value_changed_cb, item);

  item->notify_volume_id =
    g_signal_connect (item->mixer_stream, "notify::volume",
                      G_CALLBACK (notify_volume_cb), item);

  item->notify_is_muted_id =
    g_signal_connect (item->mixer_stream, "notify::is-muted",
                      G_CALLBACK (stream_notify_is_muted_cb), item);

  update_icon (item);
}
