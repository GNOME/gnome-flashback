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

  int         width;
  int         height;
};

enum
{
  PROP_0,

  PROP_MONITOR,

  LAST_PROP
};

static GParamSpec *view_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfMonitorView, gf_monitor_view, GTK_TYPE_FIXED)

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
gf_monitor_view_new (GdkMonitor *monitor)
{
  return g_object_new (GF_TYPE_MONITOR_VIEW,
                       "monitor", monitor,
                       NULL);
}

GdkMonitor *
gf_monitor_view_get_monitor (GfMonitorView *self)
{
  return self->monitor;
}
