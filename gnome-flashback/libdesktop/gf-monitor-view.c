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
#include "gf-dummy-icon.h"
#include "gf-icon.h"

struct _GfMonitorView
{
  GtkFixed     parent;

  GdkMonitor  *monitor;

  gboolean     grid_points;

  GfDummyIcon *dummy_icon;
  guint        column_spacing;
  guint        row_spacing;

  GfPlacement  placement;

  guint        grid_size_id;

  int          view_width;
  int          view_height;

  int          icon_width;
  int          icon_height;

  int          columns;
  int          rows;

  int          spacing_x;
  int          spacing_y;

  int          offset_x;
  int          offset_y;

  GHashTable  *grid;
};

enum
{
  PROP_0,

  PROP_MONITOR,

  PROP_GRID_POINTS,

  PROP_DUMMY_ICON,
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

static GfIcon *
find_first_icon (GfMonitorView *self)
{
  int column;
  int row;

  for (column = 0; column < self->columns; column++)
    {
      for (row = 0; row < self->rows; row++)
        {
          gpointer key;
          GPtrArray *array;

          key = GINT_TO_POINTER (column * self->rows + row);
          array = g_hash_table_lookup (self->grid, key);

          if (array == NULL || array->len == 0)
            continue;

          return array->pdata[0];
        }
    }

  return NULL;
}

static gboolean
find_icon_grid_position (GfMonitorView *self,
                         GfIcon        *icon,
                         int           *column_out,
                         int           *row_out)
{
  int column;
  int row;

  for (column = 0; column < self->columns; column++)
    {
      for (row = 0; row < self->rows; row++)
        {
          gpointer key;
          GPtrArray *array;

          key = GINT_TO_POINTER (column * self->rows + row);
          array = g_hash_table_lookup (self->grid, key);

          if (array == NULL || array->len == 0)
            continue;

          if (g_ptr_array_find (array, icon, NULL))
            {
              *column_out = column;
              *row_out = row;

              return TRUE;
            }
        }
    }

  return FALSE;
}

static GfIcon *
find_icon_up (GfMonitorView *self,
              int            start_column,
              int            start_row)
{
  int row;

  for (row = start_row - 1; row >= 0; row--)
    {
      gpointer key;
      GPtrArray *array;

      key = GINT_TO_POINTER (start_column * self->rows + row);
      array = g_hash_table_lookup (self->grid, key);

      if (array == NULL || array->len == 0)
        continue;

      return array->pdata[0];
    }

  return NULL;
}

static GfIcon *
find_icon_down (GfMonitorView *self,
                int            start_column,
                int            start_row)
{
  int row;

  for (row = start_row + 1; row < self->rows; row++)
    {
      gpointer key;
      GPtrArray *array;

      key = GINT_TO_POINTER (start_column * self->rows + row);
      array = g_hash_table_lookup (self->grid, key);

      if (array == NULL || array->len == 0)
        continue;

      return array->pdata[0];
    }

  return NULL;
}

static GfIcon *
find_icon_left (GfMonitorView *self,
                int            start_column,
                int            start_row)
{
  int column;

  for (column = start_column - 1; column >= 0; column--)
    {
      gpointer key;
      GPtrArray *array;

      key = GINT_TO_POINTER (column * self->rows + start_row);
      array = g_hash_table_lookup (self->grid, key);

      if (array == NULL || array->len == 0)
        continue;

      return array->pdata[0];
    }

  return NULL;
}

static GfIcon *
find_icon_right (GfMonitorView *self,
                 int            start_column,
                 int            start_row)
{
  int column;

  for (column = start_column + 1; column < self->columns; column++)
    {
      gpointer key;
      GPtrArray *array;

      key = GINT_TO_POINTER (column * self->rows + start_row);
      array = g_hash_table_lookup (self->grid, key);

      if (array == NULL || array->len == 0)
        continue;

      return array->pdata[0];
    }

  return NULL;
}

static GfIcon *
find_next_icon (GfMonitorView    *self,
                GfIcon           *next_to,
                GtkDirectionType  direction)
{
  int column;
  int row;
  GfIcon *next_icon;

  if (!find_icon_grid_position (self, next_to, &column, &row))
    return NULL;

  next_icon = NULL;

  switch (direction)
    {
      case GTK_DIR_UP:
        next_icon = find_icon_up (self, column, row);
        break;

      case GTK_DIR_DOWN:
        next_icon = find_icon_down (self, column, row);
        break;

      case GTK_DIR_LEFT:
        next_icon = find_icon_left (self, column, row);
        break;

      case GTK_DIR_RIGHT:
        next_icon = find_icon_right (self, column, row);
        break;

      case GTK_DIR_TAB_FORWARD:
      case GTK_DIR_TAB_BACKWARD:
      default:
        break;
    }

  return next_icon;
}

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
          gpointer key;
          GPtrArray *array;

          key = GINT_TO_POINTER (column * self->rows + row);
          array = g_hash_table_lookup (self->grid, key);

          if (array == NULL || array->len == 0)
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
calculate_grid_size (GfMonitorView *self)
{
  int icon_width;
  int icon_height;
  int columns;
  int rows;
  int spacing_x;
  int spacing_y;
  int offset_x;
  int offset_y;
  gboolean changed;

  icon_width = gf_dummy_icon_get_width (self->dummy_icon);
  icon_height = gf_dummy_icon_get_height (self->dummy_icon);

  columns = self->view_width / icon_width;
  rows = self->view_height / icon_height;

  while (TRUE)
    {
      int spacing;

      spacing = (columns - 1) * self->column_spacing;
      if (spacing + columns * icon_width <= self->view_width ||
          columns == 1)
        break;

      columns--;
    }

  while (TRUE)
    {
      int spacing;

      spacing = (rows - 1) * self->row_spacing;
      if (spacing + rows * icon_height <= self->view_height ||
          rows == 1)
        break;

      rows--;
    }

  spacing_x = icon_width + self->column_spacing;
  spacing_y = icon_height + self->row_spacing;

  offset_x = (self->view_width - columns * icon_width -
              (columns - 1) * self->column_spacing) / 2;
  offset_y = (self->view_height - rows * icon_height -
              (rows - 1) * self->row_spacing) / 2;

  changed = FALSE;
  if (self->icon_width != icon_width ||
      self->icon_height != icon_height ||
      self->columns != columns ||
      self->rows != rows ||
      self->spacing_x != spacing_x ||
      self->spacing_y != spacing_y ||
      self->offset_x != offset_x ||
      self->offset_y != offset_y)
    changed = TRUE;

  self->icon_width = icon_width;
  self->icon_height = icon_height;

  self->columns = columns;
  self->rows = rows;

  self->spacing_x = spacing_x;
  self->spacing_y = spacing_y;

  self->offset_x = offset_x;
  self->offset_y = offset_y;

  if (!changed)
    return;

  g_hash_table_remove_all (self->grid);

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
dummy_icon_size_changed_cb (GtkWidget     *widget,
                            GfMonitorView *self)
{
  recalculate_grid_size (self);
}

static void
gf_monitor_view_constructed (GObject *object)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (object);

  G_OBJECT_CLASS (gf_monitor_view_parent_class)->constructed (object);

  g_signal_connect (self->dummy_icon, "size-changed",
                    G_CALLBACK (dummy_icon_size_changed_cb),
                    self);
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

  if (self->grid_size_id != 0)
    {
      g_source_remove (self->grid_size_id);
      self->grid_size_id = 0;
    }

  g_clear_pointer (&self->grid, g_hash_table_destroy);

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

              x = self->offset_x + c * self->spacing_x + self->icon_width / 2;
              y = self->offset_y + r * self->spacing_y + self->icon_height / 2;

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

      case PROP_DUMMY_ICON:
        g_assert (self->dummy_icon == NULL);
        self->dummy_icon = g_value_get_object (value);
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

  view_properties[PROP_DUMMY_ICON] =
    g_param_spec_object ("dummy-icon",
                         "dummy-icon",
                         "dummy-icon",
                         GF_TYPE_DUMMY_ICON,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
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
  self->grid = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
                                      (GDestroyNotify) g_ptr_array_unref);
}

GtkWidget *
gf_monitor_view_new (GdkMonitor  *monitor,
                     GfDummyIcon *dummy_icon,
                     guint        column_spacing,
                     guint        row_spacing)
{
  return g_object_new (GF_TYPE_MONITOR_VIEW,
                       "monitor", monitor,
                       "dummy-icon", dummy_icon,
                       "column-spacing", column_spacing,
                       "row-spacing", row_spacing,
                       NULL);
}

void
gf_monitor_view_set_placement (GfMonitorView *self,
                               GfPlacement    placement)
{
  self->placement = placement;
}

void
gf_monitor_view_set_size (GfMonitorView *self,
                          int            width,
                          int            height)
{
  if (self->view_width == width &&
      self->view_height == height)
    return;

  self->view_width = width;
  self->view_height = height;

  recalculate_grid_size (self);
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
  gpointer key;
  GPtrArray *array;
  int x;
  int y;

  if (!find_free_grid_position (self, &column, &row))
    return FALSE;

  key = GINT_TO_POINTER (column * self->rows + row);
  array = g_hash_table_lookup (self->grid, key);

  if (array == NULL)
    {
      array = g_ptr_array_new ();
      g_hash_table_insert (self->grid, key, array);
    }

  g_ptr_array_add (array, icon);

  x = self->offset_x + column * self->spacing_x;
  y = self->offset_y + row * self->spacing_y;

  gtk_fixed_put (GTK_FIXED (self), icon, x, y);
  gtk_widget_show (icon);

  return TRUE;
}

void
gf_monitor_view_remove_icon (GfMonitorView *self,
                             GtkWidget     *icon)
{
  int column;
  int row;

  for (column = 0; column < self->columns; column++)
    {
      for (row = 0; row < self->rows; row++)
        {
          gpointer key;
          GPtrArray *array;

          key = GINT_TO_POINTER (column * self->rows + row);
          array = g_hash_table_lookup (self->grid, key);

          if (array == NULL || array->len == 0)
            continue;

          g_ptr_array_remove (array, icon);
        }
    }

  gtk_container_remove (GTK_CONTAINER (self), icon);
  gtk_widget_hide (icon);
}

GList *
gf_monitor_view_get_icons (GfMonitorView *self,
                           GdkRectangle  *rect)
{
  GList *icons;
  GList *children;
  GList *l;

  icons = NULL;

  children = gtk_container_get_children (GTK_CONTAINER (self));

  for (l = children; l != NULL; l = l->next)
    {
      GtkWidget *icon;
      GtkAllocation allocation;

      icon = l->data;

      gtk_widget_get_allocation (icon, &allocation);

      if (gdk_rectangle_intersect (&allocation, rect, NULL))
        icons = g_list_prepend (icons, icon);
    }

  g_list_free (children);

  return icons;
}

GfIcon *
gf_monitor_view_find_next_icon (GfMonitorView    *self,
                                GfIcon           *next_to,
                                GtkDirectionType  direction)
{
  if (next_to == NULL)
    return find_first_icon (self);

  return find_next_icon (self, next_to, direction);
}

void
gf_monitor_view_select_icons (GfMonitorView *self,
                              GdkRectangle  *rect)
{
  GList *children;
  GList *l;

  children = gtk_container_get_children (GTK_CONTAINER (self));

  for (l = children; l != NULL; l = l->next)
    {
      GtkWidget *icon;
      GtkAllocation allocation;

      icon = l->data;

      gtk_widget_get_allocation (icon, &allocation);

      if (gdk_rectangle_intersect (&allocation, rect, NULL))
        gf_icon_set_selected (GF_ICON (icon), TRUE);
    }

  g_list_free (children);
}
