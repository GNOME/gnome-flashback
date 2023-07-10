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

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

#include "gf-desktop-enums.h"
#include "gf-desktop-enum-types.h"
#include "gf-rename-popover.h"
#include "gf-trash-icon.h"
#include "gf-utils.h"

typedef struct
{
  GCancellable    *cancellable;
  GCancellable    *thumbnail_cancellable;

  GtkGesture      *multi_press;

  double           press_x;
  double           press_y;

  GfIconView      *icon_view;
  GFile           *file;
  GFileInfo       *info;

  GFileMonitor    *monitor;

  GfIconSize       icon_size;
  guint            extra_text_width;

  char            *css_class;

  GtkWidget       *image;
  GtkWidget       *label;

  gboolean         selected;
  gboolean         did_select;

  GDesktopAppInfo *app_info;

  char            *name;
  char            *name_collated;

  GtkWidget       *popover;

  gboolean         thumbnail_error;
  GIcon           *thumbnail;
} GfIconPrivate;

enum
{
  PROP_0,

  PROP_ICON_VIEW,
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

  CHANGED,

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

static char **
get_selected_uris (GfIcon *self)
{
  GfIconPrivate *priv;
  GList *selected_icons;
  int n_uris;
  char **uris;
  GFile *file;
  GList *l;
  int i;

  priv = gf_icon_get_instance_private (self);

  selected_icons = gf_icon_view_get_selected_icons (priv->icon_view);
  if (selected_icons == NULL)
    return NULL;

  n_uris = g_list_length (selected_icons);
  uris = g_new0 (char *, n_uris + 1);

  file = gf_icon_get_file (self);
  uris[0] = g_file_get_uri (file);

  for (l = selected_icons, i = 1; l != NULL; l = l->next)
    {
      GfIcon *icon;

      icon = l->data;
      if (icon == self)
        continue;

      file = gf_icon_get_file (icon);
      uris[i++] = g_file_get_uri (file);
    }

  return uris;
}

static GString *
get_gnome_icon_list (GfIcon *self)
{
  GfIconPrivate *priv;
  GString *icon_list;
  GList *selected_icons;
  double scale;
  GList *l;

  priv = gf_icon_get_instance_private (self);

  icon_list = g_string_new (NULL);
  selected_icons = gf_icon_view_get_selected_icons (priv->icon_view);

  if (selected_icons == NULL)
    return icon_list;

  scale = 1.0 / gf_get_nautilus_scale ();

  for (l = selected_icons; l != NULL; l = l->next)
    {
      GfIcon *icon;
      GtkWidget *image;
      GFile *file;
      char *uri;
      GtkAllocation allocation;

      icon = l->data;

      image = gf_icon_get_image (icon);
      file = gf_icon_get_file (icon);
      uri = g_file_get_uri (file);

      gtk_widget_get_allocation (image, &allocation);
      gtk_widget_translate_coordinates (image,
                                        GTK_WIDGET (self),
                                        -priv->press_x,
                                        -priv->press_y,
                                        &allocation.x,
                                        &allocation.y);

      allocation.x *= scale;
      allocation.y *= scale;
      allocation.width *= scale;
      allocation.height *= scale;

      g_string_append_printf (icon_list,
                              "%s\r%d:%d:%hu:%hu\r\n",
                              uri,
                              allocation.x,
                              allocation.y,
                              allocation.width,
                              allocation.height);

      g_free (uri);
    }

  return icon_list;
}

static void
drag_begin_cb (GtkWidget      *widget,
               GdkDragContext *context,
               GfIcon         *self)
{
  GfIconPrivate *priv;
  gint scale;
  GtkAllocation allocation;
  cairo_surface_t *icon;
  cairo_t *cr;

  priv = gf_icon_get_instance_private (self);

  scale = gtk_widget_get_scale_factor (widget);
  gtk_widget_get_allocation (widget, &allocation);

  icon = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                     allocation.width * scale,
                                     allocation.height * scale);

  cairo_surface_set_device_scale (icon, scale, scale);

  cr = cairo_create (icon);
  gtk_widget_draw (widget, cr);
  cairo_destroy (cr);

  gtk_drag_set_icon_surface (context, icon);
  cairo_surface_destroy (icon);

  gdk_drag_context_set_hotspot (context, priv->press_x, priv->press_y);
}

static void
drag_data_delete_cb (GtkWidget      *widget,
                     GdkDragContext *context,
                     GfIcon         *self)
{
}

static void
drag_data_get_cb (GtkWidget        *widget,
                  GdkDragContext   *context,
                  GtkSelectionData *data,
                  guint             info,
                  guint             time,
                  GfIcon           *self)
{
  if (info == 100)
    {
      gtk_selection_data_set (data,
                              gtk_selection_data_get_target (data),
                              32,
                              (const guchar *) &widget,
                              sizeof (gpointer));
    }
  else if (info == 200)
    {
      GString *icon_list;

      icon_list = get_gnome_icon_list (self);

      gtk_selection_data_set (data,
                              gtk_selection_data_get_target (data),
                              8,
                              (const guchar *) icon_list->str,
                              icon_list->len);

      g_string_free (icon_list, TRUE);
    }
  else if (info == 300)
    {
      char **uris;

      uris = get_selected_uris (self);
      gtk_selection_data_set_uris (data, uris);
      g_strfreev (uris);
    }
}

static void
drag_end_cb (GtkWidget      *widget,
             GdkDragContext *context,
             GfIcon         *self)
{
}

static gboolean
drag_failed_cb (GtkWidget      *widget,
                GdkDragContext *context,
                GtkDragResult   result,
                GfIcon         *self)
{
  return FALSE;
}

static void
setup_drag_source (GfIcon *self)
{
  GdkModifierType modifiers;
  GdkDragAction actions;
  GtkTargetList *target_list;
  GdkAtom target;

  modifiers = GDK_BUTTON1_MASK | GDK_BUTTON2_MASK;
  actions = GDK_ACTION_COPY | GDK_ACTION_MOVE;

  gtk_drag_source_set (GTK_WIDGET (self), modifiers, NULL, 0, actions);

  target_list = gtk_target_list_new (NULL, 0);

  target = gdk_atom_intern_static_string ("x-gnome-flashback/icon-list");
  gtk_target_list_add (target_list, target, GTK_TARGET_SAME_APP, 100);

  target = gdk_atom_intern_static_string ("x-special/gnome-icon-list");
  gtk_target_list_add (target_list, target, 0, 200);

  target = gdk_atom_intern_static_string ("text/uri-list");
  gtk_target_list_add (target_list, target, 0, 300);

  gtk_drag_source_set_target_list (GTK_WIDGET (self), target_list);
  gtk_target_list_unref (target_list);

  g_signal_connect (self, "drag-begin",
                    G_CALLBACK (drag_begin_cb), self);

  g_signal_connect (self, "drag-data-delete",
                    G_CALLBACK (drag_data_delete_cb), self);

  g_signal_connect (self, "drag-data-get",
                    G_CALLBACK (drag_data_get_cb), self);

  g_signal_connect (self, "drag-end",
                    G_CALLBACK (drag_end_cb), self);

  g_signal_connect (self, "drag-failed",
                    G_CALLBACK (drag_failed_cb), self);
}

static void
rename_validate_cb (GfRenamePopover *popover,
                    const char      *new_name,
                    GfIcon          *self)
{
  GfIconPrivate *priv;
  char *message;
  gboolean valid;

  priv = gf_icon_get_instance_private (self);

  if (g_strcmp0 (new_name, gf_icon_get_name (self)) == 0)
    {
      gf_rename_popover_set_valid (popover, TRUE, "");
      return;
    }

  message = NULL;
  valid = gf_icon_view_validate_new_name (priv->icon_view,
                                          gf_icon_get_file_type (self),
                                          new_name,
                                          &message);

  gf_rename_popover_set_valid (popover, valid, message);
  g_free (message);
}

static void
rename_do_rename_cb (GfRenamePopover *popover,
                     GfIcon          *self)
{
  GfIconPrivate *priv;
  char *new_name;

  priv = gf_icon_get_instance_private (self);
  new_name = gf_rename_popover_get_name (popover);

  if (g_strcmp0 (new_name, priv->name) != 0)
    {
      char *uri;

      uri = g_file_get_uri (priv->file);
      gf_icon_view_rename_file (priv->icon_view,
                                uri,
                                new_name,
                                gtk_get_current_event_time ());
      g_free (uri);
    }

  gtk_popover_popdown (GTK_POPOVER (popover));
  g_free (new_name);
}

static void
rename_closed_cb (GtkPopover *popover,
                  GfIcon     *self)
{
  gtk_widget_destroy (GTK_WIDGET (popover));
}

static void
rename_destroy_cb (GtkWidget *widget,
                   GfIcon    *self)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  priv->popover = NULL;
}

static void
open_cb (GtkMenuItem *item,
         GfIcon      *self)
{
  gf_icon_open (self);
}

static void
move_to_trash_cb (GtkMenuItem *item,
                  GfIcon      *self)
{
  GfIconPrivate *priv;
  char **uris;

  priv = gf_icon_get_instance_private (self);

  uris = get_selected_uris (self);
  if (uris == NULL)
    return;

  gf_icon_view_move_to_trash (priv->icon_view,
                              (const char * const *) uris,
                              gtk_get_current_event_time ());
  g_strfreev (uris);
}

static void
rename_cb (GtkMenuItem *item,
           GfIcon      *self)
{
  gf_icon_rename (self);
}

static void
empty_trash_cb (GtkMenuItem *item,
                GfIcon      *self)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  gf_icon_view_empty_trash (priv->icon_view, gtk_get_current_event_time ());
}

static void
properties_cb (GtkMenuItem *item,
               GfIcon      *self)
{
  GfIconPrivate *priv;
  char **uris;

  priv = gf_icon_get_instance_private (self);

  uris = get_selected_uris (self);
  if (uris == NULL)
    return;

  gf_icon_view_show_item_properties (priv->icon_view,
                                     (const char * const *) uris);

  g_strfreev (uris);
}

static GtkWidget *
create_popup_menu (GfIcon *self)
{
  GfIconPrivate *priv;
  GtkWidget *popup_menu;
  GtkStyleContext *context;
  GList *selected_icons;
  int n_selected_icons;
  GtkWidget *item;
  GtkWidget *label;
  gboolean show_delete;
  gboolean disable_delete;
  GList *l;

  priv = gf_icon_get_instance_private (self);

  popup_menu = gtk_menu_new ();

  context = gtk_widget_get_style_context (popup_menu);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_CONTEXT_MENU);

  selected_icons = gf_icon_view_get_selected_icons (priv->icon_view);
  n_selected_icons = g_list_length (selected_icons);

  item = gtk_menu_item_new_with_label (_("Open"));
  gtk_widget_set_sensitive (item, n_selected_icons == 1);
  gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
  gtk_widget_show (item);

  label = gtk_bin_get_child (GTK_BIN (item));
  gtk_accel_label_set_accel (GTK_ACCEL_LABEL (label), GDK_KEY_Return, 0);

  g_signal_connect (item, "activate",
                    G_CALLBACK (open_cb),
                    self);

  show_delete = FALSE;
  disable_delete = FALSE;

  if (n_selected_icons == 1 &&
      GF_ICON_GET_CLASS (self)->can_delete (GF_ICON (self)))
    show_delete = TRUE;

  if (n_selected_icons > 1)
    {
      for (l = selected_icons; l != NULL; l = l->next)
        {
          if (GF_ICON_GET_CLASS (l->data)->can_delete (GF_ICON (l->data)))
            show_delete = TRUE;
          else
            disable_delete = TRUE;
        }
    }

  if (show_delete)
    {
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
      gtk_widget_show (item);

      item = gtk_menu_item_new_with_label (_("Move to Trash"));
      gtk_widget_set_sensitive (item, !disable_delete);
      gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
      gtk_widget_show (item);

      label = gtk_bin_get_child (GTK_BIN (item));
      gtk_accel_label_set_accel (GTK_ACCEL_LABEL (label), GDK_KEY_Delete, 0);

      g_signal_connect (item, "activate",
                        G_CALLBACK (move_to_trash_cb),
                        self);
    }

  if (GF_ICON_GET_CLASS (self)->can_rename (GF_ICON (self)) &&
      n_selected_icons == 1)
    {
      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
      gtk_widget_show (item);

      item = gtk_menu_item_new_with_label (_("Rename..."));
      gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
      gtk_widget_show (item);

      label = gtk_bin_get_child (GTK_BIN (item));
      gtk_accel_label_set_accel (GTK_ACCEL_LABEL (label), GDK_KEY_F2, 0);

      g_signal_connect (item, "activate",
                        G_CALLBACK (rename_cb),
                        self);
    }

  if (GF_IS_TRASH_ICON (self) &&
      n_selected_icons == 1)
    {
      gboolean is_empty;

      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
      gtk_widget_show (item);

      is_empty = gf_trash_icon_is_empty (GF_TRASH_ICON (self));

      item = gtk_menu_item_new_with_label (_("Empty Trash"));
      gtk_widget_set_sensitive (item, !is_empty);
      gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
      gtk_widget_show (item);

      g_signal_connect (item, "activate",
                        G_CALLBACK (empty_trash_cb),
                        self);
    }

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
  GdkModifierType state;
  gboolean control_pressed;
  gboolean shift_pressed;

  priv = gf_icon_get_instance_private (self);

  priv->press_x = x;
  priv->press_y = y;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  if (event == NULL)
    return;

  gdk_event_get_state (event, &state);

  control_pressed = (state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK;
  shift_pressed = (state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK;

  gtk_widget_grab_focus (GTK_WIDGET (priv->icon_view));

  if (button == GDK_BUTTON_PRIMARY)
    {
      if (!priv->selected && !control_pressed && !shift_pressed)
        gf_icon_view_clear_selection (priv->icon_view);

      priv->did_select = !priv->selected;
      gf_icon_set_selected (self, TRUE);

      if (!control_pressed && n_press == 2)
        gf_icon_open (self);
    }
  else if (button == GDK_BUTTON_SECONDARY)
    {
      if (!priv->selected && !control_pressed && !shift_pressed)
        gf_icon_view_clear_selection (priv->icon_view);

      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      gf_icon_set_selected (self, TRUE);

      gf_icon_popup_menu (self);
    }
  else if (button == GDK_BUTTON_MIDDLE)
    {
      if (!priv->selected && !control_pressed && !shift_pressed)
        gf_icon_view_clear_selection (priv->icon_view);

      priv->did_select = !priv->selected;
      gf_icon_set_selected (self, TRUE);
    }
}

static void
multi_press_released_cb (GtkGestureMultiPress *gesture,
                         gint                  n_press,
                         gdouble               x,
                         gdouble               y,
                         GfIcon               *self)
{
  GfIconPrivate *priv;
  guint button;
  GdkEventSequence *sequence;
  const GdkEvent *event;
  GdkModifierType state;
  gboolean control_pressed;
  gboolean shift_pressed;

  priv = gf_icon_get_instance_private (self);

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  if (event == NULL)
    return;

  gdk_event_get_state (event, &state);

  control_pressed = (state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK;
  shift_pressed = (state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK;

  if (button == GDK_BUTTON_PRIMARY ||
      button == GDK_BUTTON_MIDDLE)
    {
      if (!control_pressed && !shift_pressed)
        {
          gboolean was_selected;

          was_selected = priv->selected;

          gf_icon_view_clear_selection (priv->icon_view);
          gf_icon_set_selected (self, was_selected);
        }
      else if (control_pressed && !priv->did_select)
        {
          gf_icon_set_selected (self, FALSE);
        }
    }
}

static cairo_surface_t *
get_thumbnail_surface (GfIcon *self)
{
  GfIconPrivate *priv;
  GtkIconTheme *icon_theme;
  GtkIconInfo *icon_info;
  int scale;
  GtkIconLookupFlags lookup_flags;
  GError *error;
  cairo_surface_t *surface;
  int width;
  int height;
  int size;
  cairo_surface_t *thumbnail_surface;
  double x_scale;
  double y_scale;
  cairo_t *cr;
  double x;
  double y;

  priv = gf_icon_get_instance_private (self);

  if (priv->thumbnail == NULL)
    return NULL;

  icon_theme = gtk_icon_theme_get_default ();
  scale = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  lookup_flags = GTK_ICON_LOOKUP_FORCE_SIZE;

  icon_info = gtk_icon_theme_lookup_by_gicon_for_scale (icon_theme,
                                                        priv->thumbnail,
                                                        priv->icon_size,
                                                        scale,
                                                        lookup_flags);

  if (icon_info == NULL)
    return NULL;

  error = NULL;
  surface = gtk_icon_info_load_surface (icon_info, NULL, &error);
  g_object_unref (icon_info);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  if (surface == NULL)
    return NULL;

  if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_IMAGE)
    {
      cairo_surface_destroy (surface);
      return NULL;
    }

  width = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);
  size = MAX (width, height);

  thumbnail_surface = cairo_surface_create_similar_image (surface,
                                                          CAIRO_FORMAT_ARGB32,
                                                          size,
                                                          size);

  cairo_surface_get_device_scale (surface, &x_scale, &y_scale);
  cairo_surface_set_device_scale (thumbnail_surface, x_scale, y_scale);

  x = (size - width) / scale;
  y = (size - height) / scale;

  cr = cairo_create (thumbnail_surface);

  cairo_set_source_surface (cr, surface, x, y);
  cairo_surface_destroy (surface);

  cairo_paint (cr);
  cairo_destroy (cr);

  return thumbnail_surface;
}

static void
update_icon (GfIcon *self)
{
  GfIconPrivate *priv;
  gboolean is_thumbnail;
  GIcon *icon;
  GtkIconSize size;

  priv = gf_icon_get_instance_private (self);

  is_thumbnail = FALSE;
  icon = GF_ICON_GET_CLASS (self)->get_icon (self, &is_thumbnail);

  if (is_thumbnail)
    {
      cairo_surface_t *surface;

      surface = get_thumbnail_surface (self);

      if (surface != NULL)
        {
          gtk_image_set_from_surface (GTK_IMAGE (priv->image), surface);
          cairo_surface_destroy (surface);
          return;
        }
    }

  size = GTK_ICON_SIZE_DIALOG;

  if (icon != NULL)
    gtk_image_set_from_gicon (GTK_IMAGE (priv->image), icon, size);
  else
    gtk_image_set_from_icon_name (GTK_IMAGE (priv->image), "image-missing", size);

  gtk_image_set_pixel_size (GTK_IMAGE (priv->image), priv->icon_size);
}

static gboolean
update_text (GfIcon *self)
{
  GfIconPrivate *priv;
  const char *name;
  char *old_name;

  priv = gf_icon_get_instance_private (self);

  name = GF_ICON_GET_CLASS (self)->get_text (self);

  old_name = priv->name;
  priv->name = g_strdup (name);

  gtk_label_set_text (GTK_LABEL (priv->label), name);

  g_clear_pointer (&priv->name_collated, g_free);
  priv->name_collated = g_utf8_collate_key_for_filename (name, -1);

  if (g_strcmp0 (old_name, name) != 0)
    {
      g_free (old_name);
      return TRUE;
    }

  g_free (old_name);
  return FALSE;
}

static void
thumbnail_ready_cb (GObject      *object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  GError *error;
  GIcon *icon;
  GfIcon *self;
  GfIconPrivate *priv;

  error = NULL;
  icon = gf_thumbnail_factory_load_finish (GF_THUMBNAIL_FACTORY (object),
                                           res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  self = GF_ICON (user_data);
  priv = gf_icon_get_instance_private (self);

  if (error != NULL)
    {
      priv->thumbnail_error = TRUE;

      g_error_free (error);
      return;
    }

  g_assert (priv->thumbnail == NULL);

  priv->thumbnail_error = FALSE;
  priv->thumbnail = icon;

  update_icon (self);
}

static void
load_thumbnail (GfIcon *self)
{
  GfIconPrivate *priv;
  GfThumbnailFactory *factory;
  char *uri;
  const char *content_type;
  guint64 time_modified;

  priv = gf_icon_get_instance_private (self);

  factory = gf_icon_view_get_thumbnail_factory (priv->icon_view);

  uri = g_file_get_uri (priv->file);

  content_type = NULL;
  if (g_file_info_has_attribute (priv->info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
    content_type = g_file_info_get_content_type (priv->info);

  time_modified = gf_icon_get_time_modified (self);

  g_cancellable_cancel (priv->thumbnail_cancellable);
  g_clear_object (&priv->thumbnail_cancellable);

  priv->thumbnail_cancellable = g_cancellable_new ();

  gf_thumbnail_factory_load_async (factory,
                                   uri,
                                   content_type,
                                   time_modified,
                                   priv->thumbnail_cancellable,
                                   thumbnail_ready_cb,
                                   self);

  g_free (uri);
}

static gboolean
icon_refresh (GfIcon *self)
{
  GfIconPrivate *priv;
  const char *content_type;

  priv = gf_icon_get_instance_private (self);

  content_type = NULL;
  if (g_file_info_has_attribute (priv->info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
    content_type = g_file_info_get_content_type (priv->info);

  g_clear_object (&priv->app_info);

  priv->thumbnail_error = FALSE;
  g_clear_object (&priv->thumbnail);

  if (g_strcmp0 (content_type, "application/x-desktop") == 0)
    {
      char *path;

      path = g_file_get_path (priv->file);
      priv->app_info = g_desktop_app_info_new_from_filename (path);
      g_free (path);
    }

  update_icon (self);

  return update_text (self);
}

static void
query_info_cb (GObject      *object,
               GAsyncResult *res,
               gpointer      user_data)
{
  GError *error;
  GFileInfo *file_info;
  GfIcon *self;
  GfIconPrivate *priv;

  error = NULL;
  file_info = g_file_query_info_finish (G_FILE (object), res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_ICON (user_data);
  priv = gf_icon_get_instance_private (self);

  g_clear_object (&priv->info);
  priv->info = file_info;

  icon_refresh (self);

  g_signal_emit (self, icon_signals[CHANGED], 0);
}

static void
set_icon_size (GfIcon *self,
               int     icon_size)
{
  GfIconPrivate *priv;
  GtkStyleContext *context;

  priv = gf_icon_get_instance_private (self);
  priv->icon_size = icon_size;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (priv->css_class != NULL)
    {
      gtk_style_context_remove_class (context, priv->css_class);
      g_clear_pointer (&priv->css_class, g_free);
    }

  priv->css_class = g_strdup_printf ("s%dpx", icon_size);
  gtk_style_context_add_class (context, priv->css_class);

  update_icon (self);
}

static void
file_changed_cb (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other_file,
                 GFileMonitorEvent  event_type,
                 GfIcon            *self)
{
  switch (event_type)
    {
      case G_FILE_MONITOR_EVENT_CHANGED:
        break;

      case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        if (icon_refresh (self))
          g_signal_emit (self, icon_signals[CHANGED], 0);
        break;

      case G_FILE_MONITOR_EVENT_DELETED:
        break;

      case G_FILE_MONITOR_EVENT_CREATED:
        break;

      case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        gf_icon_update (self);
        break;

      case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
        break;

      case G_FILE_MONITOR_EVENT_UNMOUNTED:
        break;

      case G_FILE_MONITOR_EVENT_MOVED:
        break;

      case G_FILE_MONITOR_EVENT_RENAMED:
        break;

      case G_FILE_MONITOR_EVENT_MOVED_IN:
        break;

      case G_FILE_MONITOR_EVENT_MOVED_OUT:
        break;

      default:
        break;
    }
}

static void
set_file (GfIcon *self,
          GFile  *file)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  g_clear_object (&priv->file);
  priv->file = g_object_ref (file);

  GF_ICON_GET_CLASS (self)->create_file_monitor (self);
}

static void
gf_icon_constructed (GObject *object)
{
  G_OBJECT_CLASS (gf_icon_parent_class)->constructed (object);
  icon_refresh (GF_ICON (object));
}

static void
gf_icon_dispose (GObject *object)
{
  GfIcon *self;
  GfIconPrivate *priv;

  self = GF_ICON (object);
  priv = gf_icon_get_instance_private (self);

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);

  g_cancellable_cancel (priv->thumbnail_cancellable);
  g_clear_object (&priv->thumbnail_cancellable);

  g_clear_object (&priv->multi_press);

  g_clear_object (&priv->file);
  g_clear_object (&priv->info);

  g_clear_object (&priv->monitor);

  g_clear_object (&priv->app_info);

  g_clear_object (&priv->thumbnail);

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

  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->name_collated, g_free);

  g_clear_pointer (&priv->popover, gtk_widget_destroy);

  G_OBJECT_CLASS (gf_icon_parent_class)->finalize (object);
}

static void
gf_icon_get_property (GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{

  GfIcon *self;
  GfIconPrivate *priv;

  self = GF_ICON (object);
  priv = gf_icon_get_instance_private (self);

  switch (property_id)
    {
      case PROP_ICON_SIZE:
        g_value_set_enum (value, priv->icon_size);
        break;

      case PROP_EXTRA_TEXT_WIDTH:
        g_value_set_uint (value, priv->extra_text_width);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
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
      case PROP_ICON_VIEW:
        g_assert (priv->icon_view == NULL);
        priv->icon_view = g_value_get_object (value);
        break;

      case PROP_FILE:
        g_assert (priv->file == NULL);
        set_file (self, g_value_get_object (value));
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
gf_icon_create_file_monitor (GfIcon *self)
{
  GfIconPrivate *priv;
  GError *error;

  priv = gf_icon_get_instance_private (self);

  g_clear_object (&priv->monitor);

  error = NULL;
  priv->monitor = g_file_monitor_file (priv->file,
                                       G_FILE_MONITOR_NONE,
                                       NULL,
                                       &error);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (priv->monitor,
                    "changed",
                    G_CALLBACK (file_changed_cb),
                    self);
}

static GIcon *
gf_icon_get_icon (GfIcon   *self,
                  gboolean *is_thumbnail)
{
  GfIconPrivate *priv;
  GIcon *icon;

  priv = gf_icon_get_instance_private (self);
  icon = NULL;

  if (priv->thumbnail != NULL)
    {
      *is_thumbnail = TRUE;
      return priv->thumbnail;
    }

  if (priv->app_info != NULL)
    icon = g_app_info_get_icon (G_APP_INFO (priv->app_info));

  if (icon == NULL)
    {
      icon = g_file_info_get_icon (priv->info);
      load_thumbnail (self);
    }

  return icon;
}

static const char *
gf_icon_get_text (GfIcon *self)
{
  GfIconPrivate *priv;
  const char *name;

  priv = gf_icon_get_instance_private (self);

  name = NULL;
  if (priv->app_info != NULL)
    name = g_app_info_get_name (G_APP_INFO (priv->app_info));

  if (name == NULL)
    name = g_file_info_get_display_name (priv->info);

  return name;
}

static gboolean
gf_icon_can_delete (GfIcon *self)
{
  return TRUE;
}

static gboolean
gf_icon_can_rename (GfIcon *self)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  if (priv->app_info != NULL)
    return FALSE;

  return TRUE;
}

static void
install_properties (GObjectClass *object_class)
{
  icon_properties[PROP_ICON_VIEW] =
    g_param_spec_object ("icon-view",
                         "icon-view",
                         "icon-view",
                         GF_TYPE_ICON_VIEW,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);

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
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  icon_properties[PROP_EXTRA_TEXT_WIDTH] =
    g_param_spec_uint ("extra-text-width",
                       "extra-text-width",
                       "extra-text-width",
                       0, 100, 48,
                       G_PARAM_CONSTRUCT |
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, icon_properties);
}

static void
install_signals (void)
{
  icon_signals[SELECTED] =
    g_signal_new ("selected", GF_TYPE_ICON, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  icon_signals[CHANGED] =
    g_signal_new ("changed", GF_TYPE_ICON, G_SIGNAL_RUN_LAST,
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
  object_class->get_property = gf_icon_get_property;
  object_class->set_property = gf_icon_set_property;

  widget_class->get_preferred_width = gf_icon_get_preferred_width;

  self_class->create_file_monitor = gf_icon_create_file_monitor;
  self_class->get_icon = gf_icon_get_icon;
  self_class->get_text = gf_icon_get_text;
  self_class->can_delete = gf_icon_can_delete;
  self_class->can_rename = gf_icon_can_rename;

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

  priv->cancellable = g_cancellable_new ();

  priv->multi_press = gtk_gesture_multi_press_new (GTK_WIDGET (self));

  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->multi_press), 0);

  g_signal_connect (priv->multi_press, "pressed",
                    G_CALLBACK (multi_press_pressed_cb),
                    self);

  g_signal_connect (priv->multi_press, "released",
                    G_CALLBACK (multi_press_released_cb),
                    self);

  gtk_widget_set_focus_on_click (GTK_WIDGET (self), FALSE);

  setup_drag_source (self);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_container_add (GTK_CONTAINER (self), box);
  gtk_widget_show (box);

  priv->image = gtk_image_new ();
  gtk_widget_set_halign (priv->image, GTK_ALIGN_CENTER);
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
gf_icon_new (GfIconView *icon_view,
             GFile      *file,
             GFileInfo  *info)
{
  return g_object_new (GF_TYPE_ICON,
                       "icon-view", icon_view,
                       "file", file,
                       "info", info,
                       NULL);
}

GtkWidget *
gf_icon_get_image (GfIcon *self)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  return priv->image;
}

void
gf_icon_get_press (GfIcon *self,
                   double *x,
                   double *y)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  *x = priv->press_x;
  *y = priv->press_y;
}

void
gf_icon_set_file (GfIcon *self,
                  GFile  *file)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  g_clear_pointer (&priv->popover, gtk_widget_destroy);

  set_file (self, file);
  gf_icon_update (self);
}

GFile *
gf_icon_get_file (GfIcon *self)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  return priv->file;
}

GFileInfo *
gf_icon_get_file_info (GfIcon *self)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  return priv->info;
}

const char *
gf_icon_get_name (GfIcon *self)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  return g_file_info_get_name (priv->info);
}

const char *
gf_icon_get_name_collated (GfIcon *self)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  return priv->name_collated;
}

GFileType
gf_icon_get_file_type (GfIcon *self)
{
  GfIconPrivate *priv;
  const char *attribute;

  priv = gf_icon_get_instance_private (self);
  attribute = G_FILE_ATTRIBUTE_STANDARD_TYPE;

  return g_file_info_get_attribute_uint32 (priv->info, attribute);
}

guint64
gf_icon_get_time_modified (GfIcon *self)
{
  GfIconPrivate *priv;
  const char *attribute;

  priv = gf_icon_get_instance_private (self);
  attribute = G_FILE_ATTRIBUTE_TIME_MODIFIED;

  return g_file_info_get_attribute_uint64 (priv->info, attribute);
}

guint64
gf_icon_get_size (GfIcon *self)
{
  GfIconPrivate *priv;
  const char *attribute;

  priv = gf_icon_get_instance_private (self);
  attribute = G_FILE_ATTRIBUTE_STANDARD_SIZE;

  return g_file_info_get_attribute_uint64 (priv->info, attribute);
}

gboolean
gf_icon_is_hidden (GfIcon *self)
{
  GfIconPrivate *priv;
  gboolean hidden;
  gboolean backup;

  priv = gf_icon_get_instance_private (self);

  hidden = backup = FALSE;

  if (g_file_info_has_attribute (priv->info, G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN))
    hidden = g_file_info_get_is_hidden (priv->info);

  if (g_file_info_has_attribute (priv->info, G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP))
    backup = g_file_info_get_is_backup (priv->info);

  return hidden || backup;
}

void
gf_icon_set_selected (GfIcon   *self,
                      gboolean  selected)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  if (priv->selected == selected)
    return;

  priv->selected = selected;
  update_state (self);

  g_signal_emit (self, icon_signals[SELECTED], 0);
}

gboolean
gf_icon_get_selected (GfIcon *self)
{
  GfIconPrivate *priv;

  priv = gf_icon_get_instance_private (self);

  return priv->selected;
}

void
gf_icon_open (GfIcon *self)
{
  GfIconPrivate *priv;
  GError *error;
  char *uri;

  priv = gf_icon_get_instance_private (self);
  error = NULL;

  if (priv->app_info != NULL)
    {
      if (!gf_launch_app_info (priv->app_info, &error))
        {
          g_warning ("%s", error->message);
          g_error_free (error);
        }

      return;
    }

  uri = g_file_get_uri (priv->file);

  if (!gf_launch_uri (uri, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  g_free (uri);
}

void
gf_icon_rename (GfIcon *self)
{
  GfIconPrivate *priv;

  if (!GF_ICON_GET_CLASS (self)->can_rename (self))
    return;

  priv = gf_icon_get_instance_private (self);

  g_assert (priv->popover == NULL);
  priv->popover = gf_rename_popover_new (GTK_WIDGET (self),
                                         gf_icon_get_file_type (self),
                                         gf_icon_get_name (self));

  g_signal_connect (priv->popover, "validate",
                    G_CALLBACK (rename_validate_cb),
                    self);

  g_signal_connect (priv->popover, "do-rename",
                    G_CALLBACK (rename_do_rename_cb),
                    self);

  g_signal_connect (priv->popover, "closed",
                    G_CALLBACK (rename_closed_cb),
                    self);

  g_signal_connect (priv->popover, "destroy",
                    G_CALLBACK (rename_destroy_cb),
                    self);

  gtk_popover_popup (GTK_POPOVER (priv->popover));
}

void
gf_icon_popup_menu (GfIcon *self)
{
  GtkWidget *popup_menu;

  popup_menu = create_popup_menu (self);
  g_object_ref_sink (popup_menu);

  gtk_menu_popup_at_pointer (GTK_MENU (popup_menu), NULL);
  g_object_unref (popup_menu);
}

void
gf_icon_update (GfIcon *self)
{
  GfIconPrivate *priv;
  char *attributes;

  priv = gf_icon_get_instance_private (self);
  attributes = gf_icon_view_get_file_attributes (priv->icon_view);

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);

  priv->cancellable = g_cancellable_new ();

  g_file_query_info_async (priv->file,
                           attributes,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_LOW,
                           priv->cancellable,
                           query_info_cb,
                           self);

  g_free (attributes);
}
