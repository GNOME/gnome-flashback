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
#include "gf-create-folder-dialog.h"

#include <glib/gi18n.h>

struct _GfCreateFolderDialog
{
  GtkDialog  parent;

  GtkWidget *create_button;

  GtkWidget *name_entry;

  GtkWidget *error_revealer;
  GtkWidget *error_label;
};

enum
{
  VALIDATE,

  LAST_SIGNAL
};

static guint dialog_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfCreateFolderDialog, gf_create_folder_dialog, GTK_TYPE_DIALOG)

static void
cancel_clicked_cb (GtkWidget            *widget,
                   GfCreateFolderDialog *self)
{
  gtk_widget_destroy (GTK_WIDGET (self));
}

static void
create_clicked_cb (GtkWidget            *widget,
                   GfCreateFolderDialog *self)
{
  gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_ACCEPT);
}

static void
name_changed_cb (GtkEditable          *editable,
                 GfCreateFolderDialog *self)
{
  GtkRevealer *revealer;
  const char *text;
  char *folder_name;
  char *validate_error;
  const char *error;
  gboolean valid;

  revealer = GTK_REVEALER (self->error_revealer);
  text = gtk_entry_get_text (GTK_ENTRY (self->name_entry));
  folder_name = g_strdup (text);
  error = NULL;
  valid = TRUE;

  folder_name = g_strstrip (folder_name);
  validate_error = NULL;

  if (*folder_name == '\0')
    {
      error = NULL;
      valid = FALSE;
    }
  else if (g_strstr_len (folder_name, -1, "/") != NULL)
    {
      error = _("Folder names cannot contain “/”.");
      valid = FALSE;
    }
  else if (g_strcmp0 (folder_name, ".") == 0)
    {
      error = _("A folder cannot be called “.”.");
      valid = FALSE;
    }
  else if (g_strcmp0 (folder_name, "..") == 0)
    {
      error = _("A folder cannot be called “..”.");
      valid = FALSE;
    }

  if (valid)
    {
      g_assert_true (error == NULL);

      g_signal_emit (self, dialog_signals[VALIDATE], 0,
                     folder_name, &validate_error);

      if (validate_error != NULL)
        {
          error = validate_error;
          valid = FALSE;
        }
    }

  if (error == NULL &&
      g_str_has_prefix (folder_name, "."))
    {
      error = _("Folders with “.” at the beginning of their name are hidden.");
    }

  gtk_label_set_text (GTK_LABEL (self->error_label), error);
  gtk_revealer_set_reveal_child (revealer, error != NULL);

  gtk_widget_set_sensitive (self->create_button, valid);

  g_free (validate_error);
  g_free (folder_name);
}

static void
name_activate_cb (GtkWidget            *widget,
                  GfCreateFolderDialog *self)
{
  if (!gtk_widget_get_sensitive (self->create_button))
    return;

  gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_ACCEPT);
}

static void
setup_header_bar (GfCreateFolderDialog *self)
{
  GtkWidget *header_bar;
  GtkWidget *cancel_button;
  GtkStyleContext *style;

  header_bar = gtk_dialog_get_header_bar (GTK_DIALOG (self));
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header_bar), FALSE);
  gtk_header_bar_set_title (GTK_HEADER_BAR (header_bar), _("New Folder"));

  cancel_button = gtk_button_new_with_label (_("Cancel"));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), cancel_button);
  gtk_widget_show (cancel_button);

  g_signal_connect (cancel_button, "clicked",
                    G_CALLBACK (cancel_clicked_cb),
                    self);

  self->create_button = gtk_button_new_with_label (_("Create"));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header_bar), self->create_button);
  gtk_widget_show (self->create_button);

  style = gtk_widget_get_style_context (self->create_button);
  gtk_style_context_add_class (style, GTK_STYLE_CLASS_SUGGESTED_ACTION);

  gtk_widget_set_sensitive (self->create_button, FALSE);

  g_signal_connect (self->create_button, "clicked",
                    G_CALLBACK (create_clicked_cb),
                    self);
}

static void
setup_content_area (GfCreateFolderDialog *self)
{
  GtkWidget *content;
  GtkWidget *label;

  content = gtk_dialog_get_content_area (GTK_DIALOG (self));

  g_object_set (content,
                "margin", 18,
                "margin-bottom", 12,
                "spacing", 6,
                NULL);

  label = gtk_label_new (_("Folder name"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_container_add (GTK_CONTAINER (content), label);
  gtk_widget_show (label);

  self->name_entry = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (content), self->name_entry);
  gtk_widget_show (self->name_entry);

  g_signal_connect (self->name_entry, "changed",
                    G_CALLBACK (name_changed_cb),
                    self);

  g_signal_connect (self->name_entry, "activate",
                    G_CALLBACK (name_activate_cb),
                    self);

  self->error_revealer = gtk_revealer_new ();
  gtk_container_add (GTK_CONTAINER (content), self->error_revealer);
  gtk_widget_show (self->error_revealer);

  self->error_label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (self->error_label), 0.0);
  gtk_container_add (GTK_CONTAINER (self->error_revealer), self->error_label);
  gtk_widget_show (self->error_label);
}

static void
install_signals (void)
{
  dialog_signals[VALIDATE] =
    g_signal_new ("validate", GF_TYPE_CREATE_FOLDER_DIALOG,
                  G_SIGNAL_RUN_LAST, 0,
                  g_signal_accumulator_first_wins,
                  NULL, NULL,
                  G_TYPE_STRING, 1, G_TYPE_STRING);
}

static void
gf_create_folder_dialog_class_init (GfCreateFolderDialogClass *self_class)
{
  install_signals ();
}

static void
gf_create_folder_dialog_init (GfCreateFolderDialog *self)
{
  setup_header_bar (self);
  setup_content_area (self);
}

GtkWidget *
gf_create_folder_dialog_new (void)
{
  return g_object_new (GF_TYPE_CREATE_FOLDER_DIALOG,
                       "use-header-bar", TRUE,
                       "width-request", 450,
                       "resizable", FALSE,
                       NULL);
}

char *
gf_create_folder_dialog_get_folder_name (GfCreateFolderDialog *self)
{
  const char *text;
  char *folder_name;

  text = gtk_entry_get_text (GTK_ENTRY (self->name_entry));
  folder_name = g_strdup (text);

  return g_strstrip (folder_name);
}
