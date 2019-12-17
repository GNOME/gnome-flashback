/*
 * Copyright (C) 2016 Alberts Muktupāvels
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
#include "gf-audio-device-selection.h"

#include "dbus/gf-audio-device-selection-gen.h"
#include "gf-audio-device-selection-dialog.h"

#define AUDIO_DEVICE_SELECTION_DBUS_NAME "org.gnome.Shell.AudioDeviceSelection"
#define AUDIO_DEVICE_SELECTION_DBUS_PATH "/org/gnome/Shell/AudioDeviceSelection"

struct _GfAudioDeviceSelection
{
  GObject                    parent;

  GfAudioDeviceSelectionGen *selection;
  gint                       bus_name_id;

  GtkWidget                 *dialog;
};

G_DEFINE_TYPE (GfAudioDeviceSelection, gf_audio_device_selection, G_TYPE_OBJECT)

static void
close_cb (GfAudioDeviceSelectionDialog *dialog,
          GfAudioDeviceSelection       *ads)
{
  gtk_widget_destroy (ads->dialog);
  ads->dialog = NULL;
}

static void
selected_cb (GfAudioDeviceSelectionDialog *dialog,
             const gchar                  *device,
             GfAudioDeviceSelection       *ads)
{
  gf_audio_device_selection_gen_emit_device_selected (ads->selection, device);
  close_cb (dialog, ads);
}

static gboolean
delete_event_cb (GtkWidget              *widget,
                 GdkEvent               *event,
                 GfAudioDeviceSelection *ads)
{
  ads->dialog = NULL;

  return GDK_EVENT_PROPAGATE;
}

static gboolean
handle_close_cb (GfAudioDeviceSelectionGen *object,
                 GDBusMethodInvocation     *invocation,
                 GfAudioDeviceSelection    *ads)
{
  if (ads->dialog)
    {
      GfAudioDeviceSelectionDialog *dialog;
      const gchar *invocation_sender;
      const gchar *dialog_sender;

      dialog = GF_AUDIO_DEVICE_SELECTION_DIALOG (ads->dialog);

      invocation_sender = g_dbus_method_invocation_get_sender (invocation);
      dialog_sender = gf_audio_device_selection_dialog_get_sender (dialog);

      if (g_strcmp0 (invocation_sender, dialog_sender) == 0)
        {
          gtk_widget_destroy (ads->dialog);
          ads->dialog = NULL;
        }
    }

  gf_audio_device_selection_gen_complete_close (object, invocation);

  return TRUE;
}

static gboolean
handle_open_cb (GfAudioDeviceSelectionGen *object,
                GDBusMethodInvocation     *invocation,
                const gchar *const        *devices,
                GfAudioDeviceSelection    *ads)
{
  const gchar *sender;

  if (ads->dialog)
    {
      gf_audio_device_selection_gen_complete_open (object, invocation);

      return TRUE;
    }

  sender = g_dbus_method_invocation_get_sender (invocation);
  ads->dialog = gf_audio_device_selection_dialog_new (sender, devices);

  g_signal_connect (ads->dialog, "close", G_CALLBACK (close_cb), ads);
  g_signal_connect (ads->dialog, "selected", G_CALLBACK (selected_cb), ads);

  g_signal_connect (ads->dialog, "delete-event",
                    G_CALLBACK (delete_event_cb), ads);

  gtk_window_present (GTK_WINDOW (ads->dialog));

  gf_audio_device_selection_gen_complete_open (object, invocation);

  return TRUE;
}

static void
bus_acquired_handler (GDBusConnection *connection,
                      const gchar     *name,
                      gpointer         user_data)
{
  GfAudioDeviceSelection *ads;
  GDBusInterfaceSkeleton *skeleton;
  GError *error;
  gboolean exported;

  ads = GF_AUDIO_DEVICE_SELECTION (user_data);
  skeleton = G_DBUS_INTERFACE_SKELETON (ads->selection);

  g_signal_connect (ads->selection, "handle-close",
                    G_CALLBACK (handle_close_cb), ads);
  g_signal_connect (ads->selection, "handle-open",
                    G_CALLBACK (handle_open_cb), ads);

  error = NULL;
  exported = g_dbus_interface_skeleton_export (skeleton, connection,
                                               AUDIO_DEVICE_SELECTION_DBUS_PATH,
                                               &error);

  if (!exported)
    {
      g_warning ("Failed to export interface: %s", error->message);
      g_error_free (error);

      return;
    }
}

static void
gf_audio_device_selection_dispose (GObject *object)
{
  GfAudioDeviceSelection *ads;
  GDBusInterfaceSkeleton *skeleton;

  ads = GF_AUDIO_DEVICE_SELECTION (object);

  if (ads->bus_name_id)
    {
      g_bus_unown_name (ads->bus_name_id);
      ads->bus_name_id = 0;
    }

  if (ads->selection)
    {
      skeleton = G_DBUS_INTERFACE_SKELETON (ads->selection);

      g_dbus_interface_skeleton_unexport (skeleton);
      g_clear_object (&ads->selection);
    }

  if (ads->dialog)
    {
      gtk_widget_destroy (ads->dialog);
      ads->dialog = NULL;
    }

  G_OBJECT_CLASS (gf_audio_device_selection_parent_class)->dispose (object);
}

static void
gf_audio_device_selection_class_init (GfAudioDeviceSelectionClass *ads_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (ads_class);

  object_class->dispose = gf_audio_device_selection_dispose;
}

static void
gf_audio_device_selection_init (GfAudioDeviceSelection *ads)
{
  GBusNameOwnerFlags flags;

  flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
  flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

  ads->selection = gf_audio_device_selection_gen_skeleton_new ();
  ads->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                     AUDIO_DEVICE_SELECTION_DBUS_NAME,
                                     flags, bus_acquired_handler,
                                     NULL, NULL, ads, NULL);
}

GfAudioDeviceSelection *
gf_audio_device_selection_new (void)
{
  return g_object_new (GF_TYPE_AUDIO_DEVICE_SELECTION, NULL);
}
