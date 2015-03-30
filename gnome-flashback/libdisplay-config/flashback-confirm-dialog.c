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

#include <config.h>
#include <glib/gi18n.h>
#include "flashback-confirm-dialog.h"

typedef struct _FlashbackConfirmDialogPrivate FlashbackConfirmDialogPrivate;

struct _FlashbackConfirmDialogPrivate
{
  GtkWidget *title;
  GtkWidget *description;

  GtkWidget *revert_settings;
  GtkWidget *keep_changes;

  gint       timeout;
  guint      timeout_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (FlashbackConfirmDialog, flashback_confirm_dialog, GTK_TYPE_WINDOW)

enum
{
  SIGNAL_RESPONSE,
  SIGNAL_CLOSE,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0, };

static void
update_text (FlashbackConfirmDialog *dialog)
{
  FlashbackConfirmDialogPrivate *priv;
  const gchar *text;
  gchar *description;

  priv = flashback_confirm_dialog_get_instance_private (dialog);
  text = ngettext ("Settings changes will revert in %d second",
                   "Settings changes will revert in %d seconds",
                   priv->timeout);

  description = g_strdup_printf (text, priv->timeout);
  gtk_label_set_text (GTK_LABEL (priv->description), description);
  g_free (description);
}

static gboolean
timeout_cb (gpointer user_data)
{
  FlashbackConfirmDialog *dialog;
  FlashbackConfirmDialogPrivate *priv;

  dialog = FLASHBACK_CONFIRM_DIALOG (user_data);
  priv = flashback_confirm_dialog_get_instance_private (dialog);

  if (priv->timeout == 0)
    {
      priv->timeout_id = 0;
      g_signal_emit (dialog, signals[SIGNAL_RESPONSE], 0,
                     FLASHBACK_CONFIRM_DIALOG_RESPONSE_REVERT_SETTINGS);
      return FALSE;
    }

  priv->timeout--;
  update_text (dialog);

  return TRUE;
}

static void
revert_settings_clicked_cb (FlashbackConfirmDialog *dialog,
                            GtkButton              *button)
{
  g_signal_emit (dialog, signals[SIGNAL_RESPONSE], 0,
                 FLASHBACK_CONFIRM_DIALOG_RESPONSE_REVERT_SETTINGS);
}

static void
keep_changes_clicked_cb (FlashbackConfirmDialog *dialog,
                         GtkButton              *button)
{
  g_signal_emit (dialog, signals[SIGNAL_RESPONSE], 0,
                 FLASHBACK_CONFIRM_DIALOG_RESPONSE_KEEP_CHANGES);
}

static void
flashback_confirm_dialog_close (FlashbackConfirmDialog *dialog)
{
  g_signal_emit (dialog, signals[SIGNAL_RESPONSE], 0,
                 FLASHBACK_CONFIRM_DIALOG_RESPONSE_REVERT_SETTINGS);
}

static void
flashback_confirm_dialog_finalize (GObject *object)
{
  FlashbackConfirmDialog *dialog;
  FlashbackConfirmDialogPrivate *priv;

  dialog = FLASHBACK_CONFIRM_DIALOG (object);
  priv = flashback_confirm_dialog_get_instance_private (dialog);

  if (priv->timeout_id > 0)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }

  G_OBJECT_CLASS (flashback_confirm_dialog_parent_class)->finalize (object);
}

static void
flashback_confirm_dialog_destroy (GtkWidget *widget)
{
  FlashbackConfirmDialog *dialog;
  FlashbackConfirmDialogPrivate *priv;

  dialog = FLASHBACK_CONFIRM_DIALOG (widget);
  priv = flashback_confirm_dialog_get_instance_private (dialog);

  if (priv->timeout_id > 0)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }

  GTK_WIDGET_CLASS (flashback_confirm_dialog_parent_class)->destroy (widget);
}

static void
flashback_confirm_dialog_show (GtkWidget *widget)
{
  FlashbackConfirmDialog *dialog;
  FlashbackConfirmDialogPrivate *priv;

  dialog = FLASHBACK_CONFIRM_DIALOG (widget);
  priv = flashback_confirm_dialog_get_instance_private (dialog);

  GTK_WIDGET_CLASS (flashback_confirm_dialog_parent_class)->show (widget);

  if (priv->timeout_id == 0)
    priv->timeout_id = g_timeout_add (1000, (GSourceFunc) timeout_cb, dialog);
}

static void
flashback_confirm_dialog_class_init (FlashbackConfirmDialogClass *dialog_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;

  widget_class = GTK_WIDGET_CLASS (dialog_class);

  dialog_class->close = flashback_confirm_dialog_close;

  object_class->finalize = flashback_confirm_dialog_finalize;

  widget_class->destroy = flashback_confirm_dialog_destroy;
  widget_class->show = flashback_confirm_dialog_show;

  signals[SIGNAL_RESPONSE] =
    g_signal_new ("response",
                  G_OBJECT_CLASS_TYPE (dialog_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (FlashbackConfirmDialogClass, response),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_INT);
  signals[SIGNAL_CLOSE] =
    g_signal_new ("close",
                  G_OBJECT_CLASS_TYPE (dialog_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (FlashbackConfirmDialogClass, close),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  binding_set = gtk_binding_set_by_class (dialog_class);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gnome-flashback/flashback-confirm-dialog.ui");

  gtk_widget_class_bind_template_child_private (widget_class, FlashbackConfirmDialog, title);
  gtk_widget_class_bind_template_child_private (widget_class, FlashbackConfirmDialog, description);
  gtk_widget_class_bind_template_child_private (widget_class, FlashbackConfirmDialog, revert_settings);
  gtk_widget_class_bind_template_child_private (widget_class, FlashbackConfirmDialog, keep_changes);

  gtk_widget_class_bind_template_callback (widget_class, revert_settings_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, keep_changes_clicked_cb);
}

static void
flashback_confirm_dialog_init (FlashbackConfirmDialog *dialog)
{
  GtkWindow *window;
  GtkWidget *widget;

  widget = GTK_WIDGET (dialog);
  window = GTK_WINDOW (dialog);

  gtk_widget_init_template (widget);

  gtk_window_set_keep_above (window, TRUE);
  gtk_window_set_skip_taskbar_hint (window, TRUE);
  gtk_window_set_skip_pager_hint (window, TRUE);
}

GtkWidget *
flashback_confirm_dialog_new (gint timeout)
{
  FlashbackConfirmDialog *dialog;
  FlashbackConfirmDialogPrivate *priv;

  dialog = g_object_new (FLASHBACK_TYPE_CONFIRM_DIALOG, NULL);
  priv = flashback_confirm_dialog_get_instance_private (dialog);

  priv->timeout = timeout;
  update_text (dialog);

  return GTK_WIDGET (dialog);
}
