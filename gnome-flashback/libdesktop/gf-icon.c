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

#include <glib/gi18n.h>

#include "gf-desktop-enums.h"
#include "gf-desktop-enum-types.h"
#include "gf-utils.h"

typedef struct
{
  GtkGesture *multi_press;

  GFile      *file;
  GFileInfo  *info;

  GfIconSize  icon_size;
  guint       extra_text_width;

  char       *css_class;

  GtkWidget  *image;
  GtkWidget  *label;

  gboolean    selected;
} GfIconPrivate;

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

enum
{
  SELECTED,

  SHOW_PROPERTIES,

  LAST_SIGNAL
};

static guint icon_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GfIcon, gf_icon, GTK_TYPE_BUTTON)

static void
update_state (GfIcon *self)
{
  GfIconPrivate *priv;
  GtkStateFlags state;

  priv = gf_icon_get_instance_private (self);

  state = gtk_widget_get_state_flags (GTK_WIDGET (self));
  state &= ~GTK_STATE_FLAG_SELECTED;

  if (priv->selected)
    state |= GTK_STATE_FLAG_SELECTED;

  gtk_widget_set_state_flags (GTK_WIDGET (self), state, TRUE);
}

static void
icon_open (GfIcon *self)
{
  GfIconPrivate *priv;
  char *uri;
  GError *error;

  priv = gf_icon_get_instance_private (self);

  uri = g_file_get_uri (priv->file);

  error = NULL;
  if (!gf_launch_uri (uri, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  g_free (uri);
}

static void
open_cb (GtkMenuItem *item,
         GfIcon      *self)
{
  icon_open (self);
}

static void
properties_cb (GtkMenuItem *item,
               GfIcon      *self)
{
  g_signal_emit (self, icon_signals[SHOW_PROPERTIES], 0);
}

static GtkWidget *
create_popup_menu (GfIcon *self)
{
  GtkWidget *popup_menu;
  GtkStyleContext *context;
  GtkWidget *item;

  popup_menu = gtk_menu_new ();

  context = gtk_widget_get_style_context (popup_menu);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_CONTEXT_MENU);

  item = gtk_menu_item_new_with_label (_("Open"));
  gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
  gtk_widget_show (item);

  g_signal_connect (item, "activate",
                    G_CALLBACK (open_cb),
                    self);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
  gtk_widget_show (item);

  item = gtk_menu_item_new_with_label (_("Properties"));
  gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
  gtk_widget_show (item);

  g_signal_connect (item, "activate",
                    G_CALLBACK (properties_cb),
                    self);

  return popup_menu;
}

static void
multi_press_pressed_cb (GtkGestureMultiPress *gesture,
                        gint                  n_press,
                        gdouble               x,
                        gdouble               y,
                        GfIcon               *self)
{
  GfIconPrivate *priv;
  guint button;
  GdkEventSequence *sequence;
  const GdkEvent *event;
  GfIconSelectedFlags flags;
  GdkModifierType state;
  gboolean control_pressed;
  gboolean shift_pressed;

  priv = gf_icon_get_instance_private (self);

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  flags = GF_ICON_SELECTED_NONE;

  if (event == NULL)
    return;

  gdk_event_get_state (event, &state);

  control_pressed = (state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK;
  shift_pressed = (state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK;

  if (button == GDK_BUTTON_PRIMARY)
    {
      gboolean selected;

      if (!control_pressed && !shift_pressed)
        flags |= GF_ICON_SELECTED_CLEAR;

      if (control_pressed || shift_pressed)
        {
          selected = !priv->selected;

          if (priv->selected)
            flags |= GF_ICON_SELECTED_REMOVE;
          else
            flags |= GF_ICON_SELECTED_ADD;
        }
      else
        {
          selected = TRUE;
          flags |= GF_ICON_SELECTED_ADD;
        }

      gf_icon_set_selected (self, selected, flags);

      if (!control_pressed && n_press == 2)
        icon_open (self);
    }
  else if (button == GDK_BUTTON_SECONDARY)
    {
      GtkWidget *popup_menu;

      if (!priv->selected && !control_pressed)
        flags |= GF_ICON_SELECTED_CLEAR;
      flags |= GF_ICON_SELECTED_ADD;

      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      gf_icon_set_selected (self, TRUE, flags);

      popup_menu = create_popup_menu (self);
      g_object_ref_sink (popup_menu);

      gtk_menu_popup_at_pointer (GTK_MENU (popup_menu), event);
      g_object_unref (popup_menu);
    }
  else if (button == GDK_BUTTON_MIDDLE)
    {
    }
}

static void
set_icon_size (GfIcon *self,
               int     icon_size)
{
  GfIconPrivate *priv;
  GtkStyleContext *context;

  priv = gf_icon_get_instance_private (self);

  priv->icon_size = icon_size;
  gtk_image_set_pixel_size (GTK_IMAGE (priv->image), icon_size);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (priv->css_class != NULL)
    {
      gtk_style_context_remove_class (context, priv->css_class);
      g_clear_pointer (&priv->css_class, g_free);
    }

  priv->css_class = g_strdup_printf ("s%dpx", icon_size);
  gtk_style_context_add_class (context, priv->css_class);
}

static void
gf_icon_constructed (GObject *object)
{
  GfIcon *self;
  GfIconPrivate *priv;
  GIcon *icon;
  const char *name;

  self = GF_ICON (object);
  priv = gf_icon_get_instance_private (self);

  G_OBJECT_CLASS (gf_icon_parent_class)->constructed (object);

  icon = g_file_info_get_icon (priv->info);
  gtk_image_set_from_gicon (GTK_IMAGE (priv->image), icon, GTK_ICON_SIZE_DIALOG);
  gtk_image_set_pixel_size (GTK_IMAGE (priv->image), priv->icon_size);

  name = g_file_info_get_display_name (priv->info);
  gtk_label_set_text (GTK_LABEL (priv->label), name);
}

static void
gf_icon_dispose (GObject *object)
{
  GfIcon *self;
  GfIconPrivate *priv;

  self = GF_ICON (object);
  priv = gf_icon_get_instance_private (self);

  g_clear_object (&priv->multi_press);

  g_clear_object (&priv->file);
  g_clear_object (&priv->info);

  G_OBJECT_CLASS (gf_icon_parent_class)->dispose (object);
}

static void
gf_icon_finalize (GObject *object)
{
  GfIcon *self;
  GfIconPrivate *priv;

  self = GF_ICON (object);
  priv = gf_icon_get_instance_private (self);

  g_clear_pointer (&priv->css_class, g_free);

  G_OBJECT_CLASS (gf_icon_parent_class)->finalize (object);
}

static void
gf_icon_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  GfIcon *self;
  GfIconPrivate *priv;

  self = GF_ICON (object);
  priv = gf_icon_get_instance_private (self);

  switch (property_id)
    {
      case PROP_FILE:
        g_assert (priv->file == NULL);
        priv->file = g_value_dup_object (value);
        break;

      case PROP_INFO:
        g_assert (priv->info == NULL);
        priv->info = g_value_dup_object (value);
        break;

      case PROP_ICON_SIZE:
        set_icon_size (self, g_value_get_enum (value));
        break;

      case PROP_EXTRA_TEXT_WIDTH:
        priv->extra_text_width = g_value_get_uint (value);
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
  GfIconPrivate *priv;

  self = GF_ICON (widget);
  priv = gf_icon_get_instance_private (self);

  GTK_WIDGET_CLASS (gf_icon_parent_class)->get_preferred_width (widget,
                                                                minimum_width,
                                                                natural_width);

  *minimum_width += priv->extra_text_width;
  *natural_width += priv->extra_text_width;
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
install_signals (void)
{
  icon_signals[SELECTED] =
    g_signal_new ("selected", GF_TYPE_ICON, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                  GF_TYPE_ICON_SELECTED_FLAGS);

  icon_signals[SHOW_PROPERTIES] =
    g_signal_new ("show-properties", GF_TYPE_ICON, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
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
  object_class->finalize = gf_icon_finalize;
  object_class->set_property = gf_icon_set_property;

  widget_class->get_preferred_width = gf_icon_get_preferred_width;

  install_properties (object_class);
  install_signals ();

  gtk_widget_class_set_css_name (widget_class, "gf-icon");
}

static void
gf_icon_init (GfIcon *self)
{
  GfIconPrivate *priv;
  GtkWidget *box;
  GtkLabel *label;
#ifdef HAVE_PANGO144
  PangoAttrList *attrs;
#endif

  priv = gf_icon_get_instance_private (self);

  priv->multi_press = gtk_gesture_multi_press_new (GTK_WIDGET (self));

  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->multi_press), 0);

  g_signal_connect (priv->multi_press, "pressed",
                    G_CALLBACK (multi_press_pressed_cb),
                    self);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_container_add (GTK_CONTAINER (self), box);
  gtk_widget_show (box);

  priv->image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (box), priv->image, FALSE, FALSE, 0);
  gtk_widget_show (priv->image);

  priv->label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (box), priv->label, TRUE, TRUE, 0);
  gtk_widget_show (priv->label);

  label = GTK_LABEL (priv->label);

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

GFile *
gf_icon_get_file (GfIcon *self)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  return priv->file;
}

const char *
gf_icon_get_name (GfIcon *self)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  return g_file_info_get_name (priv->info);
}

gboolean
gf_icon_is_hidden (GfIcon *self)
{
  GfIconPrivate *priv;
  gboolean hidden;
  gboolean backup;

  priv = gf_icon_get_instance_private (self);

  hidden = g_file_info_get_is_hidden (priv->info);
  backup = g_file_info_get_is_backup (priv->info);

  return hidden || backup;
}

void
gf_icon_set_selected (GfIcon              *self,
                      gboolean             selected,
                      GfIconSelectedFlags  flags)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  if (priv->selected == selected)
    return;

  priv->selected = selected;
  update_state (self);

  g_signal_emit (self, icon_signals[SELECTED], 0, flags);
}

gboolean
gf_icon_get_selected (GfIcon *self)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  return priv->selected;
}
