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
#include "gf-icon-view.h"

#include <gdk/gdkx.h>
#include <glib/gi18n.h>

#include "gf-create-folder-dialog.h"
#include "gf-desktop-enum-types.h"
#include "gf-dummy-icon.h"
#include "gf-file-manager-gen.h"
#include "gf-icon.h"
#include "gf-monitor-view.h"
#include "gf-nautilus-gen.h"
#include "gf-utils.h"

typedef struct
{
  GtkWidget *icon;

  GtkWidget *view;
} GfIconInfo;

struct _GfIconView
{
  GtkEventBox       parent;

  GtkGesture       *multi_press;
  GtkGesture       *drag;

  GFile            *desktop;
  GFileMonitor     *monitor;

  GSettings        *settings;

  GtkWidget        *fixed;

  GtkWidget        *dummy_icon;

  GCancellable     *cancellable;

  GList            *icons;

  guint             add_icons_id;

  GList            *selected_icons;

  GtkCssProvider   *rubberband_css;
  GtkStyleContext  *rubberband_style;
  GdkRectangle      rubberband_rect;
  GList            *rubberband_icons;

  GfNautilusGen    *nautilus;
  GfFileManagerGen *file_manager;

  GtkWidget        *create_folder_dialog;
};

enum
{
  SELECT_ALL,
  UNSELECT_ALL,

  LAST_SIGNAL
};

static guint view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfIconView, gf_icon_view, GTK_TYPE_EVENT_BOX)

static char *
build_attributes_list (const char *first,
                       ...)
{
  GString *attributes;
  va_list args;
  const char *attribute;

  attributes = g_string_new (first);
  va_start (args, first);

  while ((attribute = va_arg (args, const char *)) != NULL)
    g_string_append_printf (attributes, ",%s", attribute);

  va_end (args);

  return g_string_free (attributes, FALSE);
}

static char *
get_required_attributes (void)
{
  return build_attributes_list (G_FILE_ATTRIBUTE_STANDARD_NAME,
                                G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                G_FILE_ATTRIBUTE_STANDARD_ICON,
                                G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                                G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP,
                                NULL);
}

static GfIconInfo *
gf_icon_info_new (GtkWidget *icon)
{
  GfIconInfo *info;

  info = g_new0 (GfIconInfo, 1);
  info->icon = g_object_ref_sink (icon);

  info->view = NULL;

  return info;
}

static void
gf_icon_info_free (gpointer data)
{
  GfIconInfo *info;

  info = (GfIconInfo *) data;

  g_clear_pointer (&info->icon, g_object_unref);
  g_free (info);
}

static GList *
get_monitor_views (GfIconView *self)
{
  GList *views;
  GList *children;
  GList *l;

  views = NULL;

  children = gtk_container_get_children (GTK_CONTAINER (self->fixed));

  for (l = children; l != NULL; l = l->next)
    {
      GfMonitorView *view;

      view = GF_MONITOR_VIEW (l->data);

      if (gf_monitor_view_is_primary (view))
        views = g_list_prepend (views, view);
      else
        views = g_list_append (views, view);
    }

  g_list_free (children);
  return views;
}

static void
add_icons (GfIconView *self)
{
  GList *views;
  GList *view;
  GList *l;

  views = get_monitor_views (self);
  view = views;

  for (l = self->icons; l != NULL; l = l->next)
    {
      GfIconInfo *info;

      info = (GfIconInfo *) l->data;

      if (gf_icon_is_hidden (GF_ICON (info->icon)))
        continue;

      if (info->view != NULL)
        continue;

      while (view != NULL)
        {
          if (!gf_monitor_view_add_icon (GF_MONITOR_VIEW (view->data),
                                         info->icon))
            {
              view = view->next;
              continue;
            }

          info->view = view->data;

          break;
        }

      if (view == NULL)
        break;
    }

  g_list_free (views);
}

static gboolean
add_icons_cb (gpointer user_data)
{
  GfIconView *self;

  self = GF_ICON_VIEW (user_data);

  add_icons (self);
  self->add_icons_id = 0;

  return G_SOURCE_REMOVE;
}

static void
add_icons_idle (GfIconView *self)
{
  if (self->add_icons_id != 0)
    return;

  self->add_icons_id = g_idle_add (add_icons_cb, self);

  g_source_set_name_by_id (self->add_icons_id,
                           "[gnome-flashback] add_icons_cb");
}

static void
file_deleted (GfIconView *self,
              GFile      *deleted_file)
{
  GList *l;

  for (l = self->icons; l != NULL; l = l->next)
    {
      GfIconInfo *info;
      GFile *file;

      info = (GfIconInfo *) l->data;

      file = gf_icon_get_file (GF_ICON (info->icon));

      if (!g_file_equal (file, deleted_file))
        continue;

      if (info->view != NULL)
        {
          gf_monitor_view_remove_icon (GF_MONITOR_VIEW (info->view), info->icon);
          info->view = NULL;

          self->selected_icons = g_list_remove (self->selected_icons, l->data);
          self->rubberband_icons = g_list_remove (self->rubberband_icons, l->data);
        }

      self->icons = g_list_remove_link (self->icons, l);
      g_list_free_full (l, gf_icon_info_free);

      break;
    }
}

static void
unselect_cb (gpointer data,
             gpointer user_data)
{
  gf_icon_set_selected (data, FALSE);
}

static void
unselect_icons (GfIconView *self)
{
  if (self->selected_icons == NULL)
    return;

  g_list_foreach (self->selected_icons, unselect_cb, NULL);
  g_clear_pointer (&self->selected_icons, g_list_free);
}

static void
icon_selected_cb (GfIcon     *icon,
                  GfIconView *self)
{
  if (gf_icon_get_selected (icon))
    self->selected_icons = g_list_append (self->selected_icons, icon);
  else
    self->selected_icons = g_list_remove (self->selected_icons, icon);
}

static void
show_item_properties_cb (GObject      *object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  GError *error;

  error = NULL;
  gf_file_manager_gen_call_show_item_properties_finish (GF_FILE_MANAGER_GEN (object),
                                                        res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error showing properties: %s", error->message);
      g_error_free (error);
    }
}

static GfIconInfo *
create_icon_info (GfIconView *self,
                  GFile      *file,
                  GFileInfo  *info)
{
  GtkWidget *icon;

  icon = gf_icon_new (self, file, info);

  g_signal_connect (icon, "selected",
                    G_CALLBACK (icon_selected_cb),
                    self);

  g_settings_bind (self->settings, "icon-size",
                   icon, "icon-size",
                   G_SETTINGS_BIND_GET);

  g_settings_bind (self->settings, "extra-text-width",
                   icon, "extra-text-width",
                   G_SETTINGS_BIND_GET);

  return gf_icon_info_new (icon);
}

static void
query_info_cb (GObject      *object,
               GAsyncResult *res,
               gpointer      user_data)
{
  GFile *file;
  GFileInfo *file_info;
  GError *error;
  GfIconView *self;
  GfIconInfo *icon_info;

  file = G_FILE (object);

  error = NULL;
  file_info = g_file_query_info_finish (file, res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_ICON_VIEW (user_data);

  icon_info = create_icon_info (self, file, file_info);
  g_object_unref (file_info);

  self->icons = g_list_append (self->icons, icon_info);

  add_icons (self);
}

static void
file_created (GfIconView *self,
              GFile      *created_file)
{

  char *attributes;

  attributes = get_required_attributes ();

  g_file_query_info_async (created_file,
                           attributes,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_LOW,
                           self->cancellable,
                           query_info_cb,
                           self);

  g_free (attributes);
}

static void
desktop_changed_cb (GFileMonitor      *monitor,
                    GFile             *file,
                    GFile             *other_file,
                    GFileMonitorEvent  event_type,
                    GfIconView        *self)
{
  switch (event_type)
    {
      case G_FILE_MONITOR_EVENT_CHANGED:
        break;

      case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        break;

      case G_FILE_MONITOR_EVENT_DELETED:
        file_deleted (self, file);
        break;

      case G_FILE_MONITOR_EVENT_CREATED:
        file_created (self, file);
        break;

      case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
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

static char *
create_folder_dialog_validate_cb (GfCreateFolderDialog *dialog,
                                  const char           *folder_name,
                                  GfIconView           *self)
{
  GList *l;

  for (l = self->icons; l != NULL; l = l->next)
    {
      GfIconInfo *info;
      const char *name;

      info = l->data;

      name = gf_icon_get_name (GF_ICON (info->icon));

      if (g_strcmp0 (name, folder_name) == 0)
        return g_strdup (_("A folder with that name already exists."));

    }

  return NULL;
}

static void
create_folder_cb (GObject      *object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  GError *error;

  error = NULL;
  gf_nautilus_gen_call_create_folder_finish (GF_NAUTILUS_GEN (object),
                                             res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error creating new folder: %s", error->message);
      g_error_free (error);
    }
}

static void
create_folder_dialog_response_cb (GtkDialog  *dialog,
                                  gint        response_id,
                                  GfIconView *self)
{
  GfCreateFolderDialog *folder_dialog;
  char *folder_name;
  GFile *new_file;
  char *uri;

  if (response_id != GTK_RESPONSE_ACCEPT)
    return;

  folder_dialog = GF_CREATE_FOLDER_DIALOG (dialog);
  folder_name = gf_create_folder_dialog_get_folder_name (folder_dialog);

  new_file = g_file_get_child (self->desktop, folder_name);
  g_free (folder_name);

  uri = g_file_get_uri (new_file);
  g_object_unref (new_file);

  gf_nautilus_gen_call_create_folder (self->nautilus, uri,
                                      self->cancellable,
                                      create_folder_cb,
                                      NULL);

  gtk_widget_destroy (GTK_WIDGET (dialog));
  g_free (uri);
}

static void
create_folder_dialog_destroy_cb (GtkWidget   *widget,
                                 GfIconView  *self)
{
  self->create_folder_dialog = NULL;
}

static void
new_folder_cb (GtkMenuItem *item,
               GfIconView  *self)
{
  GtkWidget *dialog;

  if (self->nautilus == NULL)
    return;

  if (self->create_folder_dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (self->create_folder_dialog));
      return;
    }

  dialog = gf_create_folder_dialog_new ();
  self->create_folder_dialog = dialog;

  g_signal_connect (dialog, "validate",
                    G_CALLBACK (create_folder_dialog_validate_cb),
                    self);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (create_folder_dialog_response_cb),
                    self);

  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (create_folder_dialog_destroy_cb),
                    self);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
change_background_cb (GtkMenuItem *item,
                      GfIconView  *self)
{
  GError *error;

  error = NULL;
  if (!gf_launch_desktop_file ("gnome-background-panel.desktop", &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
}

static void
display_settings_cb (GtkMenuItem *item,
                     GfIconView  *self)
{
  GError *error;

  error = NULL;
  if (!gf_launch_desktop_file ("gnome-display-panel.desktop", &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
}

static void
open_terminal_cb (GtkMenuItem *item,
                  GfIconView  *self)
{
  GError *error;

  error = NULL;
  if (!gf_launch_desktop_file ("org.gnome.Terminal.desktop", &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
}

static GtkWidget *
create_popup_menu (GfIconView *self)
{
  GtkWidget *popup_menu;
  GtkStyleContext *context;
  GtkWidget *item;

  popup_menu = gtk_menu_new ();

  context = gtk_widget_get_style_context (popup_menu);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_CONTEXT_MENU);

  item = gtk_menu_item_new_with_label (_("New Folder"));
  gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
  gtk_widget_show (item);

  g_signal_connect (item, "activate",
                    G_CALLBACK (new_folder_cb),
                    self);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
  gtk_widget_show (item);

  item = gtk_menu_item_new_with_label (_("Change Background"));
  gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
  gtk_widget_show (item);

  g_signal_connect (item, "activate",
                    G_CALLBACK (change_background_cb),
                    self);

  item = gtk_menu_item_new_with_label (_("Display Settings"));
  gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
  gtk_widget_show (item);

  g_signal_connect (item, "activate",
                    G_CALLBACK (display_settings_cb),
                    self);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
  gtk_widget_show (item);

  item = gtk_menu_item_new_with_label (_("Open Terminal"));
  gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
  gtk_widget_show (item);

  g_signal_connect (item, "activate",
                    G_CALLBACK (open_terminal_cb),
                    self);

  return popup_menu;
}

static void
multi_press_pressed_cb (GtkGestureMultiPress *gesture,
                        gint                  n_press,
                        gdouble               x,
                        gdouble               y,
                        GfIconView           *self)
{
  guint button;
  GdkEventSequence *sequence;
  const GdkEvent *event;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  if (event == NULL)
    return;

  if (button == GDK_BUTTON_PRIMARY)
    {
      GdkModifierType state;
      gboolean control_pressed;
      gboolean shift_pressed;

      gdk_event_get_state (event, &state);

      control_pressed = (state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK;
      shift_pressed = (state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK;

      if (!control_pressed && !shift_pressed)
        {
          g_clear_pointer (&self->rubberband_icons, g_list_free);
          unselect_icons (self);
        }
    }
  else if (button == GDK_BUTTON_SECONDARY)
    {
      GtkWidget *popup_menu;

      unselect_icons (self);

      popup_menu = create_popup_menu (self);
      g_object_ref_sink (popup_menu);

      gtk_menu_popup_at_pointer (GTK_MENU (popup_menu), event);
      g_object_unref (popup_menu);
    }
}

static void
drag_begin_cb (GtkGestureDrag *gesture,
               gdouble         start_x,
               gdouble         start_y,
               GfIconView     *self)
{
  self->rubberband_rect = (GdkRectangle) { 0 };

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
drag_end_cb (GtkGestureDrag *gesture,
             gdouble         offset_x,
             gdouble         offset_y,
             GfIconView     *self)
{

  g_clear_pointer (&self->rubberband_icons, g_list_free);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
drag_update_cb (GtkGestureDrag *gesture,
                gdouble         offset_x,
                gdouble         offset_y,
                GfIconView     *self)
{
  double start_x;
  double start_y;
  GdkRectangle old_rect;

  gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (self->drag),
                                    &start_x, &start_y);

  old_rect = self->rubberband_rect;
  self->rubberband_rect = (GdkRectangle) {
    .x = start_x,
    .y = start_y,
    .width = ABS (offset_x),
    .height = ABS (offset_y)
  };

  if (offset_x < 0)
    self->rubberband_rect.x += offset_x;

  if (offset_y < 0)
    self->rubberband_rect.y += offset_y;

  if ((self->rubberband_rect.x > old_rect.x ||
       self->rubberband_rect.y > old_rect.y ||
       self->rubberband_rect.width < old_rect.width ||
       self->rubberband_rect.height < old_rect.height) &&
      self->rubberband_icons != NULL)
    {
      GList *rubberband_icons;
      GList *l;

      rubberband_icons = g_list_copy (self->rubberband_icons);

      for (l = rubberband_icons; l != NULL; l = l->next)
        {
          GfIcon *icon;
          GtkAllocation allocation;

          icon = l->data;

          gtk_widget_get_allocation (GTK_WIDGET (icon), &allocation);

          if (!gdk_rectangle_intersect (&self->rubberband_rect,
                                        &allocation,
                                        NULL))
            {
              if (gf_icon_get_selected (icon))
                gf_icon_set_selected (icon, FALSE);
              else
                gf_icon_set_selected (icon, TRUE);

              self->rubberband_icons = g_list_remove (self->rubberband_icons,
                                                      icon);
            }
        }

      g_list_free (rubberband_icons);
    }

  if (self->rubberband_rect.x < old_rect.x ||
      self->rubberband_rect.y < old_rect.y ||
      self->rubberband_rect.width > old_rect.width ||
      self->rubberband_rect.height > old_rect.height)
    {
      GList *rubberband_icons;
      GList *monitor_views;
      GList *l;

      rubberband_icons = NULL;
      monitor_views = get_monitor_views (self);

      for (l = monitor_views; l != NULL; l = l->next)
        {
          GfMonitorView *monitor_view;
          GtkAllocation allocation;
          GdkRectangle rect;

          monitor_view = l->data;

          gtk_widget_get_allocation (GTK_WIDGET (monitor_view), &allocation);

          if (gdk_rectangle_intersect (&self->rubberband_rect,
                                       &allocation,
                                       &rect))
            {
              GList *icons;

              icons = gf_monitor_view_get_icons (monitor_view, &rect);
              rubberband_icons = g_list_concat (rubberband_icons, icons);
            }
        }

      g_list_free (monitor_views);

      if (rubberband_icons != NULL)
        {
          for (l = rubberband_icons; l != NULL; l = l->next)
            {
              GfIcon *icon;

              icon = l->data;

              if (g_list_find (self->rubberband_icons, icon) != NULL)
                continue;

              self->rubberband_icons = g_list_prepend (self->rubberband_icons,
                                                       icon);

              if (gf_icon_get_selected (icon))
                gf_icon_set_selected (icon, FALSE);
              else
                gf_icon_set_selected (icon, TRUE);
            }

          g_list_free (rubberband_icons);
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
view_foreach_cb (GtkWidget *widget,
                 gpointer   user_data)
{
  GfIconView *self;
  GList *l;

  self = GF_ICON_VIEW (user_data);

  for (l = self->icons; l != NULL; l = l->next)
    {
      GfIconInfo *info;

      info = (GfIconInfo *) l->data;

      if (info->icon == widget)
        {
          gtk_container_remove (GTK_CONTAINER (info->view), widget);
          info->view = NULL;
          break;
        }
    }
}

static void
size_changed_cb (GtkWidget  *view,
                 GfIconView *self)
{
  gtk_container_foreach (GTK_CONTAINER (view), view_foreach_cb, self);
  add_icons_idle (self);
}

static void
next_files_cb (GObject      *object,
               GAsyncResult *res,
               gpointer      user_data)
{
  GFileEnumerator *enumerator;
  GList *files;
  GError *error;
  GfIconView *self;
  GList *l;

  enumerator = G_FILE_ENUMERATOR (object);

  error = NULL;
  files = g_file_enumerator_next_files_finish (enumerator, res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_ICON_VIEW (user_data);

  for (l = files; l != NULL; l = l->next)
    {
      GFileInfo *info;
      GFile *file;
      GfIconInfo *icon_info;

      info = l->data;
      file = g_file_enumerator_get_child (enumerator, info);

      icon_info = create_icon_info (self, file, info);
      g_object_unref (file);

      self->icons = g_list_prepend (self->icons, icon_info);
    }

  self->icons = g_list_reverse (self->icons);
  g_list_free_full (files, g_object_unref);

  add_icons (self);
}

static void
enumerate_children_cb (GObject      *object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GFileEnumerator *enumerator;
  GError *error;
  GfIconView *self;

  error = NULL;
  enumerator = g_file_enumerate_children_finish (G_FILE (object), res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_ICON_VIEW (user_data);

  g_file_enumerator_next_files_async (enumerator,
                                      G_MAXINT32,
                                      G_PRIORITY_LOW,
                                      self->cancellable,
                                      next_files_cb,
                                      user_data);

  g_object_unref (enumerator);
}

static void
enumerate_desktop (GfIconView *self)
{
  char *attributes;

  attributes = get_required_attributes ();

  g_file_enumerate_children_async (self->desktop,
                                   attributes,
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_LOW,
                                   self->cancellable,
                                   enumerate_children_cb,
                                   self);

  g_free (attributes);
}

static void
nautilus_ready_cb (GObject     *object,
                   GAsyncResult *res,
                   gpointer      user_data)

{
  GError *error;
  GfNautilusGen *nautilus;
  GfIconView *self;

  error = NULL;
  nautilus = gf_nautilus_gen_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_ICON_VIEW (user_data);
  self->nautilus = nautilus;
}

static void
file_manager_ready_cb (GObject     *object,
                       GAsyncResult *res,
                       gpointer      user_data)

{
  GError *error;
  GfFileManagerGen *file_manager;
  GfIconView *self;

  error = NULL;
  file_manager = gf_file_manager_gen_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_ICON_VIEW (user_data);
  self->file_manager = file_manager;
}

static GtkWidget *
find_monitor_view_by_monitor (GfIconView *self,
                              GdkMonitor *monitor)
{
  GfMonitorView *view;
  GList *children;
  GList *l;

  children = gtk_container_get_children (GTK_CONTAINER (self->fixed));

  for (l = children; l != NULL; l = l->next)
    {
      view = GF_MONITOR_VIEW (l->data);

      if (gf_monitor_view_get_monitor (view) == monitor)
        break;

      view = NULL;
    }

  g_list_free (children);

  return GTK_WIDGET (view);
}

static void
workarea_changed (GfIconView *self)
{
  GList *children;
  GList *l;

  children = gtk_container_get_children (GTK_CONTAINER (self->fixed));

  for (l = children; l != NULL; l = l->next)
    {
      GfMonitorView *view;
      GdkMonitor *monitor;
      GdkRectangle workarea;

      view = GF_MONITOR_VIEW (l->data);

      monitor = gf_monitor_view_get_monitor (view);
      gdk_monitor_get_workarea (monitor, &workarea);

      gf_monitor_view_set_size (view, workarea.width, workarea.height);
      gtk_fixed_move (GTK_FIXED (self->fixed), l->data, workarea.x, workarea.y);
    }

  g_list_free (children);
}

static void
create_monitor_view (GfIconView *self,
                     GdkMonitor *monitor)
{
  guint column_spacing;
  guint row_spacing;
  GdkRectangle workarea;
  GtkWidget *view;

  column_spacing = g_settings_get_uint (self->settings, "column-spacing");
  row_spacing = g_settings_get_uint (self->settings, "row-spacing");

  gdk_monitor_get_workarea (monitor, &workarea);

  view = gf_monitor_view_new (monitor,
                              GF_DUMMY_ICON (self->dummy_icon),
                              column_spacing,
                              row_spacing);

  gf_monitor_view_set_size (GF_MONITOR_VIEW (view),
                            workarea.width,
                            workarea.height);

  g_signal_connect (view, "size-changed", G_CALLBACK (size_changed_cb), self);

  gtk_fixed_put (GTK_FIXED (self->fixed), view, workarea.x, workarea.y);
  gtk_widget_show (view);

  g_settings_bind (self->settings, "column-spacing",
                   view, "column-spacing",
                   G_SETTINGS_BIND_GET);

  g_settings_bind (self->settings, "row-spacing",
                   view, "row-spacing",
                   G_SETTINGS_BIND_GET);
}

static void
monitor_added_cb (GdkDisplay *display,
                  GdkMonitor *monitor,
                  GfIconView *self)
{
  create_monitor_view (self, monitor);
}

static void
monitor_removed_cb (GdkDisplay *display,
                    GdkMonitor *monitor,
                    GfIconView *self)
{
  GtkWidget *view;

  view = find_monitor_view_by_monitor (self, monitor);
  if (view == NULL)
    return;

  gtk_widget_destroy (view);
}

static GdkFilterReturn
filter_func (GdkXEvent *xevent,
             GdkEvent  *event,
             gpointer   user_data)
{
  XEvent *x;
  GdkAtom atom;

  x = (XEvent *) xevent;

  if (x->type != PropertyNotify)
    return GDK_FILTER_CONTINUE;

  atom = gdk_atom_intern_static_string ("_NET_WORKAREA");
  if (x->xproperty.atom == gdk_x11_atom_to_xatom (atom))
    workarea_changed (GF_ICON_VIEW (user_data));

  return GDK_FILTER_CONTINUE;
}

static void
remove_event_filter (GfIconView *self)
{
  GdkScreen *screen;
  GdkWindow *root;

  screen = gtk_widget_get_screen (GTK_WIDGET (self));
  root = gdk_screen_get_root_window (screen);

  gdk_window_remove_filter (root, filter_func, self);
}

static void
add_event_filter (GfIconView *self)
{
  GdkScreen *screen;
  GdkWindow *root;
  GdkEventMask event_mask;

  screen = gtk_widget_get_screen (GTK_WIDGET (self));
  root = gdk_screen_get_root_window (screen);

  event_mask = gdk_window_get_events (root);
  event_mask |= GDK_PROPERTY_NOTIFY;

  gdk_window_add_filter (root, filter_func, self);
  gdk_window_set_events (root, event_mask);
}

static void
select_cb (gpointer data,
           gpointer user_data)
{
  GfIconInfo *info;

  info = data;

  gf_icon_set_selected (GF_ICON (info->icon), TRUE);
}

static void
select_all_cb (GfIconView *self,
               gpointer    user_data)
{
  g_list_foreach (self->icons, select_cb, NULL);
}

static void
unselect_all_cb (GfIconView *self,
                 gpointer    user_data)
{
  unselect_icons (self);
}

static GtkWidget *
create_dummy_icon (GfIconView *self)
{
  GtkWidget *widget;

  widget = gf_dummy_icon_new (self);

  g_settings_bind (self->settings, "icon-size",
                   widget, "icon-size",
                   G_SETTINGS_BIND_GET);

  g_settings_bind (self->settings, "extra-text-width",
                   widget, "extra-text-width",
                   G_SETTINGS_BIND_GET);

  return widget;
}

static void
gf_icon_view_dispose (GObject *object)
{
  GfIconView *self;

  self = GF_ICON_VIEW (object);

  g_clear_object (&self->multi_press);
  g_clear_object (&self->drag);

  g_clear_object (&self->desktop);
  g_clear_object (&self->monitor);
  g_clear_object (&self->settings);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (self->icons != NULL)
    {
      g_list_free_full (self->icons, gf_icon_info_free);
      self->icons = NULL;
    }

  g_clear_pointer (&self->selected_icons, g_list_free);

  g_clear_object (&self->rubberband_css);
  g_clear_object (&self->rubberband_style);
  g_clear_pointer (&self->rubberband_icons, g_list_free);

  g_clear_object (&self->nautilus);
  g_clear_object (&self->file_manager);

  g_clear_pointer (&self->create_folder_dialog, gtk_widget_destroy);

  G_OBJECT_CLASS (gf_icon_view_parent_class)->dispose (object);
}

static void
gf_icon_view_finalize (GObject *object)
{
  GfIconView *self;

  self = GF_ICON_VIEW (object);

  g_clear_pointer (&self->dummy_icon, gtk_widget_unparent);

  if (self->add_icons_id != 0)
    {
      g_source_remove (self->add_icons_id);
      self->add_icons_id = 0;
    }

  remove_event_filter (self);

  G_OBJECT_CLASS (gf_icon_view_parent_class)->finalize (object);
}

static void
ensure_rubberband_style (GfIconView *self)
{
  GtkWidgetPath *path;
  GdkScreen *screen;
  GtkStyleProvider *provider;
  guint priority;

  if (self->rubberband_style != NULL)
    return;

  self->rubberband_css = gtk_css_provider_new ();
  self->rubberband_style = gtk_style_context_new ();

  path = gtk_widget_path_new ();

  gtk_widget_path_append_type (path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name (path, -1, "rubberband");
  gtk_widget_path_iter_add_class (path, -1, GTK_STYLE_CLASS_RUBBERBAND);

  gtk_style_context_set_path (self->rubberband_style, path);
  gtk_widget_path_unref (path);

  screen = gtk_widget_get_screen (GTK_WIDGET (self));
  provider = GTK_STYLE_PROVIDER (self->rubberband_css);
  priority = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION;

  gtk_style_context_add_provider_for_screen (screen, provider, priority);
}

static gboolean
gf_icon_view_draw (GtkWidget *widget,
                   cairo_t   *cr)
{
  GfIconView *self;

  self = GF_ICON_VIEW (widget);

  GTK_WIDGET_CLASS (gf_icon_view_parent_class)->draw (widget, cr);

  if (!gtk_gesture_is_recognized (GTK_GESTURE (self->drag)))
    return TRUE;

  ensure_rubberband_style (self);

  cairo_save (cr);

  gtk_render_background (self->rubberband_style, cr,
                         self->rubberband_rect.x,
                         self->rubberband_rect.y,
                         self->rubberband_rect.width,
                         self->rubberband_rect.height);

  gtk_render_frame (self->rubberband_style, cr,
                    self->rubberband_rect.x,
                    self->rubberband_rect.y,
                    self->rubberband_rect.width,
                    self->rubberband_rect.height);

  cairo_restore (cr);

  return TRUE;
}

static void
install_signals (void)
{
  view_signals[SELECT_ALL] =
    g_signal_new ("select-all", GF_TYPE_ICON_VIEW,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  view_signals[UNSELECT_ALL] =
    g_signal_new ("unselect-all", GF_TYPE_ICON_VIEW,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
add_bindings (GtkBindingSet *binding_set)
{
  GdkModifierType modifiers;

  modifiers = GDK_CONTROL_MASK;
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, modifiers,
                                "select-all", 0);

  modifiers = GDK_CONTROL_MASK | GDK_SHIFT_MASK;
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, modifiers,
                                "unselect-all", 0);
}

static void
gf_icon_view_class_init (GfIconViewClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);

  object_class->dispose = gf_icon_view_dispose;
  object_class->finalize = gf_icon_view_finalize;

  widget_class->draw = gf_icon_view_draw;

  install_signals ();

  binding_set = gtk_binding_set_by_class (widget_class);
  add_bindings (binding_set);
}

static void
gf_icon_view_init (GfIconView *self)
{
  const char *desktop_dir;
  GError *error;
  GdkDisplay *display;
  int n_monitors;
  int i;

  g_signal_connect (self, "select-all", G_CALLBACK (select_all_cb), NULL);
  g_signal_connect (self, "unselect-all", G_CALLBACK (unselect_all_cb), NULL);

  add_event_filter (self);

  self->multi_press = gtk_gesture_multi_press_new (GTK_WIDGET (self));
  self->drag = gtk_gesture_drag_new (GTK_WIDGET (self));

  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->multi_press), 0);

  g_signal_connect (self->multi_press, "pressed",
                    G_CALLBACK (multi_press_pressed_cb),
                    self);

  g_signal_connect (self->drag, "drag-begin",
                    G_CALLBACK (drag_begin_cb),
                    self);

  g_signal_connect (self->drag, "drag-end",
                    G_CALLBACK (drag_end_cb),
                    self);

  g_signal_connect (self->drag, "drag-update",
                    G_CALLBACK (drag_update_cb),
                    self);

  desktop_dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  self->desktop = g_file_new_for_path (desktop_dir);

  error = NULL;
  self->monitor = g_file_monitor_directory (self->desktop,
                                            G_FILE_MONITOR_WATCH_MOVES,
                                            NULL,
                                            &error);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (self->monitor, "changed",
                    G_CALLBACK (desktop_changed_cb),
                    self);

  self->settings = g_settings_new ("org.gnome.gnome-flashback.desktop.icons");

  self->fixed = gtk_fixed_new ();
  gtk_container_add (GTK_CONTAINER (self), self->fixed);
  gtk_widget_show (self->fixed);

  self->dummy_icon = create_dummy_icon (self);
  gtk_widget_set_parent (self->dummy_icon, GTK_WIDGET (self));
  gtk_widget_show (self->dummy_icon);

  self->cancellable = g_cancellable_new ();

  gf_nautilus_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                     G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION,
                                     "org.gnome.Nautilus",
                                     "/org/gnome/Nautilus",
                                     self->cancellable,
                                     nautilus_ready_cb,
                                     self);

  gf_file_manager_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION,
                                         "org.freedesktop.FileManager1",
                                         "/org/freedesktop/FileManager1",
                                         self->cancellable,
                                         file_manager_ready_cb,
                                         self);

  display = gdk_display_get_default ();
  n_monitors = gdk_display_get_n_monitors (display);

  g_signal_connect_object (display, "monitor-added",
                           G_CALLBACK (monitor_added_cb),
                           self, 0);

  g_signal_connect_object (display, "monitor-removed",
                           G_CALLBACK (monitor_removed_cb),
                           self, 0);

  for (i = 0; i < n_monitors; i++)
    {
      GdkMonitor *monitor;

      monitor = gdk_display_get_monitor (display, i);
      create_monitor_view (self, monitor);
    }

  enumerate_desktop (self);
}

GtkWidget *
gf_icon_view_new (void)
{
  return g_object_new (GF_TYPE_ICON_VIEW,
                       "can-focus", TRUE,
                       NULL);
}

void
gf_icon_view_set_representative_color (GfIconView *self,
                                       GdkRGBA    *color)
{
  ensure_rubberband_style (self);

  if (color != NULL)
    {
      double shade;
      GdkRGBA background;
      GdkRGBA border;
      char *background_css;
      char *border_css;
      char *css;

      shade = color->green < 0.5 ? 1.1 : 0.9;

      background = *color;
      background.alpha = 0.6;

      border.red = color->red * shade;
      border.green = color->green * shade;
      border.blue = color->green * shade;
      border.alpha = 1.0;

      background_css = gdk_rgba_to_string (&background);
      border_css = gdk_rgba_to_string (&border);

      css = g_strdup_printf (".rubberband { background-color: %s; border: 1px solid %s; }",
                             background_css, border_css);

      g_free (background_css);
      g_free (border_css);

      gtk_css_provider_load_from_data (self->rubberband_css, css, -1, NULL);
      g_free (css);
    }
  else
    {
      gtk_css_provider_load_from_data (self->rubberband_css, "", -1, NULL);
    }
}

void
gf_icon_view_clear_selection (GfIconView *self)
{
  unselect_icons (self);
}

GList *
gf_icon_view_get_selected_icons (GfIconView *self)
{
  return self->selected_icons;
}

void
gf_icon_view_show_item_properties (GfIconView         *self,
                                   const char * const *uris)
{
  if (self->file_manager == NULL)
    return;

  gf_file_manager_gen_call_show_item_properties (self->file_manager,
                                                 uris, "",
                                                 self->cancellable,
                                                 show_item_properties_cb,
                                                 NULL);
}
