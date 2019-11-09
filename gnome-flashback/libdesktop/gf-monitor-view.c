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

#include "gf-desktop-enum-types.h"
#include "gf-icon.h"
#include "gf-utils.h"

struct _GfMonitorView
{
  GtkFixed    parent;

  GdkMonitor *monitor;

  gboolean    grid_points;

  GfIconSize  icon_size;
  guint       extra_text_width;
  guint       column_spacing;
  guint       row_spacing;

  guint       grid_size_id;

  GtkWidget  *dummy_icon;

  int         view_width;
  int         view_height;

  int         columns;
  int         rows;

  int         spacing_x;
  int         spacing_y;

  int         offset_x;
  int         offset_y;

  int        *grid;
};

enum
{
  PROP_0,

  PROP_MONITOR,

  PROP_GRID_POINTS,

  PROP_ICON_SIZE,
  PROP_EXTRA_TEXT_WIDTH,
  PROP_COLUMN_SPACING,
  PROP_ROW_SPACING,

  LAST_PROP
};

static GParamSpec *view_properties[LAST_PROP] = { NULL };

enum
{
  SIZE_CHANGED,

  LAST_SIGNAL
};

static guint view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfMonitorView, gf_monitor_view, GTK_TYPE_FIXED)

static gboolean
find_free_grid_position (GfMonitorView *self,
                         int           *column_out,
                         int           *row_out)
{
  int column;
  int row;

  for (column = 0; column < self->columns; column++)
    {
      for (row = 0; row < self->rows; row++)
        {
          if (self->grid[column * self->rows + row] == 0)
            {
              *column_out = column;
              *row_out = row;

              return TRUE;
            }
        }
    }

  return FALSE;
}

static void
icon_destroy_cb (GtkWidget *widget,
                 GFile     *file)
{
  g_file_delete (file, NULL, NULL);
  g_object_unref (file);
}

static GtkWidget *
create_dummy_icon (GfMonitorView *self)
{
  GFileIOStream *iostream;
  GError *error;
  GFile *file;
  char *attributes;
  GFileInfo *info;
  GIcon *icon;
  const char *name;
  GtkWidget *widget;

  iostream = NULL;
  error = NULL;

  file = g_file_new_tmp ("dummy-desktop-file-XXXXXX", &iostream, &error);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return NULL;
    }

  attributes = gf_build_attributes_list (G_FILE_ATTRIBUTE_STANDARD_NAME,
                                         G_FILE_ATTRIBUTE_STANDARD_ICON,
                                         NULL);

  info = g_file_io_stream_query_info (iostream, attributes, NULL, &error);
  g_object_unref (iostream);
  g_free (attributes);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return NULL;
    }

  icon = g_icon_new_for_string ("text-x-generic", NULL);
  g_file_info_set_icon (info, icon);
  g_object_unref (icon);

  name = "Lorem Ipsum is simply dummy text of the printing and typesetting "
         "industry. Lorem Ipsum has been the industry's standard dummy text "
         "ever since the 1500s, when an unknown printer took a galley of "
         "type and scrambled it to make a type specimen book.";

  g_file_info_set_name (info, name);

  widget = gf_icon_new (file, info);
  g_object_unref (info);

  g_object_bind_property (self, "icon-size",
                          widget, "icon-size",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (self, "extra-text-width",
                          widget, "extra-text-width",
                          G_BINDING_DEFAULT |
                          G_BINDING_SYNC_CREATE);

  g_signal_connect (widget, "destroy",
                    G_CALLBACK (icon_destroy_cb),
                    g_object_ref (file));

  g_object_unref (file);

  return widget;
}

static void
calculate_grid_size (GfMonitorView *self)
{
  GtkRequisition icon_size;
  int columns;
  int rows;

  if (self->dummy_icon == NULL)
    {
      self->dummy_icon = create_dummy_icon (self);
      gtk_widget_show (self->dummy_icon);
    }

  if (self->dummy_icon == NULL)
    return;

  gtk_widget_get_preferred_size (self->dummy_icon, &icon_size, NULL);

  columns = self->view_width / icon_size.width;
  rows = self->view_height / icon_size.height;

  while (TRUE)
    {
      int spacing;

      spacing = (columns - 1) * self->column_spacing;
      if (spacing + columns * icon_size.width <= self->view_width ||
          columns == 1)
        break;

      columns--;
    }

  while (TRUE)
    {
      int spacing;

      spacing = (rows - 1) * self->row_spacing;
      if (spacing + rows * icon_size.height <= self->view_height ||
          rows == 1)
        break;

      rows--;
    }

  self->columns = columns;
  self->rows = rows;

  self->spacing_x = icon_size.width + self->column_spacing;
  self->spacing_y = icon_size.height + self->row_spacing;

  self->offset_x = (self->view_width - columns * icon_size.width -
                    (columns - 1) * self->column_spacing) / 2;
  self->offset_y = (self->view_height - rows * icon_size.height -
                    (rows - 1) * self->row_spacing) / 2;

  g_clear_pointer (&self->grid, g_free);
  self->grid = g_new0 (int, columns * rows);

  g_signal_emit (self, view_signals[SIZE_CHANGED], 0);

  if (self->grid_points)
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
recalculate_grid_size_cb (gpointer user_data)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (user_data);

  calculate_grid_size (self);
  self->grid_size_id = 0;

  return G_SOURCE_REMOVE;
}

static void
recalculate_grid_size (GfMonitorView *self)
{
  if (self->grid_size_id != 0)
    return;

  self->grid_size_id = g_idle_add (recalculate_grid_size_cb, self);

  g_source_set_name_by_id (self->grid_size_id,
                           "[gnome-flashback] recalculate_grid_size_cb");
}

static void
set_icon_size (GfMonitorView *self,
               guint          icon_size)
{
  if (self->icon_size == icon_size)
    return;

  self->icon_size = icon_size;

  recalculate_grid_size (self);
}

static void
set_extra_text_width (GfMonitorView *self,
                      guint          extra_text_width)
{
  if (self->extra_text_width == extra_text_width)
    return;

  self->extra_text_width = extra_text_width;

  recalculate_grid_size (self);
}

static void
set_column_spacing (GfMonitorView *self,
                    guint          column_spacing)
{
  if (self->column_spacing == column_spacing)
    return;

  self->column_spacing = column_spacing;

  recalculate_grid_size (self);
}

static void
set_row_spacing (GfMonitorView *self,
                 guint          row_spacing)
{
  if (self->row_spacing == row_spacing)
    return;

  self->row_spacing = row_spacing;

  recalculate_grid_size (self);
}

static void
workarea_cb (GdkMonitor    *monitor,
             GParamSpec    *pspec,
             GfMonitorView *self)
{
  GdkRectangle workarea;

  gdk_monitor_get_workarea (monitor, &workarea);

  self->view_width = workarea.width;
  self->view_height = workarea.height;

  recalculate_grid_size (self);
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
gf_monitor_view_finalize (GObject *object)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (object);

  g_clear_pointer (&self->dummy_icon, gtk_widget_destroy);

  if (self->grid_size_id != 0)
    {
      g_source_remove (self->grid_size_id);
      self->grid_size_id = 0;
    }

  g_clear_pointer (&self->grid, g_free);

  G_OBJECT_CLASS (gf_monitor_view_parent_class)->finalize (object);
}

static gboolean
gf_monitor_view_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (widget);

  if (self->grid_points)
    {
      int c;

      cairo_save (cr);
      cairo_set_line_width (cr, 1);

      for (c = 0; c < self->columns; c++)
        {
          int r;

          for (r = 0; r < self->rows; r++)
            {
              int x;
              int y;

              x = self->offset_x + c * self->spacing_x + self->spacing_x / 2;
              y = self->offset_y + r * self->spacing_y + self->spacing_y / 2;

              cairo_move_to (cr, x - 3, y);
              cairo_line_to (cr, x + 3, y);

              cairo_move_to (cr, x, y - 3);
              cairo_line_to (cr, x, y + 3);
            }
        }

      cairo_stroke (cr);
      cairo_restore (cr);
    }

  return GTK_WIDGET_CLASS (gf_monitor_view_parent_class)->draw (widget, cr);
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
      case PROP_GRID_POINTS:
        g_value_set_boolean (value, self->grid_points);
        break;

      case PROP_ICON_SIZE:
        g_value_set_enum (value, self->icon_size);
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

      case PROP_GRID_POINTS:
        self->grid_points = g_value_get_boolean (value);
        gtk_widget_queue_draw (GTK_WIDGET (self));
        break;

      case PROP_ICON_SIZE:
        set_icon_size (self, g_value_get_enum (value));
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

  *minimum_height = self->view_height;
  *natural_height = self->view_height;
}

static void
gf_monitor_view_get_preferred_width (GtkWidget *widget,
                                     gint      *minimum_width,
                                     gint      *natural_width)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (widget);

  *minimum_width = self->view_width;
  *natural_width = self->view_width;
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

  view_properties[PROP_GRID_POINTS] =
    g_param_spec_boolean ("grid-points",
                          "grid-points",
                          "grid-points",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS);

  view_properties[PROP_ICON_SIZE] =
    g_param_spec_enum ("icon-size",
                       "icon-size",
                       "icon-size",
                       GF_TYPE_ICON_SIZE,
                       GF_ICON_SIZE_48PX,
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
install_signals (void)
{
  view_signals[SIZE_CHANGED] =
    g_signal_new ("size-changed", GF_TYPE_MONITOR_VIEW, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
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
  object_class->finalize = gf_monitor_view_finalize;
  object_class->get_property = gf_monitor_view_get_property;
  object_class->set_property = gf_monitor_view_set_property;

  widget_class->draw = gf_monitor_view_draw;
  widget_class->get_preferred_height = gf_monitor_view_get_preferred_height;
  widget_class->get_preferred_width = gf_monitor_view_get_preferred_width;
  widget_class->get_request_mode = gf_monitor_view_get_request_mode;

  install_properties (object_class);
  install_signals ();
}

static void
gf_monitor_view_init (GfMonitorView *self)
{
}

GtkWidget *
gf_monitor_view_new (GdkMonitor *monitor,
                     GfIconSize  icon_size,
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

gboolean
gf_monitor_view_add_icon (GfMonitorView *self,
                          GtkWidget     *icon)
{
  int column;
  int row;
  int x;
  int y;

  if (!find_free_grid_position (self, &column, &row))
    return FALSE;

  x = self->offset_x + column * self->spacing_x;
  y = self->offset_y + row * self->spacing_y;

  self->grid[column * self->rows + row] = 1;

  gtk_fixed_put (GTK_FIXED (self), icon, x, y);
  gtk_widget_show (icon);

  return TRUE;
}
