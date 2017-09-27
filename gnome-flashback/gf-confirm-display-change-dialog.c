/*
 * Copyright (C) 2017 Alberts Muktupāvels
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

#include "gf-confirm-display-change-dialog.h"

struct _GfConfirmDisplayChangeDialog
{
  GtkWindow  parent;

  gint       timeout;
  GtkWidget *description;

  guint      timeout_id;
};

enum
{
  PROP_0,

  PROP_TIMEOUT,

  LAST_PROP
};

static GParamSpec *dialog_properties[LAST_PROP] = { NULL };

enum
{
  CLOSE,
  KEEP_CHANGES,

  LAST_SIGNAL
};

static guint dialog_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfConfirmDisplayChangeDialog, gf_confirm_display_change_dialog, GTK_TYPE_WINDOW)

static void
update_description (GfConfirmDisplayChangeDialog *dialog)
{
  gchar *description;

  description = g_strdup_printf (ngettext ("Settings changes will revert in %d second!",
                                           "Settings changes will revert in %d seconds!",
                                           dialog->timeout), dialog->timeout);

  gtk_label_set_text (GTK_LABEL (dialog->description), description);
  g_free (description);
}

static gboolean
timeout_cb (gpointer user_data)
{
  GfConfirmDisplayChangeDialog *dialog;

  dialog = GF_CONFIRM_DISPLAY_CHANGE_DIALOG (user_data);

  if (dialog->timeout == 0)
    {
      dialog->timeout = 0;

      g_signal_emit (dialog, dialog_signals[KEEP_CHANGES], 0, FALSE);
      return G_SOURCE_REMOVE;
    }

  dialog->timeout--;

  update_description (dialog);
  return G_SOURCE_CONTINUE;
}

static void
keep_changes_clicked_cb (GtkButton                    *button,
                         GfConfirmDisplayChangeDialog *dialog)
{
  g_signal_emit (dialog, dialog_signals[KEEP_CHANGES], 0, TRUE);
}

static void
revert_settings_clicked_cb (GtkButton                    *button,
                            GfConfirmDisplayChangeDialog *dialog)
{
  g_signal_emit (dialog, dialog_signals[KEEP_CHANGES], 0, FALSE);
}

static void
close_cb (GfConfirmDisplayChangeDialog *dialog,
          gpointer                      user_data)
{
  g_signal_emit (dialog, dialog_signals[KEEP_CHANGES], 0, FALSE);
}

static gboolean
delete_event_cb (GfConfirmDisplayChangeDialog *dialog,
                 GdkEvent                     *event,
                 gpointer                      user_data)
{
  g_signal_emit (dialog, dialog_signals[KEEP_CHANGES], 0, FALSE);
  return TRUE;
}

static void
gf_confirm_display_change_dialog_finalize (GObject *object)
{
  GfConfirmDisplayChangeDialog *dialog;

  dialog = GF_CONFIRM_DISPLAY_CHANGE_DIALOG (object);

  if (dialog->timeout_id > 0)
    {
      g_source_remove (dialog->timeout_id);
      dialog->timeout_id = 0;
    }

  G_OBJECT_CLASS (gf_confirm_display_change_dialog_parent_class)->finalize (object);
}

static void
gf_confirm_display_change_dialog_set_property (GObject      *object,
                                               guint         property_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  GfConfirmDisplayChangeDialog *dialog;

  dialog = GF_CONFIRM_DISPLAY_CHANGE_DIALOG (object);

  switch (property_id)
    {
      case PROP_TIMEOUT:
        dialog->timeout = g_value_get_int (value);
        update_description (dialog);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_confirm_display_change_dialog_show (GtkWidget *widget)
{
  GfConfirmDisplayChangeDialog *dialog;

  dialog = GF_CONFIRM_DISPLAY_CHANGE_DIALOG (widget);
  GTK_WIDGET_CLASS (gf_confirm_display_change_dialog_parent_class)->show (widget);

  if (dialog->timeout_id == 0)
    {
      dialog->timeout_id = g_timeout_add (1000, timeout_cb, dialog);
      g_source_set_name_by_id (dialog->timeout_id, "[gnome-flashback] timeout_cb");
    }
}

static void
install_properties (GObjectClass *object_class)
{
  dialog_properties[PROP_TIMEOUT] =
    g_param_spec_int ("timeout", "timeout", "timeout",
                      G_MININT, G_MAXINT, 20,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     dialog_properties);
}

static void
install_signals (GObjectClass *object_class)
{
  dialog_signals[CLOSE] =
    g_signal_new ("close",
                  GF_TYPE_CONFIRM_DISPLAY_CHANGE_DIALOG,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  dialog_signals[KEEP_CHANGES] =
    g_signal_new ("keep-changes",
                  GF_TYPE_CONFIRM_DISPLAY_CHANGE_DIALOG,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                  G_TYPE_BOOLEAN);
}

static void
gf_confirm_display_change_dialog_class_init (GfConfirmDisplayChangeDialogClass *dialog_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;
  const gchar *resource_name;

  object_class = G_OBJECT_CLASS (dialog_class);
  widget_class = GTK_WIDGET_CLASS (dialog_class);

  object_class->finalize = gf_confirm_display_change_dialog_finalize;
  object_class->set_property = gf_confirm_display_change_dialog_set_property;

  widget_class->show = gf_confirm_display_change_dialog_show;

  install_properties (object_class);
  install_signals (object_class);

  binding_set = gtk_binding_set_by_class (dialog_class);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);

  resource_name = "/org/gnome/gnome-flashback/ui/gf-confirm-display-change-dialog.ui";
  gtk_widget_class_set_template_from_resource (widget_class, resource_name);

  gtk_widget_class_bind_template_child (widget_class, GfConfirmDisplayChangeDialog, description);

  gtk_widget_class_bind_template_callback (widget_class, keep_changes_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, revert_settings_clicked_cb);
}

static void
gf_confirm_display_change_dialog_init (GfConfirmDisplayChangeDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
  gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);

  g_signal_connect (dialog, "close", G_CALLBACK (close_cb), NULL);
  g_signal_connect (dialog, "delete-event", G_CALLBACK (delete_event_cb), NULL);
}

GtkWidget *
gf_confirm_display_change_dialog_new (gint timeout)
{
  return g_object_new (GF_TYPE_CONFIRM_DISPLAY_CHANGE_DIALOG,
                       "timeout", timeout,
                       NULL);
}
