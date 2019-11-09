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

#include "gf-desktop-enum-types.h"
#include "gf-icon.h"
#include "gf-monitor-view.h"
#include "gf-utils.h"

struct _GfIconView
{
  GtkEventBox   parent;

  GFile        *desktop;

  GSettings    *settings;

  GtkWidget    *fixed;

  GCancellable *cancellable;

  GList        *icons;

  guint         add_icons_id;
};

G_DEFINE_TYPE (GfIconView, gf_icon_view, GTK_TYPE_EVENT_BOX)

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
      while (view != NULL)
        {
          if (!gf_monitor_view_add_icon (GF_MONITOR_VIEW (view->data), l->data))
            {
              view = view->next;
              continue;
            }

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
view_foreach_cb (GtkWidget *widget,
                 gpointer   user_data)
{
  gtk_container_remove (GTK_CONTAINER (user_data), widget);
}

static void
size_changed_cb (GtkWidget  *view,
                 GfIconView *self)
{
  gtk_container_foreach (GTK_CONTAINER (view), view_foreach_cb, view);
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
      GtkWidget *icon;

      info = l->data;
      file = g_file_enumerator_get_child (enumerator, info);

      icon = gf_icon_new (file, info);
      g_object_unref (file);

      g_settings_bind (self->settings, "icon-size",
                       icon, "icon-size",
                       G_SETTINGS_BIND_GET);

      g_settings_bind (self->settings, "extra-text-width",
                       icon, "extra-text-width",
                       G_SETTINGS_BIND_GET);

      self->icons = g_list_prepend (self->icons, g_object_ref_sink (icon));
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

  attributes = gf_build_attributes_list (G_FILE_ATTRIBUTE_STANDARD_NAME,
                                         G_FILE_ATTRIBUTE_STANDARD_ICON,
                                         NULL);

  self->cancellable = g_cancellable_new ();

  g_file_enumerate_children_async (self->desktop,
                                   attributes,
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_LOW,
                                   self->cancellable,
                                   enumerate_children_cb,
                                   self);

  g_free (attributes);
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
workarea_cb (GdkMonitor *monitor,
             GParamSpec *pspec,
             GfIconView *self)
{
  GtkWidget *view;
  GdkRectangle workarea;

  view = find_monitor_view_by_monitor (self, monitor);
  if (view == NULL)
    return;

  gdk_monitor_get_workarea (monitor, &workarea);

  gtk_fixed_move (GTK_FIXED (self->fixed), view, workarea.x, workarea.y);
}

static void
create_monitor_view (GfIconView *self,
                     GdkMonitor *monitor)
{
  GfIconSize icon_size;
  guint extra_text_width;
  guint column_spacing;
  guint row_spacing;
  GdkRectangle workarea;
  GtkWidget *view;

  icon_size = g_settings_get_enum (self->settings, "icon-size");
  extra_text_width = g_settings_get_uint (self->settings, "extra-text-width");
  column_spacing = g_settings_get_uint (self->settings, "column-spacing");
  row_spacing = g_settings_get_uint (self->settings, "row-spacing");

  gdk_monitor_get_workarea (monitor, &workarea);

  view = gf_monitor_view_new (monitor,
                              icon_size,
                              extra_text_width,
                              column_spacing,
                              row_spacing);

  g_signal_connect (view, "size-changed", G_CALLBACK (size_changed_cb), self);

  gtk_fixed_put (GTK_FIXED (self->fixed), view, workarea.x, workarea.y);
  gtk_widget_show (view);

  g_settings_bind (self->settings, "icon-size",
                   view, "icon-size",
                   G_SETTINGS_BIND_GET);

  g_settings_bind (self->settings, "extra-text-width",
                   view, "extra-text-width",
                   G_SETTINGS_BIND_GET);

  g_settings_bind (self->settings, "column-spacing",
                   view, "column-spacing",
                   G_SETTINGS_BIND_GET);

  g_settings_bind (self->settings, "row-spacing",
                   view, "row-spacing",
                   G_SETTINGS_BIND_GET);

  g_signal_connect_object (monitor, "notify::workarea",
                           G_CALLBACK (workarea_cb),
                           self, 0);
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

static void
gf_icon_view_dispose (GObject *object)
{
  GfIconView *self;

  self = GF_ICON_VIEW (object);

  g_clear_object (&self->desktop);
  g_clear_object (&self->settings);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (self->icons != NULL)
    {
      g_list_free_full (self->icons, g_object_unref);
      self->icons = NULL;
    }

  G_OBJECT_CLASS (gf_icon_view_parent_class)->dispose (object);
}

static void
gf_icon_view_finalize (GObject *object)
{
  GfIconView *self;

  self = GF_ICON_VIEW (object);

  if (self->add_icons_id != 0)
    {
      g_source_remove (self->add_icons_id);
      self->add_icons_id = 0;
    }

  G_OBJECT_CLASS (gf_icon_view_parent_class)->finalize (object);
}

static void
gf_icon_view_class_init (GfIconViewClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_icon_view_dispose;
  object_class->finalize = gf_icon_view_finalize;
}

static void
gf_icon_view_init (GfIconView *self)
{
  const char *desktop_dir;
  GdkDisplay *display;
  int n_monitors;
  int i;

  desktop_dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  self->desktop = g_file_new_for_path (desktop_dir);

  self->settings = g_settings_new ("org.gnome.gnome-flashback.desktop.icons");

  self->fixed = gtk_fixed_new ();
  gtk_container_add (GTK_CONTAINER (self), self->fixed);
  gtk_widget_show (self->fixed);

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
  return g_object_new (GF_TYPE_ICON_VIEW, NULL);
}
