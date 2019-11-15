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
#include "gf-rename-popover.h"

#include <glib/gi18n.h>

struct _GfRenamePopover
{
  GtkPopover  parent;

  GFileType   file_type;
  char       *name;

  GtkWidget  *title_label;

  GtkWidget  *rename_button;

  GtkWidget  *name_entry;

  GtkWidget  *error_revealer;
  GtkWidget  *error_label;
};

enum
{
  PROP_0,

  PROP_FILE_TYPE,
  PROP_NAME,

  LAST_PROP
};

static GParamSpec *popover_properties[LAST_PROP] = { NULL };

enum
{
  VALIDATE,

  DO_RENAME,

  LAST_SIGNAL
};

static guint popover_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfRenamePopover, gf_rename_popover, GTK_TYPE_POPOVER)

static void
validate (GfRenamePopover *self)
{
  const char *text;

  text = gtk_entry_get_text (GTK_ENTRY (self->name_entry));
  g_signal_emit (self, popover_signals[VALIDATE], 0, text);
}

static void
name_changed_cb (GtkEditable     *editable,
                 GfRenamePopover *self)
{
  validate (self);
}

static void
name_activate_cb (GtkWidget       *widget,
                  GfRenamePopover *self)
{
  validate (self);

  if (!gtk_widget_get_sensitive (self->rename_button))
    return;

  g_signal_emit (self, popover_signals[DO_RENAME], 0);
}

static void
rename_clicked_cb (GtkWidget       *widget,
                   GfRenamePopover *self)
{
  validate (self);

  if (!gtk_widget_get_sensitive (self->rename_button))
    return;

  g_signal_emit (self, popover_signals[DO_RENAME], 0);
}

static void
gf_rename_popover_constructed (GObject *object)
{
  GfRenamePopover *self;
  gboolean is_dir;
  const char *title;

  self = GF_RENAME_POPOVER (object);

  G_OBJECT_CLASS (gf_rename_popover_parent_class)->constructed (object);

  is_dir = self->file_type == G_FILE_TYPE_DIRECTORY;
  title = is_dir ? _("Folder name") : _("File name");

  gtk_label_set_text (GTK_LABEL (self->title_label), title);
  gtk_entry_set_text (GTK_ENTRY (self->name_entry), self->name);

  g_signal_connect (self->name_entry, "changed",
                    G_CALLBACK (name_changed_cb),
                    self);
}

static void
gf_rename_popover_finalize (GObject *object)
{
  GfRenamePopover *self;

  self = GF_RENAME_POPOVER (object);

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gf_rename_popover_parent_class)->finalize (object);
}

static void
gf_rename_popover_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GfRenamePopover *self;

  self = GF_RENAME_POPOVER (object);

  switch (property_id)
    {
      case PROP_FILE_TYPE:
        self->file_type = g_value_get_enum (value);
        break;

      case PROP_NAME:
        g_assert (self->name == NULL);
        self->name = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  popover_properties[PROP_FILE_TYPE] =
    g_param_spec_enum ("file-type",
                       "file-type",
                       "file-type",
                       G_TYPE_FILE_TYPE,
                       G_FILE_TYPE_UNKNOWN,
                       G_PARAM_CONSTRUCT_ONLY |
                       G_PARAM_WRITABLE |
                       G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);

  popover_properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "name",
                         "name",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     popover_properties);
}

static void
install_signals (void)
{
  popover_signals[VALIDATE] =
    g_signal_new ("validate", GF_TYPE_RENAME_POPOVER,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  popover_signals[DO_RENAME] =
    g_signal_new ("do-rename", GF_TYPE_RENAME_POPOVER,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
gf_rename_popover_class_init (GfRenamePopoverClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = gf_rename_popover_constructed;
  object_class->finalize = gf_rename_popover_finalize;
  object_class->set_property = gf_rename_popover_set_property;

  install_properties (object_class);
  install_signals ();
}

static void
gf_rename_popover_init (GfRenamePopover *self)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkStyleContext *style;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  g_object_set (vbox, "margin", 10, NULL);
  gtk_container_add (GTK_CONTAINER (self), vbox);
  gtk_widget_show (vbox);

  self->title_label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (self->title_label), 0.0);
  gtk_container_add (GTK_CONTAINER (vbox), self->title_label);
  gtk_widget_show (self->title_label);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_widget_show (hbox);

  self->name_entry = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (hbox), self->name_entry);
  gtk_widget_show (self->name_entry);

  g_signal_connect (self->name_entry, "activate",
                    G_CALLBACK (name_activate_cb),
                    self);

  self->rename_button = gtk_button_new_with_label (_("Rename"));
  gtk_container_add (GTK_CONTAINER (hbox), self->rename_button);
  gtk_widget_show (self->rename_button);

  style = gtk_widget_get_style_context (self->rename_button);
  gtk_style_context_add_class (style, GTK_STYLE_CLASS_SUGGESTED_ACTION);

  g_signal_connect (self->rename_button, "clicked",
                    G_CALLBACK (rename_clicked_cb),
                    self);

  self->error_revealer = gtk_revealer_new ();
  gtk_container_add (GTK_CONTAINER (vbox), self->error_revealer);
  gtk_widget_show (self->error_revealer);

  self->error_label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (self->error_label), 0.0);
  gtk_container_add (GTK_CONTAINER (self->error_revealer), self->error_label);
  gtk_widget_show (self->error_label);
}

GtkWidget *
gf_rename_popover_new (GtkWidget  *relative_to,
                       GFileType   file_type,
                       const char *name)
{
  return g_object_new (GF_TYPE_RENAME_POPOVER,
                       "relative-to", relative_to,
                       "file-type", file_type,
                       "name", name,
                       NULL);
}

void
gf_rename_popover_set_valid (GfRenamePopover *self,
                             gboolean         valid,
                             const char      *message)
{
  GtkRevealer *revealer;

  revealer = GTK_REVEALER (self->error_revealer);

  gtk_label_set_text (GTK_LABEL (self->error_label), message);
  gtk_revealer_set_reveal_child (revealer, message != NULL);

  gtk_widget_set_sensitive (self->rename_button, valid);
}

char *
gf_rename_popover_get_name (GfRenamePopover *self)
{
  const char *text;
  char *name;

  text = gtk_entry_get_text (GTK_ENTRY (self->name_entry));
  name = g_strdup (text);

  return g_strstrip (name);
}
