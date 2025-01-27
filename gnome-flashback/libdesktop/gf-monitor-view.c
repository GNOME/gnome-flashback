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
#include "gf-utils.h"

struct _GfMonitorView
{
  GtkFixed     parent;

  GdkMonitor  *monitor;

  gboolean     grid_points;

  GfIconView  *icon_view;

  GfDummyIcon *dummy_icon;
  gulong       size_changed_id;

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

  gboolean     drop_pending;
};

enum
{
  PROP_0,

  PROP_MONITOR,

  PROP_GRID_POINTS,

  PROP_ICON_VIEW,

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
add_drag_rectangles_from_icon_list (GfMonitorView *self,
                                    GfIcon        *drag_icon,
                                    GPtrArray     *rectangles)
{
  GList *selected_icons;
  GList *l;

  selected_icons = gf_icon_view_get_selected_icons (self->icon_view);
  if (selected_icons == NULL)
    return;

  for (l = selected_icons; l != NULL; l = l->next)
    {
      GfIcon *icon;
      GtkWidget *image;
      double press_x;
      double press_y;
      GdkRectangle *rectangle;

      icon = l->data;

      image = gf_icon_get_image (icon);
      gf_icon_get_press (drag_icon, &press_x, &press_y);

      rectangle = g_new0 (GdkRectangle, 1);
      g_ptr_array_add (rectangles, rectangle);

      gtk_widget_get_allocation (image, rectangle);
      gtk_widget_translate_coordinates (image,
                                        GTK_WIDGET (drag_icon),
                                        -press_x,
                                        -press_y,
                                        &rectangle->x,
                                        &rectangle->y);
    }
}

static void
add_drag_rectangles_from_gnome_icon_list (GfMonitorView *self,
                                          const guchar  *gnome_icon_list,
                                          GPtrArray     *rectangles)
{
  char **list;
  double scale;
  int i;

  list = g_strsplit ((const char *) gnome_icon_list, "\r\n", -1);
  if (list == NULL)
    return;

  scale = 1.0 / gf_get_nautilus_scale ();

  for (i = 0; list[i] != NULL; i++)
    {
      int x;
      int y;
      unsigned short int width;
      unsigned short int height;
      GdkRectangle *rectangle;

      if (sscanf (list[i], "%*s\r%d:%d:%hu:%hu", &x, &y, &width, &height) != 4)
        continue;

      rectangle = g_new0 (GdkRectangle, 1);
      g_ptr_array_add (rectangles, rectangle);

      rectangle->x = x / scale;
      rectangle->y = y / scale;
      rectangle->width = width / scale;
      rectangle->height = height / scale;
    }

  g_strfreev (list);
}

static char **
get_uris_from_icon_list (GfMonitorView *self,
                         GfIcon        *drag_icon)
{
  GPtrArray *uris;
  GList *selected_icons;
  GList *l;

  uris = g_ptr_array_new ();
  selected_icons = gf_icon_view_get_selected_icons (self->icon_view);

  for (l = selected_icons; l != NULL; l = l->next)
    {
      GFile *file;

      file = gf_icon_get_file (GF_ICON (l->data));
      g_ptr_array_add (uris, g_file_get_uri (file));
    }

  g_ptr_array_add (uris, NULL);

  return (char **) g_ptr_array_free (uris, FALSE);
}

static char **
get_uris_from_gnome_icon_list (GfMonitorView *self,
                               const guchar  *gnome_icon_list)
{
  GPtrArray *uris;
  char **list;

  uris = g_ptr_array_new ();
  list = g_strsplit ((const char *) gnome_icon_list, "\r\n", -1);

  if (list != NULL)
    {
      int i;

      for (i = 0; list[i] != NULL; i++)
        {
          char **parts;

          parts = g_strsplit (list[i], "\r", -1);
          if (parts == NULL)
            continue;

          g_ptr_array_add (uris, g_strdup (parts[0]));
          g_strfreev (parts);
        }

      g_strfreev (list);
    }

  g_ptr_array_add (uris, NULL);

  return (char **) g_ptr_array_free (uris, FALSE);
}

static void
copy_to_desktop (GfMonitorView  *self,
                 char          **uris,
                 guint           time)
{
  char *desktop_uri;

  desktop_uri = gf_icon_view_get_desktop_uri (self->icon_view);

  gf_icon_view_copy_uris (self->icon_view,
                          (const char * const *) uris,
                          desktop_uri,
                          time);

  g_free (desktop_uri);
}

static void
move_to_desktop (GfMonitorView  *self,
                 char          **uris,
                 guint           time)
{
  char *desktop_uri;

  desktop_uri = gf_icon_view_get_desktop_uri (self->icon_view);

  gf_icon_view_move_uris (self->icon_view,
                          (const char * const *) uris,
                          desktop_uri,
                          time);

  g_free (desktop_uri);
}

static void
drag_data_received_cb (GtkWidget        *widget,
                       GdkDragContext   *context,
                       gint              x,
                       gint              y,
                       GtkSelectionData *data,
                       guint             info,
                       guint             time,
                       GfMonitorView    *self)
{

  if (data == NULL || gtk_selection_data_get_length (data) == 0)
    {
      gtk_drag_finish (context, FALSE, FALSE, time);
      return;
    }

  if (self->drop_pending)
    {
      GdkWindow *window;
      GdkDevice *device;
      GdkModifierType mask;
      GdkDragAction action;
      GPtrArray *drag_rectangles;

      window = gdk_drag_context_get_dest_window (context);
      device = gdk_drag_context_get_device (context);

      gdk_window_get_device_position (window, device, NULL, NULL, &mask);

      action = 0;
      drag_rectangles = g_ptr_array_new_with_free_func (g_free);

      if (info == 100)
        {
          GfIcon *icon;

          icon = *(gpointer *) gtk_selection_data_get_data (data);
          add_drag_rectangles_from_icon_list (self, icon, drag_rectangles);

          if (mask & GDK_CONTROL_MASK)
            action = GDK_ACTION_COPY;
          else
            action = GDK_ACTION_MOVE;
        }
      else if (info == 200)
        {
          const guchar *selection_data;

          selection_data = gtk_selection_data_get_data (data);
          add_drag_rectangles_from_gnome_icon_list (self,
                                                    selection_data,
                                                    drag_rectangles);

          if (mask & GDK_CONTROL_MASK)
            action = GDK_ACTION_COPY;
          else
            action = GDK_ACTION_MOVE;
        }
      else if (info == 300)
        {
          action = GDK_ACTION_COPY;
        }

      gdk_drag_status (context, action, time);

      gf_icon_view_set_drag_rectangles (self->icon_view, drag_rectangles);
      g_ptr_array_unref (drag_rectangles);
    }
  else
    {
      gboolean success;
      gboolean delete;
      GdkDragAction action;

      success = FALSE;
      delete = FALSE;

      action = gdk_drag_context_get_selected_action (context);

      if (info == 100)
        {
          GfIcon *icon;

          icon = *(gpointer *) gtk_selection_data_get_data (data);

          if (action == GDK_ACTION_MOVE &&
              self->placement != GF_PLACEMENT_AUTO_ARRANGE_ICONS)
            {
            }
          else if (action == GDK_ACTION_COPY)
            {
              if (self->placement == GF_PLACEMENT_AUTO_ARRANGE_ICONS)
                {
                  char **uris;

                  uris = get_uris_from_icon_list (self, icon);

                  copy_to_desktop (self, uris, time);
                  g_strfreev (uris);

                  success = TRUE;
                }
            }
        }
      else if (info == 200)
        {
          const guchar *selection_data;

          selection_data = gtk_selection_data_get_data (data);

          if (action == GDK_ACTION_MOVE)
            {
              if (self->placement == GF_PLACEMENT_AUTO_ARRANGE_ICONS)
                {
                  char **uris;

                  uris = get_uris_from_gnome_icon_list (self, selection_data);

                  move_to_desktop (self, uris, time);
                  g_strfreev (uris);

                  success = TRUE;
                  delete = TRUE;
                }
            }
          else if (action == GDK_ACTION_COPY)
            {
              if (self->placement == GF_PLACEMENT_AUTO_ARRANGE_ICONS)
                {
                  char **uris;

                  uris = get_uris_from_gnome_icon_list (self, selection_data);

                  copy_to_desktop (self, uris, time);
                  g_strfreev (uris);

                  success = TRUE;
                }
            }
        }
      else if (info == 300)
        {
          if (action == GDK_ACTION_COPY)
            {
              if (self->placement == GF_PLACEMENT_AUTO_ARRANGE_ICONS)
                {
                  char **uris;

                  uris = gtk_selection_data_get_uris (data);

                  copy_to_desktop (self, uris, time);
                  g_strfreev (uris);

                  success = TRUE;
                }
            }
        }

      gtk_drag_finish (context, success, delete, time);
    }
}

static gboolean
drag_drop_cb (GtkWidget      *widget,
              GdkDragContext *context,
              gint            x,
              gint            y,
              guint           time,
              GfMonitorView  *self)
{
  GtkTargetList *target_list;
  GList *list_targets;
  GList *l;

  target_list = gtk_drag_dest_get_target_list (widget);
  list_targets = gdk_drag_context_list_targets (context);

  for (l = list_targets; l != NULL; l = l->next)
    {
      GdkAtom atom;

      atom = GDK_POINTER_TO_ATOM (l->data);

      if (gtk_target_list_find (target_list, atom, NULL))
        {
          self->drop_pending = FALSE;
          gtk_drag_get_data (widget, context, atom, time);

          return TRUE;
        }
    }

  gtk_drag_finish (context, FALSE, FALSE, time);

  return TRUE;
}

static void
drag_leave_cb (GtkWidget      *widget,
               GdkDragContext *context,
               guint           time,
               GfMonitorView  *self)
{
  gf_icon_view_set_drag_rectangles (self->icon_view, NULL);
}

static gboolean
drag_motion_cb (GtkWidget      *widget,
                GdkDragContext *context,
                gint            x,
                gint            y,
                guint           time,
                GfMonitorView  *self)
{
  GdkAtom target;

  target = gtk_drag_dest_find_target (widget, context, NULL);

  if (target == GDK_NONE ||
      gdk_drag_context_get_suggested_action (context) == 0)
    {
      gdk_drag_status (context, 0, time);
      return TRUE;
    }

  self->drop_pending = TRUE;
  gtk_drag_get_data (widget, context, target, time);

  return TRUE;
}

static void
setup_drop_destination (GfMonitorView *self)
{
  GdkDragAction actions;
  GtkDestDefaults defaults;
  GtkTargetList *target_list;
  GdkAtom target;

  actions = GDK_ACTION_COPY | GDK_ACTION_MOVE;
  defaults = 0;

  gtk_drag_dest_set (GTK_WIDGET (self), 0, NULL, defaults, actions);

  target_list = gtk_target_list_new (NULL, 0);

  target = gdk_atom_intern_static_string ("x-gnome-flashback/icon-list");
  gtk_target_list_add (target_list, target, GTK_TARGET_SAME_APP, 100);

  target = gdk_atom_intern_static_string ("x-special/gnome-icon-list");
  gtk_target_list_add (target_list, target, 0, 200);

  target = gdk_atom_intern_static_string ("text/uri-list");
  gtk_target_list_add (target_list, target, 0, 300);

  gtk_drag_dest_set_target_list (GTK_WIDGET (self), target_list);
  gtk_target_list_unref (target_list);

  g_signal_connect (self, "drag-data-received",
                    G_CALLBACK (drag_data_received_cb), self);

  g_signal_connect (self, "drag-drop",
                    G_CALLBACK (drag_drop_cb), self);

  g_signal_connect (self, "drag-leave",
                    G_CALLBACK (drag_leave_cb), self);

  g_signal_connect (self, "drag-motion",
                    G_CALLBACK (drag_motion_cb), self);
}

static void
get_padding (GtkWidget *widget,
             GtkBorder *border)
{
  GtkStyleContext *style;
  GtkStateFlags state;
  style = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (style);

  gtk_style_context_get_padding (style, state, border);
}

static void
calculate_grid_size (GfMonitorView *self)
{
  GtkBorder padding;
  int view_width;
  int view_height;
  int icon_width;
  int icon_height;
  int columns;
  int rows;
  int spacing_x;
  int spacing_y;
  int offset_x;
  int offset_y;
  gboolean changed;

  get_padding (GTK_WIDGET (self), &padding);

  view_width = self->view_width - padding.left - padding.right;
  view_height = self->view_height - padding.top - padding.bottom;

  icon_width = gf_dummy_icon_get_width (self->dummy_icon);
  icon_height = gf_dummy_icon_get_height (self->dummy_icon);

  columns = view_width / icon_width;
  rows = view_height / icon_height;

  while (TRUE)
    {
      int spacing;

      spacing = (columns - 1) * self->column_spacing;
      if (spacing + columns * icon_width <= view_width ||
          columns == 1)
        break;

      columns--;
    }

  while (TRUE)
    {
      int spacing;

      spacing = (rows - 1) * self->row_spacing;
      if (spacing + rows * icon_height <= view_height ||
          rows == 1)
        break;

      rows--;
    }

  spacing_x = icon_width + self->column_spacing;
  spacing_y = icon_height + self->row_spacing;

  offset_x = (view_width - columns * icon_width -
              (columns - 1) * self->column_spacing) / 2;
  offset_y = (view_height - rows * icon_height -
              (rows - 1) * self->row_spacing) / 2;

  offset_x += padding.left;
  offset_y += padding.top;

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

  self->size_changed_id = g_signal_connect (self->dummy_icon,
                                            "size-changed",
                                            G_CALLBACK (dummy_icon_size_changed_cb),
                                            self);
}

static void
gf_monitor_view_dispose (GObject *object)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (object);

  if (self->size_changed_id != 0)
    {
      g_signal_handler_disconnect (self->dummy_icon, self->size_changed_id);
      self->size_changed_id = 0;
    }

  if (self->grid_size_id != 0)
    {
      g_source_remove (self->grid_size_id);
      self->grid_size_id = 0;
    }

  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (gf_monitor_view_parent_class)->dispose (object);
}

static void
gf_monitor_view_finalize (GObject *object)
{
  GfMonitorView *self;

  self = GF_MONITOR_VIEW (object);

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

      case PROP_ICON_VIEW:
        g_assert (self->icon_view == NULL);
        self->icon_view = g_value_get_object (value);
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

  view_properties[PROP_ICON_VIEW] =
    g_param_spec_object ("icon-view",
                         "icon-view",
                         "icon-view",
                         GF_TYPE_ICON_VIEW,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
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

  gtk_widget_class_set_css_name (widget_class, "gf-monitor-view");
}

static void
gf_monitor_view_init (GfMonitorView *self)
{
  self->grid = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
                                      (GDestroyNotify) g_ptr_array_unref);

  setup_drop_destination (self);
}

GtkWidget *
gf_monitor_view_new (GdkMonitor  *monitor,
                     GfIconView  *icon_view,
                     GfDummyIcon *dummy_icon,
                     guint        column_spacing,
                     guint        row_spacing)
{
  return g_object_new (GF_TYPE_MONITOR_VIEW,
                       "monitor", monitor,
                       "icon-view", icon_view,
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
