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
#include "gf-icon.h"

#include "gf-desktop-enums.h"
#include "gf-desktop-enum-types.h"

struct _GfIcon
{
  GtkButton   parent;

  GFile      *file;
  GFileInfo  *info;

  GfIconSize  icon_size;
  guint       extra_text_width;

  GtkWidget  *image;
  GtkWidget  *label;

  gboolean    selected;
};

enum
{
  PROP_0,

  PROP_FILE,
  PROP_INFO,

  PROP_ICON_SIZE,
  PROP_EXTRA_TEXT_WIDTH,

  LAST_PROP
};

static GParamSpec *icon_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfIcon, gf_icon, GTK_TYPE_BUTTON)

static void
update_state (GfIcon *self)
{
  GtkStateFlags state;

  state = gtk_widget_get_state_flags (GTK_WIDGET (self));
  state &= ~GTK_STATE_FLAG_SELECTED;

  if (self->selected)
    state |= GTK_STATE_FLAG_SELECTED;

  gtk_widget_set_state_flags (GTK_WIDGET (self), state, TRUE);
}

static void
gf_icon_constructed (GObject *object)
{
  GfIcon *self;
  GIcon *icon;
  const char *name;

  self = GF_ICON (object);

  G_OBJECT_CLASS (gf_icon_parent_class)->constructed (object);

  icon = g_file_info_get_icon (self->info);
  gtk_image_set_from_gicon (GTK_IMAGE (self->image), icon, GTK_ICON_SIZE_DIALOG);
  gtk_image_set_pixel_size (GTK_IMAGE (self->image), self->icon_size);

  name = g_file_info_get_name (self->info);
  gtk_label_set_text (GTK_LABEL (self->label), name);
}

static void
gf_icon_dispose (GObject *object)
{
  GfIcon *self;

  self = GF_ICON (object);

  g_clear_object (&self->file);
  g_clear_object (&self->info);

  G_OBJECT_CLASS (gf_icon_parent_class)->dispose (object);
}

static void
gf_icon_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  GfIcon *self;

  self = GF_ICON (object);

  switch (property_id)
    {
      case PROP_FILE:
        g_assert (self->file == NULL);
        self->file = g_value_dup_object (value);
        break;

      case PROP_INFO:
        g_assert (self->info == NULL);
        self->info = g_value_dup_object (value);
        break;

      case PROP_ICON_SIZE:
        self->icon_size = g_value_get_enum (value);
        gtk_image_set_pixel_size (GTK_IMAGE (self->image), self->icon_size);
        break;

      case PROP_EXTRA_TEXT_WIDTH:
        self->extra_text_width = g_value_get_uint (value);
        gtk_widget_queue_resize (GTK_WIDGET (self));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_icon_get_preferred_width (GtkWidget *widget,
                             gint      *minimum_width,
                             gint      *natural_width)
{
  GfIcon *self;

  self = GF_ICON (widget);

  GTK_WIDGET_CLASS (gf_icon_parent_class)->get_preferred_width (widget,
                                                                minimum_width,
                                                                natural_width);

  *minimum_width += self->extra_text_width;
  *natural_width += self->extra_text_width;
}

static void
install_properties (GObjectClass *object_class)
{
  icon_properties[PROP_FILE] =
    g_param_spec_object ("file",
                         "file",
                         "file",
                         G_TYPE_FILE,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);

  icon_properties[PROP_INFO] =
    g_param_spec_object ("info",
                         "info",
                         "info",
                         G_TYPE_FILE_INFO,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);

  icon_properties[PROP_ICON_SIZE] =
    g_param_spec_enum ("icon-size",
                       "icon-size",
                       "icon-size",
                       GF_TYPE_ICON_SIZE,
                       GF_ICON_SIZE_48PX,
                       G_PARAM_CONSTRUCT |
                       G_PARAM_WRITABLE |
                       G_PARAM_STATIC_STRINGS);

  icon_properties[PROP_EXTRA_TEXT_WIDTH] =
    g_param_spec_uint ("extra-text-width",
                       "extra-text-width",
                       "extra-text-width",
                       0, 100, 48,
                       G_PARAM_CONSTRUCT |
                       G_PARAM_WRITABLE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, icon_properties);
}

static void
gf_icon_class_init (GfIconClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);

  object_class->constructed = gf_icon_constructed;
  object_class->dispose = gf_icon_dispose;
  object_class->set_property = gf_icon_set_property;

  widget_class->get_preferred_width = gf_icon_get_preferred_width;

  install_properties (object_class);

  gtk_widget_class_set_css_name (widget_class, "gf-icon");
}

static void
gf_icon_init (GfIcon *self)
{
  GtkWidget *box;
  GtkLabel *label;
#ifdef HAVE_PANGO144
  PangoAttrList *attrs;
#endif

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_container_add (GTK_CONTAINER (self), box);
  gtk_widget_show (box);

  self->image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (box), self->image, FALSE, FALSE, 0);
  gtk_widget_show (self->image);

  self->label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (box), self->label, TRUE, TRUE, 0);
  gtk_widget_show (self->label);

  label = GTK_LABEL (self->label);

  gtk_label_set_lines (label, 2);
  gtk_label_set_line_wrap (label, TRUE);
  gtk_label_set_line_wrap_mode (label, PANGO_WRAP_WORD_CHAR);
  gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
  gtk_label_set_justify (label, GTK_JUSTIFY_CENTER);
  gtk_label_set_yalign (label, 0.0);

#ifdef HAVE_PANGO144
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_insert_hyphens_new (FALSE));

  gtk_label_set_attributes (label, attrs);
  pango_attr_list_unref (attrs);
#endif
}

GtkWidget *
gf_icon_new (GFile     *file,
             GFileInfo *info)
{
  return g_object_new (GF_TYPE_ICON,
                       "file", file,
                       "info", info,
                       NULL);
}

void
gf_icon_set_selected (GfIcon   *self,
                      gboolean  selected)
{
  if (self->selected == selected)
    return;

  self->selected = selected;
  update_state (self);
}

GFile *
gf_icon_get_file (GfIcon *self)
{
  return self->file;
}
