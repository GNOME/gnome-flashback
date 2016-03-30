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

#include <glib/gi18n.h>

#include "gf-audio-device-selection-dialog.h"

typedef enum
{
  GF_AUDIO_DEVICE_HEADPHONES = 1 << 0,
  GF_AUDIO_DEVICE_HEADSET    = 1 << 1,
  GF_AUDIO_DEVICE_MICROPHONE = 1 << 2
} GfAudioDevice;

struct _GfAudioDeviceSelectionDialog
{
  GtkWindow  parent;

  gchar     *sender;

  GtkWidget *selection_box;
};

enum
{
  CLOSE,
  SELECTED,

  LAST_SIGNAL
};

static guint dialog_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfAudioDeviceSelectionDialog, gf_audio_device_selection_dialog, GTK_TYPE_WINDOW)

static const gchar *
get_device_from_enum (GfAudioDevice device)
{
  switch (device)
    {
      case GF_AUDIO_DEVICE_HEADPHONES:
        return "headphones";

      case GF_AUDIO_DEVICE_HEADSET:
        return "headset";

      case GF_AUDIO_DEVICE_MICROPHONE:
        return "microphone";

      default:
        return NULL;
    }
}

static void
clicked_cb (GtkButton                    *button,
            GfAudioDeviceSelectionDialog *dialog)
{
  gpointer tmp;
  const gchar *audio_device;

  tmp = g_object_get_data (G_OBJECT (button), "device");
  audio_device = get_device_from_enum (GPOINTER_TO_INT (tmp));

  g_signal_emit (dialog, dialog_signals[SELECTED], 0, audio_device);
}

static const gchar *
get_device_icon (GfAudioDevice device)
{
  switch (device)
    {
      case GF_AUDIO_DEVICE_HEADPHONES:
        return "audio-headphones";

      case GF_AUDIO_DEVICE_HEADSET:
        return "audio-headset";

      case GF_AUDIO_DEVICE_MICROPHONE:
        return "audio-input-microphone";

      default:
        return NULL;
    }
}

static const gchar *
get_device_label (GfAudioDevice device)
{
  switch (device)
    {
      case GF_AUDIO_DEVICE_HEADPHONES:
        return _("Headphones");

      case GF_AUDIO_DEVICE_HEADSET:
        return _("Headset");

      case GF_AUDIO_DEVICE_MICROPHONE:
        return _("Microphone");

      default:
        return NULL;
    }
}

static void
add_device (GfAudioDeviceSelectionDialog *dialog,
            GfAudioDevice                 device)
{
  const gchar *icon;
  const gchar *label;
  GtkWidget *button;
  GtkWidget *image;

  icon = get_device_icon (device);
  label = get_device_label (device);

  button = gtk_button_new_with_label (label);
  gtk_box_pack_start (GTK_BOX (dialog->selection_box), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  g_object_set_data (G_OBJECT (button), "device", GINT_TO_POINTER (device));

  image = gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_DIALOG);
  gtk_widget_show (image);

  gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);
  gtk_button_set_image_position (GTK_BUTTON (button), GTK_POS_TOP);
  gtk_button_set_image (GTK_BUTTON (button), image);

  g_signal_connect (button, "clicked", G_CALLBACK (clicked_cb), dialog);
}

static void
settings_clicked_cb (GtkButton                    *button,
                     GfAudioDeviceSelectionDialog *dialog)
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
    {
      g_signal_emit (dialog, dialog_signals[CLOSE], 0);
      g_app_info_launch (app_info, NULL, G_APP_LAUNCH_CONTEXT (context), &error);
    }

  if (error != NULL)
    {
      g_warning (_("Failed to start Sound Preferences: %s"), error->message);
      g_error_free (error);
    }

  g_clear_object (&app_info);
  g_object_unref (context);
}

static void
cancel_clicked_cb (GtkButton                    *button,
                   GfAudioDeviceSelectionDialog *dialog)
{
  g_signal_emit (dialog, dialog_signals[CLOSE], 0);
}

static void
gf_audio_device_selection_dialog_finalize (GObject *object)
{
  GfAudioDeviceSelectionDialog *dialog;

  dialog = GF_AUDIO_DEVICE_SELECTION_DIALOG (object);

  g_free (dialog->sender);

  G_OBJECT_CLASS (gf_audio_device_selection_dialog_parent_class)->finalize (object);
}

static void
gf_audio_device_selection_dialog_class_init (GfAudioDeviceSelectionDialogClass *dialog_class)
{
  GObjectClass *object_class;
  GtkBindingSet *binding_set;

  object_class = G_OBJECT_CLASS (dialog_class);

  object_class->finalize = gf_audio_device_selection_dialog_finalize;

  dialog_signals[CLOSE] =
    g_signal_new ("close", G_OBJECT_CLASS_TYPE (dialog_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  dialog_signals[SELECTED] =
    g_signal_new ("selected", G_OBJECT_CLASS_TYPE (dialog_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  binding_set = gtk_binding_set_by_class (dialog_class);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);
}

static void
gf_audio_device_selection_dialog_init (GfAudioDeviceSelectionDialog *dialog)
{
  GtkWidget *vbox1;
  GtkWidget *vbox2;
  GtkWidget *label;
  GtkWidget *button_box;
  GtkWidget *button;

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);

  vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_add (GTK_CONTAINER (dialog), vbox1);
  gtk_widget_show (vbox1);

  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 6);
  gtk_widget_show (vbox2);

  label = gtk_label_new (_("What kind of device did you plug in?"));
  gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  dialog->selection_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (vbox2), dialog->selection_box, TRUE, TRUE, 0);
  gtk_widget_show (dialog->selection_box);

  gtk_box_set_homogeneous (GTK_BOX (dialog->selection_box), TRUE);

  button_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (vbox1), button_box, FALSE, FALSE, 0);
  gtk_widget_show (button_box);

  button = gtk_button_new_with_label (_("Sound Settings"));
  gtk_box_pack_start (GTK_BOX (button_box), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (settings_clicked_cb), dialog);

  button = gtk_button_new_with_label (_("Cancel"));
  gtk_box_pack_start (GTK_BOX (button_box), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (cancel_clicked_cb), dialog);
}

GtkWidget *
gf_audio_device_selection_dialog_new (const gchar        *sender,
                                      const gchar *const *devices)
{
  GfAudioDeviceSelectionDialog *dialog;
  gint i;

  dialog = g_object_new (GF_TYPE_AUDIO_DEVICE_SELECTION_DIALOG,
                         "title", _("Unknown Audio Device"),
                         "window-position", GTK_WIN_POS_CENTER,
                         "resizable", FALSE,
                         NULL);

  dialog->sender = g_strdup (sender);

  for (i = 0; devices[i] != NULL; i++)
    {
      GfAudioDevice device;

      if (g_strcmp0 (devices[i], "headphones") == 0)
        device = GF_AUDIO_DEVICE_HEADPHONES;
      else if (g_strcmp0 (devices[i], "headset") == 0)
        device = GF_AUDIO_DEVICE_HEADSET;
      else if (g_strcmp0 (devices[i], "microphone") == 0)
        device = GF_AUDIO_DEVICE_MICROPHONE;
      else
        g_assert_not_reached ();

      add_device (dialog, device);
    }

  return GTK_WIDGET (dialog);
}

const gchar *
gf_audio_device_selection_dialog_get_sender (GfAudioDeviceSelectionDialog *dialog)
{
  return dialog->sender;
}
