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
#include "gf-monitor-view.h"

struct _GfMonitorView
{
  GtkFixed    parent;

  GdkMonitor *monitor;

  guint       icon_size;
  guint       extra_text_width;
  guint       column_spacing;
  guint       row_spacing;

  int         width;
  int         height;
};

enum
{
  PROP_0,

  PROP_MONITOR,

  PROP_ICON_SIZE,
  PROP_EXTRA_TEXT_WIDTH,
  PROP_COLUMN_SPACING,
  PROP_ROW_SPACING,

  LAST_PROP
};

static GParamSpec *view_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfMonitorView, gf_monitor_view, GTK_TYPE_FIXED)

static void
set_icon_size (GfMonitorView *self,
               guint          icon_size)
{
  if (self->icon_size == icon_size)
    return;

  self->icon_size = icon_size;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
set_extra_text_width (GfMonitorView *self,
                      guint          extra_text_width)
{
  if (self->extra_text_width == extra_text_width)
    return;

  self->extra_text_width = extra_text_width;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
set_column_spacing (GfMonitorView *self,
                    guint          column_spacing)
{
  if (self->column_spacing == column_spacing)
    return;

  self->column_spacing = column_spacing;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
set_row_spacing (GfMonitorView *self,
                 guint          row_spacing)
{
  if (self->row_spacing == row_spacing)
    return;

  self->row_spacing = row_spacing;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
workarea_cb (GdkMonitor    *monitor,
             GParamSpec    *pspec,
             GfMonitorView *self)
{
  GdkRectangle workarea;

  gdk_monitor_get_workarea (monitor, &workarea);

  self->width = workarea.width;
  self->height = workarea.height;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gf_monitor_view_constructed (GObject *object)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (object);

  G_OBJECT_CLASS (gf_monitor_view_parent_class)->constructed (object);

  g_signal_connect_object (self->monitor, "notify::workarea",
                           G_CALLBACK (workarea_cb),
                           self, 0);

  workarea_cb (self->monitor, NULL, self);
}

static void
gf_monitor_view_dispose (GObject *object)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (object);

  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (gf_monitor_view_parent_class)->dispose (object);
}

static void
gf_monitor_view_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (object);

  switch (property_id)
    {
      case PROP_ICON_SIZE:
        g_value_set_uint (value, self->icon_size);
        break;

      case PROP_EXTRA_TEXT_WIDTH:
        g_value_set_uint (value, self->extra_text_width);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_monitor_view_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (object);

  switch (property_id)
    {
      case PROP_MONITOR:
        g_assert (self->monitor == NULL);
        self->monitor = g_value_dup_object (value);
        break;

      case PROP_ICON_SIZE:
        set_icon_size (self, g_value_get_uint (value));
        break;

      case PROP_EXTRA_TEXT_WIDTH:
        set_extra_text_width (self, g_value_get_uint (value));
        break;

      case PROP_COLUMN_SPACING:
        set_column_spacing (self, g_value_get_uint (value));
        break;

      case PROP_ROW_SPACING:
        set_row_spacing (self, g_value_get_uint (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_monitor_view_get_preferred_height (GtkWidget *widget,
                                      gint      *minimum_height,
                                      gint      *natural_height)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (widget);

  *minimum_height = self->height;
  *natural_height = self->height;
}

static void
gf_monitor_view_get_preferred_width (GtkWidget *widget,
                                     gint      *minimum_width,
                                     gint      *natural_width)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (widget);

  *minimum_width = self->width;
  *natural_width = self->width;
}

static GtkSizeRequestMode
gf_monitor_view_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
install_properties (GObjectClass *object_class)
{
  view_properties[PROP_MONITOR] =
    g_param_spec_object ("monitor",
                         "monitor",
                         "monitor",
                         GDK_TYPE_MONITOR,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  view_properties[PROP_ICON_SIZE] =
    g_param_spec_uint ("icon-size",
                       "icon-size",
                       "icon-size",
                       16, 128, 48,
                       G_PARAM_CONSTRUCT |
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  view_properties[PROP_EXTRA_TEXT_WIDTH] =
    g_param_spec_uint ("extra-text-width",
                       "extra-text-width",
                       "extra-text-width",
                       0, 100, 48,
                       G_PARAM_CONSTRUCT |
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  view_properties[PROP_COLUMN_SPACING] =
    g_param_spec_uint ("column-spacing",
                       "column-spacing",
                       "column-spacing",
                       0, 100, 10,
                       G_PARAM_CONSTRUCT |
                       G_PARAM_WRITABLE |
                       G_PARAM_STATIC_STRINGS);

  view_properties[PROP_ROW_SPACING] =
    g_param_spec_uint ("row-spacing",
                       "row-spacing",
                       "row-spacing",
                       0, 100, 10,
                       G_PARAM_CONSTRUCT |
                       G_PARAM_WRITABLE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, view_properties);
}

static void
gf_monitor_view_class_init (GfMonitorViewClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);

  object_class->constructed = gf_monitor_view_constructed;
  object_class->dispose = gf_monitor_view_dispose;
  object_class->get_property = gf_monitor_view_get_property;
  object_class->set_property = gf_monitor_view_set_property;

  widget_class->get_preferred_height = gf_monitor_view_get_preferred_height;
  widget_class->get_preferred_width = gf_monitor_view_get_preferred_width;
  widget_class->get_request_mode = gf_monitor_view_get_request_mode;

  install_properties (object_class);
}

static void
gf_monitor_view_init (GfMonitorView *self)
{
}

GtkWidget *
gf_monitor_view_new (GdkMonitor *monitor,
                     guint       icon_size,
                     guint       extra_text_width,
                     guint       column_spacing,
                     guint       row_spacing)
{
  return g_object_new (GF_TYPE_MONITOR_VIEW,
                       "monitor", monitor,
                       "icon-size", icon_size,
                       "extra-text-width", extra_text_width,
                       "column-spacing", column_spacing,
                       "row-spacing", row_spacing,
                       NULL);
}

GdkMonitor *
gf_monitor_view_get_monitor (GfMonitorView *self)
{
  return self->monitor;
}

gboolean
gf_monitor_view_is_primary (GfMonitorView *self)
{
  return gdk_monitor_is_primary (self->monitor);
}
