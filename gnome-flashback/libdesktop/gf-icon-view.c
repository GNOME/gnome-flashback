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
#include "gf-monitor-view.h"

struct _GfIconView
{
  GtkEventBox  parent;

  GSettings   *settings;

  GtkWidget   *fixed;
};

G_DEFINE_TYPE (GfIconView, gf_icon_view, GTK_TYPE_EVENT_BOX)

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

static gboolean
enum_to_uint (GValue   *value,
              GVariant *variant,
              gpointer  user_data)
{
  const char *nick;
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  nick = g_variant_get_string (variant, NULL);

  enum_class = g_type_class_ref (GF_TYPE_ICON_SIZE);
  enum_value = g_enum_get_value_by_nick (enum_class, nick);
  g_type_class_unref (enum_class);

  if (enum_value != NULL)
    {
      g_value_set_uint (value, enum_value->value);
      return TRUE;
    }

  return FALSE;
}

static void
create_monitor_view (GfIconView *self,
                     GdkMonitor *monitor)
{
  guint icon_size;
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

  gtk_fixed_put (GTK_FIXED (self->fixed), view, workarea.x, workarea.y);
  gtk_widget_show (view);

  g_settings_bind_with_mapping (self->settings, "icon-size",
                                view, "icon-size",
                                G_SETTINGS_BIND_GET,
                                enum_to_uint,
                                NULL,
                                self,
                                NULL);

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

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gf_icon_view_parent_class)->dispose (object);
}

static void
gf_icon_view_class_init (GfIconViewClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_icon_view_dispose;
}

static void
gf_icon_view_init (GfIconView *self)
{
  GdkDisplay *display;
  int n_monitors;
  int i;

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
}

GtkWidget *
gf_icon_view_new (void)
{
  return g_object_new (GF_TYPE_ICON_VIEW, NULL);
}
