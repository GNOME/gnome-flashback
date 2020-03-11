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
#include "si-volume.h"

#include <canberra-gtk.h>
#include <glib/gi18n-lib.h>
#include <libgnome-panel/gp-image-menu-item.h>
#include <math.h>

#include "dbus/gf-shell-gen.h"
#include "gvc-mixer-stream.h"
#include "si-desktop-menu-item.h"

#define ALLOW_AMPLIFIED_VOLUME_KEY "allow-volume-above-100-percent"
#define PA_DECIBEL_MININFTY -200.0

struct _SiVolume
{
  SiIndicator      parent;

  GSettings       *settings;
  gboolean         allow_amplified;

  GvcMixerControl *control;
  gboolean         input;

  GvcMixerStream  *stream;
  gulong           is_muted_id;
  gulong           volume_id;
  gulong           port_id;

  gboolean         has_headphones;

  GtkWidget       *mute;

  GtkWidget       *slider_item;
  gboolean         slider_selected;
  GtkWidget       *slider_icon;
  GtkWidget       *slider_scale;
  gulong           value_changed_id;

  GCancellable    *cancellable;
  GfShellGen      *shell;
};

enum
{
  PROP_0,

  PROP_CONTROL,
  PROP_INPUT,

  LAST_PROP
};

static GParamSpec *volume_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (SiVolume, si_volume, SI_TYPE_INDICATOR)

static const char *input_icons[] =
{
  "microphone-sensitivity-muted",
  "microphone-sensitivity-low",
  "microphone-sensitivity-medium",
  "microphone-sensitivity-high",
  NULL
};

static const char *input_icons_symbolic[] =
{
  "microphone-sensitivity-muted-symbolic",
  "microphone-sensitivity-low-symbolic",
  "microphone-sensitivity-medium-symbolic",
  "microphone-sensitivity-high-symbolic",
  NULL
};

static const char *output_icons[] =
{
  "audio-volume-muted",
  "audio-volume-low",
  "audio-volume-medium",
  "audio-volume-high",
  "audio-volume-overamplified",
  NULL
};

static const char *output_icons_symbolic[] =
{
  "audio-volume-muted-symbolic",
  "audio-volume-low-symbolic",
  "audio-volume-medium-symbolic",
  "audio-volume-high-symbolic",
  "audio-volume-overamplified-symbolic",
  NULL
};

static const char *skipped_apps[] =
{
  "org.gnome.VolumeControl",
  "org.PulseAudio.pavucontrol",
  NULL
};

static void
update_scale (SiVolume *self)
{
  gboolean is_muted;
  double max_norm;
  double volume;
  double value;

  is_muted = gvc_mixer_stream_get_is_muted (self->stream);
  max_norm = gvc_mixer_control_get_vol_max_norm (self->control);
  volume = gvc_mixer_stream_get_volume (self->stream);

  value = is_muted ? 0.0 : volume / max_norm;

  g_signal_handler_block (self->slider_scale, self->value_changed_id);
  gtk_range_set_value (GTK_RANGE (self->slider_scale), value);
  g_signal_handler_unblock (self->slider_scale, self->value_changed_id);
}

static double
get_max_level (SiVolume *self)
{
  double max_norm;
  double max;

  max_norm = gvc_mixer_control_get_vol_max_norm (self->control);

  if (self->allow_amplified)
    max = gvc_mixer_control_get_vol_max_amplified (self->control);
  else
    max = max_norm;

  return max / max_norm;
}

static void
update_scale_range (SiVolume *self)
{
  double max;

  max = self->allow_amplified ? get_max_level (self) : 1.0;
  gtk_range_set_range (GTK_RANGE (self->slider_scale), 0.0, max);

  gtk_scale_clear_marks (GTK_SCALE (self->slider_scale));
  if (self->allow_amplified)
    gtk_scale_add_mark (GTK_SCALE (self->slider_scale), 1.0, GTK_POS_BOTTOM, NULL);

  update_scale (self);
}

static void
update_slider_icon (SiVolume *self)
{
  GpApplet *applet;
  guint icon_size;
  const char *icon_name;

  if (self->stream == NULL)
    return;

  applet = si_indicator_get_applet (SI_INDICATOR (self));
  icon_size = gp_applet_get_menu_icon_size (applet);

  if (self->input)
    icon_name = "audio-input-microphone";
  else
    icon_name = self->has_headphones ? "audio-headphones" : "audio-speakers";

  gtk_image_set_from_icon_name (GTK_IMAGE (self->slider_icon),
                                icon_name,
                                GTK_ICON_SIZE_MENU);

  gtk_image_set_pixel_size (GTK_IMAGE (self->slider_icon), icon_size);
}

static void
mute_toggled_cb (GtkCheckMenuItem *item,
                 SiVolume         *self)
{
  gboolean is_muted;

  is_muted = gtk_check_menu_item_get_active (item);
  gvc_mixer_stream_change_is_muted (self->stream, is_muted);
}

static void
scale_value_changed_cb (GtkRange *range,
                        SiVolume *self)
{
  double value;
  double volume;
  gboolean is_muted;
  gboolean changed;

  value = gtk_range_get_value (range);
  volume = value * gvc_mixer_control_get_vol_max_norm (self->control);
  is_muted = gvc_mixer_stream_get_is_muted (self->stream);

  if (volume < 1.0)
    {
      changed = gvc_mixer_stream_set_volume (self->stream, 0.0);

      if (!is_muted)
        gvc_mixer_stream_change_is_muted (self->stream, FALSE);
    }
  else
    {
      changed = gvc_mixer_stream_set_volume (self->stream, volume);

      if (is_muted)
        gvc_mixer_stream_change_is_muted (self->stream, TRUE);
    }

  if (!changed)
    return;

  gvc_mixer_stream_push_volume (self->stream);

  if (gvc_mixer_stream_get_state (self->stream) == GVC_STREAM_STATE_RUNNING)
    return;

  ca_gtk_play_for_widget (self->slider_scale,
                          0,
                          CA_PROP_EVENT_ID, "audio-volume-change",
                          CA_PROP_EVENT_DESCRIPTION, _("Volume changed"),
                          CA_PROP_APPLICATION_ID, "org.gnome.VolumeControl",
                          NULL);
}

static void
update_menu (SiVolume *self)
{
  gboolean is_muted;

  is_muted = gvc_mixer_stream_get_is_muted (self->stream);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (self->mute), is_muted);

  update_slider_icon (self);
  update_scale (self);
}

static void
append_mute_item (SiVolume  *self,
                  GtkWidget *menu)
{
  self->mute = gtk_check_menu_item_new_with_mnemonic (_("Mute"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), self->mute);
  gtk_widget_show (self->mute);

  g_signal_connect (self->mute, "toggled", G_CALLBACK (mute_toggled_cb), self);
}

static void
maybe_emit_event_on_scale_widget (SiVolume       *self,
                                  GtkWidget      *item,
                                  GdkEventButton *event)
{
  GtkAllocation scale_rect;
  int x;
  int y;

  gtk_widget_get_allocation (self->slider_scale, &scale_rect);
  gtk_widget_translate_coordinates (item,
                                    self->slider_scale,
                                    event->x,
                                    event->y,
                                    &x,
                                    &y);

  if (x < 0 || x > scale_rect.width || y < 0 || y > scale_rect.height)
    return;

  gtk_widget_event (self->slider_scale, (GdkEvent *) event);
}

static gboolean
slider_button_press_event_cb (GtkWidget      *item,
                              GdkEventButton *event,
                              SiVolume       *self)
{
  maybe_emit_event_on_scale_widget (self, item, event);

  return FALSE;
}

static gboolean
slider_button_release_event_cb (GtkWidget      *item,
                                GdkEventButton *event,
                                SiVolume       *self)
{
  maybe_emit_event_on_scale_widget (self, item, event);

  return TRUE;
}

static gboolean
slider_scroll_event_cb (GtkWidget      *item,
                        GdkEventScroll *event,
                        SiVolume       *self)
{
  gtk_widget_event (self->slider_scale, (GdkEvent *) event);

  return FALSE;
}

static void
slider_deselect_cb (GtkMenuItem *item,
                    SiVolume    *self)
{
  self->slider_selected = FALSE;
}

static void
slider_select_cb (GtkMenuItem *item,
                  SiVolume    *self)
{
  self->slider_selected = TRUE;
}

static void
append_slider_item (SiVolume  *self,
                    GtkWidget *menu)
{
  self->slider_item = gp_image_menu_item_new ();
  gtk_widget_add_events (self->slider_item, GDK_SCROLL_MASK);
  gtk_widget_set_size_request (self->slider_item, 200, -1);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), self->slider_item);
  gtk_widget_show (self->slider_item);

  g_signal_connect (self->slider_item,
                    "deselect",
                    G_CALLBACK (slider_deselect_cb),
                    self);

  g_signal_connect (self->slider_item,
                    "select",
                    G_CALLBACK (slider_select_cb),
                    self);

  self->slider_icon = gtk_image_new ();
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (self->slider_item),
                                self->slider_icon);

  self->slider_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                 0.00,
                                                 1.00,
                                                 0.01);

  g_signal_connect (self->slider_item,
                    "button-press-event",
                    G_CALLBACK (slider_button_press_event_cb),
                    self);

  g_signal_connect (self->slider_item,
                    "button-release-event",
                    G_CALLBACK (slider_button_release_event_cb),
                    self);

  g_signal_connect (self->slider_item,
                    "scroll-event",
                    G_CALLBACK (slider_scroll_event_cb),
                    self);

  gtk_scale_set_draw_value (GTK_SCALE (self->slider_scale), FALSE);
  gtk_container_add (GTK_CONTAINER (self->slider_item), self->slider_scale);
  gtk_widget_show (self->slider_scale);

  self->value_changed_id = g_signal_connect (self->slider_scale,
                                             "value-changed",
                                             G_CALLBACK (scale_value_changed_cb),
                                             self);
}

static gboolean
menu_key_press_event_cb (GtkWidget   *widget,
                         GdkEventKey *event,
                         SiVolume    *self)
{
  if (!self->slider_selected)
    return FALSE;

  switch (event->keyval)
    {
      case GDK_KEY_Left:
      case GDK_KEY_Right:
      case GDK_KEY_minus:
      case GDK_KEY_plus:
      case GDK_KEY_KP_Subtract:
      case GDK_KEY_KP_Add:
        gtk_widget_event (self->slider_scale, (GdkEvent *) event);
        return TRUE;

      default:
        break;
    }

  return FALSE;
}

static GtkWidget *
create_menu (SiVolume *self)
{
  GtkWidget *menu;
  GtkWidget *separator;
  GtkWidget *item;

  menu = gtk_menu_new ();

  g_signal_connect (menu,
                    "key-press-event",
                    G_CALLBACK (menu_key_press_event_cb),
                    self);

  append_mute_item (self, menu);

  separator = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), separator);
  gtk_widget_show (separator);

  append_slider_item (self, menu);

  separator = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), separator);
  gtk_widget_show (separator);

  item = si_desktop_menu_item_new (_("Sound Settings"),
                                   "gnome-sound-panel.desktop");

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  return menu;
}

static const char *
get_icon (SiVolume *self,
          gboolean  symbolic)
{
  const char **icons;
  pa_volume_t volume;
  unsigned int icon;

  if (self->input)
    {
      icons = input_icons;
      if (symbolic)
        icons = input_icons_symbolic;
    }
  else
    {
      icons = output_icons;
      if (symbolic)
        icons = output_icons_symbolic;
    }

  volume = gvc_mixer_stream_get_volume (self->stream);
  icon = 0;

  if (!gvc_mixer_stream_get_is_muted (self->stream) &&
      volume > 0)
    {
      double max_norm;
      int tmp;

      max_norm = gvc_mixer_control_get_vol_max_norm (self->control);
      tmp = ceil (3 * volume / max_norm);

      if (tmp < 1)
        icon = 1;
      else if (tmp > 3)
        icon = self->input ? 3 : 4;
      else
        icon = tmp;
    }

  return icons[icon];
}

static void
update_icon (SiVolume *self)
{
  GpApplet *applet;
  const char *icon_name;

  applet = si_indicator_get_applet (SI_INDICATOR (self));
  icon_name = get_icon (self, gp_applet_get_prefer_symbolic_icons (applet));

  si_indicator_set_icon_name (SI_INDICATOR (self), icon_name);
}

static void
update_tooltip (SiVolume  *self,
                GtkWidget *item)
{
  const char *display_name;
  const char *description;
  gboolean can_decibel;
  double decibel;
  pa_volume_t volume;
  double max_norm;
  char *tooltip;

  display_name = self->input ? _("Input") : _("Output");
  description = gvc_mixer_stream_get_description (self->stream);
  can_decibel = gvc_mixer_stream_get_can_decibel (self->stream);
  decibel = gvc_mixer_stream_get_decibel (self->stream);
  volume = gvc_mixer_stream_get_volume (self->stream);
  max_norm = gvc_mixer_control_get_vol_max_norm (self->control);

  if (gvc_mixer_stream_get_is_muted (self->stream))
    {
      tooltip = g_strdup_printf ("<b>%s: %s</b>\n<small>%s</small>",
                                 display_name,
                                 _("Muted"),
                                 description);
    }
  else if (can_decibel && decibel > PA_DECIBEL_MININFTY)
    {
      tooltip = g_strdup_printf ("<b>%s: %.0f%%</b>\n<small>%0.2f dB\n%s</small>",
                                 display_name,
                                 100 * volume / max_norm,
                                 decibel,
                                 description);
    }
  else if (can_decibel)
    {
      tooltip = g_strdup_printf ("<b>%s: %.0f%%</b>\n<small>-&#8734; dB\n%s</small>",
                                 display_name,
                                 100 * volume / max_norm,
                                 description);
    }
  else
    {
      tooltip = g_strdup_printf ("<b>%s: %.0f%%</b>\n<small>%s</small>",
                                 display_name,
                                 100 * volume / max_norm,
                                 description);
    }

  gtk_widget_set_tooltip_markup (item, tooltip);
  g_free (tooltip);
}

static void
update_visibility (SiVolume  *self,
                   GtkWidget *item)
{
  gboolean should_show;

  should_show = FALSE;

  if (self->input)
    {
      GSList *outputs;
      GSList *l;

      outputs = gvc_mixer_control_get_source_outputs (self->control);

      for (l = outputs; l != NULL; l = l->next)
        {
          GvcMixerStream *stream;
          const char *id;

          stream = l->data;
          id = gvc_mixer_stream_get_application_id (stream);

          if (id == NULL || !g_strv_contains (skipped_apps, id))
            {
              should_show = TRUE;
              break;
            }
        }

      g_slist_free (outputs);
    }
  else
    {
      should_show = TRUE;
    }

  gtk_widget_set_visible (item, should_show);
}

static void
update_indicator (SiVolume *self)
{
  GtkWidget *menu_item;

  menu_item = si_indicator_get_menu_item (SI_INDICATOR (self));

  if (self->stream == NULL)
    {
      gtk_widget_hide (menu_item);
      return;
    }

  update_icon (self);
  update_menu (self);

  update_tooltip (self, menu_item);
  update_visibility (self, menu_item);
}

static void
notify_is_muted_cb (GObject    *object,
                    GParamSpec *pspec,
                    SiVolume   *self)
{
  update_indicator (self);
}

static void
notify_volume_cb (GObject    *object,
                  GParamSpec *pspec,
                  SiVolume   *self)
{
  update_indicator (self);
}

static gboolean
get_has_headphones (SiVolume *self)
{
  const char *form_factor;
  const GvcMixerStreamPort *port;

  form_factor = gvc_mixer_stream_get_form_factor (self->stream);

  if (g_strcmp0 (form_factor, "headset") == 0 ||
      g_strcmp0 (form_factor, "headphone") == 0)
    return TRUE;

  if (gvc_mixer_stream_get_ports (self->stream) == NULL)
    return FALSE;

  port = gvc_mixer_stream_get_port (self->stream);

  if (port != NULL && g_strstr_len (port->port, -1, "headphone") != NULL)
    return TRUE;

  return FALSE;
}

static void
update_has_headphones (SiVolume *self)
{
  gboolean has_headphones;

  has_headphones = get_has_headphones (self);
  if (self->has_headphones == has_headphones)
    return;

  self->has_headphones = has_headphones;
  update_slider_icon (self);
}

static void
notify_port_cb (GObject    *object,
                GParamSpec *pspec,
                SiVolume   *self)
{
  update_has_headphones (self);
}

static void
clear_stream (SiVolume *self)
{
  if (self->stream == NULL)
    return;

  if (self->is_muted_id != 0)
    {
      g_signal_handler_disconnect (self->stream, self->is_muted_id);
      self->is_muted_id = 0;
    }

  if (self->volume_id != 0)
    {
      g_signal_handler_disconnect (self->stream, self->volume_id);
      self->volume_id = 0;
    }

  if (self->port_id != 0)
    {
      g_signal_handler_disconnect (self->stream, self->port_id);
      self->port_id = 0;
    }

  g_clear_object (&self->stream);
}

static void
update_stream (SiVolume *self)
{
  GvcMixerStream *stream;

  clear_stream (self);

  if (self->input)
    stream = gvc_mixer_control_get_default_source (self->control);
  else
    stream = gvc_mixer_control_get_default_sink (self->control);

  if (stream != NULL)
    {
      self->stream = g_object_ref (stream);

      self->is_muted_id = g_signal_connect (self->stream,
                                            "notify::is-muted",
                                            G_CALLBACK (notify_is_muted_cb),
                                            self);

      self->volume_id = g_signal_connect (self->stream,
                                          "notify::volume",
                                          G_CALLBACK (notify_volume_cb),
                                          self);

      if (!self->input)
        {
          self->port_id = g_signal_connect (self->stream,
                                            "notify::port",
                                            G_CALLBACK (notify_port_cb),
                                            self);

          update_has_headphones (self);
        }

      update_scale_range (self);
    }

  update_indicator (self);
}

static void
state_changed_cb (GvcMixerControl      *control,
                  GvcMixerControlState  new_state,
                  SiVolume             *self)
{
  update_stream (self);
}

static void
default_source_changed_cb (GvcMixerControl *control,
                           guint            id,
                           SiVolume        *self)
{
  update_stream (self);
}

static void
stream_added_cb (GvcMixerControl *control,
                 guint            id,
                 SiVolume        *self)
{
  update_indicator (self);
}

static void
stream_removed_cb (GvcMixerControl *control,
                   guint            id,
                   SiVolume        *self)
{
  update_indicator (self);
}

static void
default_sink_changed_cb (GvcMixerControl *control,
                         guint            id,
                         SiVolume        *self)
{
  update_stream (self);
}

static gboolean
menu_item_scroll_event_cb (GtkWidget      *widget,
                           GdkEventScroll *event,
                           SiVolume       *self)
{
  GtkWidgetClass *widget_class;
  GVariantBuilder builder;
  const char *icon_name;
  double max_norm;
  double level;
  double max_level;

  widget_class = GTK_WIDGET_GET_CLASS (self->slider_scale);
  if (!widget_class->scroll_event (self->slider_scale, event))
    return FALSE;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  icon_name = get_icon (self, TRUE);
  max_norm = gvc_mixer_control_get_vol_max_norm (self->control);
  level = gvc_mixer_stream_get_volume (self->stream) / max_norm;
  max_level = get_max_level (self);

  if (icon_name != NULL)
    {
      g_variant_builder_add (&builder,
                             "{sv}",
                             "icon",
                             g_variant_new_string (icon_name));
    }

  if (level >= 0.0)
    {
      g_variant_builder_add (&builder,
                             "{sv}",
                             "level",
                             g_variant_new_double (level));
    }

  if (max_level > 1.0)
    {
      g_variant_builder_add (&builder,
                             "{sv}",
                             "max_level",
                             g_variant_new_double (max_level));
    }

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();

  gf_shell_gen_call_show_osd (self->shell,
                              g_variant_builder_end (&builder),
                              self->cancellable,
                              NULL,
                              NULL);

  return TRUE;
}

static void
prefer_symbolic_icons_cb (GObject    *object,
                          GParamSpec *pspec,
                          SiVolume   *self)
{
  update_indicator (self);
}

static void
menu_icon_size_cb (GObject    *object,
                   GParamSpec *pspec,
                   SiVolume   *self)
{
  update_slider_icon (self);
}

static void
allow_amplified_changed_cb (GSettings  *settings,
                            const char *key,
                            SiVolume   *self)
{
  gboolean allow_amplified;

  allow_amplified = g_settings_get_boolean (settings,
                                            ALLOW_AMPLIFIED_VOLUME_KEY);

  if (self->allow_amplified == allow_amplified)
    return;

  self->allow_amplified = allow_amplified;
  update_scale_range (self);
}

static void
shell_ready_cb (GObject      *object,
                GAsyncResult *res,
                gpointer      user_data)
{
  GError *error;
  GfShellGen *shell;
  SiVolume *self;

  error = NULL;
  shell = gf_shell_gen_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = SI_VOLUME (user_data);
  self->shell = shell;
}

static void
si_volume_constructed (GObject *object)
{
  SiVolume *self;
  GtkWidget *menu_item;
  GpApplet *applet;

  self = SI_VOLUME (object);

  G_OBJECT_CLASS (si_volume_parent_class)->constructed (object);

  menu_item = si_indicator_get_menu_item (SI_INDICATOR (self));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), create_menu (self));
  gtk_widget_add_events (menu_item, GDK_SCROLL_MASK);

  g_signal_connect (menu_item,
                    "scroll-event",
                    G_CALLBACK (menu_item_scroll_event_cb),
                    self);

  applet = si_indicator_get_applet (SI_INDICATOR (self));

  g_signal_connect (applet,
                    "notify::prefer-symbolic-icons",
                    G_CALLBACK (prefer_symbolic_icons_cb),
                    self);

  g_signal_connect (applet,
                    "notify::menu-icon-size",
                    G_CALLBACK (menu_icon_size_cb),
                    self);

  g_signal_connect (self->control,
                    "state-changed",
                    G_CALLBACK (state_changed_cb),
                    self);

  if (self->input)
    {
      g_signal_connect (self->control,
                        "default-source-changed",
                        G_CALLBACK (default_source_changed_cb),
                        self);

      g_signal_connect (self->control,
                        "stream-added",
                        G_CALLBACK (stream_added_cb),
                        self);

      g_signal_connect (self->control,
                        "stream-removed",
                        G_CALLBACK (stream_removed_cb),
                        self);
    }
  else
    {
      g_signal_connect (self->control,
                        "default-sink-changed",
                        G_CALLBACK (default_sink_changed_cb),
                        self);
    }

  state_changed_cb (self->control,
                    gvc_mixer_control_get_state (self->control),
                    self);
}

static void
si_volume_dispose (GObject *object)
{
  SiVolume *self;

  self = SI_VOLUME (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->shell);

  g_clear_object (&self->settings);
  clear_stream (self);

  G_OBJECT_CLASS (si_volume_parent_class)->dispose (object);
}

static void
si_volume_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  SiVolume *self;

  self = SI_VOLUME (object);

  switch (property_id)
    {
      case PROP_CONTROL:
        g_assert (self->control == NULL);
        self->control = g_value_get_object (value);
        break;

      case PROP_INPUT:
        self->input = g_value_get_boolean (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  volume_properties[PROP_CONTROL] =
    g_param_spec_object ("control",
                         "control",
                         "control",
                         GVC_TYPE_MIXER_CONTROL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  volume_properties[PROP_INPUT] =
    g_param_spec_boolean ("input",
                          "input",
                          "input",
                          FALSE,
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_WRITABLE |
                          G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     LAST_PROP,
                                     volume_properties);
}

static void
si_volume_class_init (SiVolumeClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = si_volume_constructed;
  object_class->dispose = si_volume_dispose;
  object_class->set_property = si_volume_set_property;

  install_properties (object_class);
}

static void
si_volume_init (SiVolume *self)
{
  self->settings = g_settings_new ("org.gnome.desktop.sound");

  g_signal_connect (self->settings,
                    "changed::" ALLOW_AMPLIFIED_VOLUME_KEY,
                    G_CALLBACK (allow_amplified_changed_cb),
                    self);

  self->allow_amplified = g_settings_get_boolean (self->settings,
                                                  ALLOW_AMPLIFIED_VOLUME_KEY);

  self->cancellable = g_cancellable_new ();
  gf_shell_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  "org.gnome.Shell",
                                  "/org/gnome/Shell",
                                  self->cancellable,
                                  shell_ready_cb,
                                  self);
}

SiIndicator *
si_volume_new (GpApplet        *applet,
               GvcMixerControl *control,
               gboolean         input)
{
  return g_object_new (SI_TYPE_VOLUME,
                       "applet", applet,
                       "control", control,
                       "input", input,
                       NULL);
}
