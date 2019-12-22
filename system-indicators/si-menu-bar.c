/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2004 Vincent Untz
 * Copyright (C) 2018-2019 Alberts Muktupāvels
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
 *
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     Mark McLoughlin <mark@skynet.ie>
 *     Vincent Untz <vincent@vuntz.net>
 */

#include "config.h"
#include "si-menu-bar.h"

#include <libgnome-panel/gp-utils.h>

struct _SiMenuBar
{
  GtkMenuBar      parent;

  double          label_angle;
  double          label_xalign;
  double          label_yalign;

  gboolean        enable_tooltips;
  GtkPositionType position;
};

enum
{
  PROP_0,

  PROP_ENABLE_TOOLTIPS,
  PROP_POSITION,

  LAST_PROP
};

static GParamSpec *menu_bar_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (SiMenuBar, si_menu_bar, GTK_TYPE_MENU_BAR)

static void
update_label (SiMenuBar *self,
              GtkWidget *widget)
{
  GtkWidget *child;
  GtkLabel *label;
  PangoLayout *layout;
  PangoContext *context;

  if (!GTK_IS_MENU_ITEM (widget))
    return;

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (!GTK_IS_LABEL (child))
    return;

  label = GTK_LABEL (child);
  layout = gtk_label_get_layout (label);
  context = pango_layout_get_context (layout);

  gtk_label_set_angle (label, self->label_angle);
  gtk_label_set_xalign (label, self->label_xalign);
  gtk_label_set_yalign (label, self->label_yalign);

  pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);
}

static void
update_label_cb (GtkWidget *widget,
                 gpointer   user_data)
{
  SiMenuBar *self;

  self = SI_MENU_BAR (user_data);

  update_label (self, widget);
}

static void
set_position (SiMenuBar       *self,
              GtkPositionType  position)
{
  GtkPackDirection direction;

  if (self->position == position)
    return;

  self->position = position;

  switch (position)
    {
      case GTK_POS_LEFT:
        direction = GTK_PACK_DIRECTION_BTT;

        self->label_angle = 90.0;
        self->label_xalign = 0.5;
        self->label_yalign = 0.0;
        break;

      case GTK_POS_RIGHT:
        direction = GTK_PACK_DIRECTION_TTB;

        self->label_angle = 270.0;
        self->label_xalign = 0.5;
        self->label_yalign = 0.0;
        break;

      case GTK_POS_TOP:
      case GTK_POS_BOTTOM:
      default:
        direction = GTK_PACK_DIRECTION_LTR;

        self->label_angle = 0.0;
        self->label_xalign = 0.0;
        self->label_yalign = 0.5;
        break;
    }

  gtk_menu_bar_set_pack_direction (GTK_MENU_BAR (self), direction);
  gtk_menu_bar_set_child_pack_direction (GTK_MENU_BAR (self), direction);

  gtk_container_foreach (GTK_CONTAINER (self), update_label_cb, self);
}

static void
si_menu_bar_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  SiMenuBar *self;

  self = SI_MENU_BAR (object);

  switch (property_id)
    {
      case PROP_ENABLE_TOOLTIPS:
        g_value_set_boolean (value, self->enable_tooltips);
        break;

      case PROP_POSITION:
        g_value_set_enum (value, self->position);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
si_menu_bar_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  SiMenuBar *self;

  self = SI_MENU_BAR (object);

  switch (property_id)
    {
      case PROP_ENABLE_TOOLTIPS:
        self->enable_tooltips = g_value_get_boolean (value);
        break;

      case PROP_POSITION:
        set_position (self, g_value_get_enum (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static gboolean
si_menu_bar_draw (GtkWidget *widget,
                  cairo_t   *cr)
{
  gboolean ret;
  GtkStyleContext *context;
  double width;
  double height;

  ret = GTK_WIDGET_CLASS (si_menu_bar_parent_class)->draw (widget, cr);

  if (!gtk_widget_has_focus (widget))
    return ret;

  context = gtk_widget_get_style_context (widget);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_focus (context, cr, 0, 0, width, height);

  return ret;
}

static void
si_menu_bar_remove (GtkContainer *container,
                    GtkWidget    *widget)
{
  gpointer signal_id;

  g_object_set_data (G_OBJECT (widget), "binding", NULL);

  signal_id = g_object_steal_data (G_OBJECT (widget), "signal-id");
  g_signal_handler_disconnect (widget, GPOINTER_TO_UINT (signal_id));

  GTK_CONTAINER_CLASS (si_menu_bar_parent_class)->remove (container, widget);
}

static void
set_has_tooltip_cb (GtkWidget *widget,
                    gpointer   user_data)
{
  gtk_widget_set_has_tooltip (widget, TRUE);
}

static void
si_menu_bar_deactivate (GtkMenuShell *menu_shell)
{
  SiMenuBar *self;

  self = SI_MENU_BAR (menu_shell);

  GTK_MENU_SHELL_CLASS (si_menu_bar_parent_class)->deactivate (menu_shell);

   if (!self->enable_tooltips)
    return;

  gtk_container_foreach (GTK_CONTAINER (menu_shell), set_has_tooltip_cb, NULL);
}

static void
activate_cb (GtkMenuItem *menu_item,
             gpointer     user_data)
{
  GtkWidget *toplevel;

  gtk_widget_set_has_tooltip (GTK_WIDGET (menu_item), FALSE);

  /* Remove focus that would be drawn on the currently focused
   * child of the toplevel. See bug #308632.
   */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (menu_item));
  if (gtk_widget_is_toplevel (toplevel))
    gtk_window_set_focus (GTK_WINDOW (toplevel), NULL);
}

static void
si_menu_bar_insert (GtkMenuShell *menu_shell,
                    GtkWidget    *child,
                    gint          position)
{
  SiMenuBar *self;
  GBinding *binding;
  gulong signal_id;

  self = SI_MENU_BAR (menu_shell);

  GTK_MENU_SHELL_CLASS (si_menu_bar_parent_class)->insert (menu_shell,
                                                           child,
                                                           position);

  binding = g_object_bind_property (menu_shell,
                                    "enable-tooltips",
                                    child,
                                    "has-tooltip",
                                    G_BINDING_DEFAULT |
                                    G_BINDING_SYNC_CREATE);

  g_object_set_data_full (G_OBJECT (child),
                          "binding",
                          binding,
                          (GDestroyNotify) g_binding_unbind);

  signal_id = g_signal_connect (child,
                                "activate",
                                G_CALLBACK (activate_cb),
                                NULL);

  g_object_set_data (G_OBJECT (child),
                     "signal-id",
                     GUINT_TO_POINTER (signal_id));

  gp_add_text_color_class (child);
  update_label (self, child);
}

static void
install_properties (GObjectClass *object_class)
{
  menu_bar_properties[PROP_ENABLE_TOOLTIPS] =
    g_param_spec_boolean ("enable-tooltips",
                          "Enable Tooltips",
                          "Enable Tooltips",
                          TRUE,
                          G_PARAM_CONSTRUCT |
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS);

  menu_bar_properties[PROP_POSITION] =
    g_param_spec_enum ("position",
                       "Position",
                       "Position",
                       GTK_TYPE_POSITION_TYPE,
                       GTK_POS_TOP,
                       G_PARAM_CONSTRUCT |
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     LAST_PROP,
                                     menu_bar_properties);
}

static void
si_menu_bar_class_init (SiMenuBarClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkMenuShellClass *menu_shell_class;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);
  container_class = GTK_CONTAINER_CLASS (self_class);
  menu_shell_class = GTK_MENU_SHELL_CLASS (self_class);

  object_class->get_property = si_menu_bar_get_property;
  object_class->set_property = si_menu_bar_set_property;

  widget_class->draw = si_menu_bar_draw;

  container_class->remove = si_menu_bar_remove;

  menu_shell_class->deactivate = si_menu_bar_deactivate;
  menu_shell_class->insert = si_menu_bar_insert;

  install_properties (object_class);
}

static void
si_menu_bar_init (SiMenuBar *self)
{
  self->label_yalign = 0.5;
}

GtkWidget *
si_menu_bar_new (void)
{
  return g_object_new (SI_TYPE_MENU_BAR,
                       "can-focus", TRUE,
                       NULL);
}
