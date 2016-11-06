/*
 * Copyright (C) 2008 Red Hat, Inc.
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

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>

#include "gf-sound-applet.h"
#include "gvc-mixer-control.h"
#include "gf-sound-item.h"

static const gchar *output_icons[] =
{
  "audio-volume-muted",
  "audio-volume-low",
  "audio-volume-medium",
  "audio-volume-high",
  NULL
};

static const gchar *input_icons[] =
{
  "microphone-sensitivity-muted",
  "microphone-sensitivity-low",
  "microphone-sensitivity-medium",
  "microphone-sensitivity-high",
  NULL
};

struct _GfSoundApplet
{
  GObject          parent;

  GvcMixerControl *control;

  GfSoundItem     *output_item;
  GfSoundItem     *input_item;
};

G_DEFINE_TYPE (GfSoundApplet, gf_sound_applet, G_TYPE_OBJECT)

static void
maybe_show_status_icons (GfSoundApplet *applet)
{
  gboolean show;
  GvcMixerStream *stream;
  GSList *source_outputs;
  GSList *l;

  show = TRUE;
  stream = gvc_mixer_control_get_default_sink (applet->control);

  if (stream == NULL)
    show = FALSE;

  if (show)
    sn_item_register (SN_ITEM (applet->output_item));
  else
    sn_item_unregister (SN_ITEM (applet->output_item));

  show = FALSE;
  stream = gvc_mixer_control_get_default_source (applet->control);
  source_outputs = gvc_mixer_control_get_source_outputs (applet->control);

  if (stream != NULL && source_outputs != NULL)
    {
      for (l = source_outputs ; l ; l = l->next)
        {
          GvcMixerStream *s;
          const gchar *id;

          s = (GvcMixerStream *) l->data;
          id = gvc_mixer_stream_get_application_id (s);

          if (id == NULL)
            {
              show = TRUE;
              break;
            }

          if (!g_str_equal (id, "org.gnome.VolumeControl") &&
              !g_str_equal (id, "org.PulseAudio.pavucontrol"))
            {
              show = TRUE;
              break;
            }
        }
    }

  if (show)
    sn_item_register (SN_ITEM (applet->input_item));
  else
    sn_item_unregister (SN_ITEM (applet->input_item));

  g_slist_free (source_outputs);
}

static void
update_default_sink (GfSoundApplet *applet)
{
  GvcMixerStream *stream;

  stream = gvc_mixer_control_get_default_sink (applet->control);

  if (stream != NULL)
    {
      gf_sound_item_set_mixer_stream (applet->output_item, stream);
      maybe_show_status_icons (applet);
    }
  else
    {
      g_warning ("Unable to get default sink");
    }
}

static void
update_default_source (GfSoundApplet *applet)
{
  GvcMixerStream *stream;

  stream = gvc_mixer_control_get_default_source (applet->control);

  if (stream != NULL)
    {
      gf_sound_item_set_mixer_stream (applet->input_item, stream);
      maybe_show_status_icons (applet);
    }
  else
    {
      g_debug ("Unable to get default source, or no source available");
    }
}

static void
on_control_state_changed (GvcMixerControl      *control,
                          GvcMixerControlState  new_state,
                          gpointer              user_data)
{
  GfSoundApplet *applet;

  applet = GF_SOUND_APPLET (user_data);

  if (new_state == GVC_STATE_READY)
    {
      update_default_sink (applet);
      update_default_source (applet);
    }
  else
    {
      g_debug ("Connecting...");
    }
}

static void
on_control_default_sink_changed (GvcMixerControl *control,
                                 guint            id,
                                 gpointer         user_data)
{
  GfSoundApplet *applet;

  applet = GF_SOUND_APPLET (user_data);

  maybe_show_status_icons (applet);
}

static void
on_control_default_source_changed (GvcMixerControl *control,
                                   guint            id,
                                   gpointer         user_data)
{
  GfSoundApplet *applet;

  applet = GF_SOUND_APPLET (user_data);

  maybe_show_status_icons (applet);
}

static void
on_control_stream_added (GvcMixerControl *control,
                         guint            id,
                         gpointer         user_data)
{
  GfSoundApplet *applet;

  applet = GF_SOUND_APPLET (user_data);

  maybe_show_status_icons (applet);
}

static void
on_control_stream_removed (GvcMixerControl *control,
                           guint            id,
                           gpointer         user_data)
{
  GfSoundApplet *applet;

  applet = GF_SOUND_APPLET (user_data);

  maybe_show_status_icons (applet);
}

static void
gf_sound_applet_constructed (GObject *object)
{
  GfSoundApplet *applet;

  applet = GF_SOUND_APPLET (object);

  G_OBJECT_CLASS (gf_sound_applet_parent_class)->constructed (object);

  applet->control = gvc_mixer_control_new ("GNOME Volume Control Applet");

  g_signal_connect (applet->control, "state-changed",
                    G_CALLBACK (on_control_state_changed), applet);
  g_signal_connect (applet->control, "default-sink-changed",
                    G_CALLBACK (on_control_default_sink_changed), applet);
  g_signal_connect (applet->control, "default-source-changed",
                    G_CALLBACK (on_control_default_source_changed), applet);
  g_signal_connect (applet->control, "stream-added",
                    G_CALLBACK (on_control_stream_added), applet);
  g_signal_connect (applet->control, "stream-removed",
                    G_CALLBACK (on_control_stream_removed), applet);

  gvc_mixer_control_open (applet->control);

  maybe_show_status_icons (applet);
}

static void
gf_sound_applet_dispose (GObject *object)
{
  GfSoundApplet *applet;

  applet = GF_SOUND_APPLET (object);

  g_clear_object (&applet->control);

  g_clear_object (&applet->output_item);
  g_clear_object (&applet->input_item);

  G_OBJECT_CLASS (gf_sound_applet_parent_class)->dispose (object);
}

static void
gf_sound_applet_class_init (GfSoundAppletClass *applet_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (applet_class);

  object_class->constructed = gf_sound_applet_constructed;
  object_class->dispose = gf_sound_applet_dispose;
}

static void
gf_sound_applet_init (GfSoundApplet *applet)
{
  SnItemCategory category;

  category = SN_ITEM_CATEGORY_HARDWARE;

  applet->output_item = gf_sound_item_new (category,
                                           "gf-sound-output",
                                           _("Sound Output Volume"),
                                           _("Output"),
                                           output_icons);

  applet->input_item = gf_sound_item_new (category,
                                          "gf-sound-input",
                                          _("Microphone Volume"),
                                          _("Input"),
                                          input_icons);
}

GfSoundApplet *
gf_sound_applet_new (void)
{
  return g_object_new (GF_TYPE_SOUND_APPLET, NULL);
}
