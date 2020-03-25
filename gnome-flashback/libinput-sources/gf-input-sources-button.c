/*
 * Copyright (C) 2020 Alberts Muktupāvels
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
#include "gf-input-sources-button.h"

struct _GfInputSourcesButton
{
  GtkButton             parent;

  GfInputSourceManager *manager;
};

enum
{
  PROP_0,

  PROP_MANAGER,

  LAST_PROP
};

static GParamSpec *button_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfInputSourcesButton, gf_input_sources_button, GTK_TYPE_BUTTON)

static void
update_button (GfInputSourcesButton *self)
{
  GfInputSource *source;
  const char *short_name;
  const char *display_name;

  source = gf_input_source_manager_get_current_source (self->manager);
  if (source == NULL)
    return;

  short_name = gf_input_source_get_short_name (source);
  gtk_button_set_label (GTK_BUTTON (self), short_name);

  display_name = gf_input_source_get_display_name (source);
  gtk_widget_set_tooltip_text (GTK_WIDGET (self), display_name);
}

static void
current_source_changed_cb (GfInputSourceManager *manager,
                           GfInputSource        *old_source,
                           GfInputSourcesButton *self)
{
  update_button (self);
}

static void
sources_changed_cb (GfInputSourceManager *manager,
                    GfInputSourcesButton *self)
{
  GList *input_sources;

  input_sources = gf_input_source_manager_get_input_sources (manager);

  if (g_list_length (input_sources) <= 1)
    {
      gtk_widget_hide (GTK_WIDGET (self));
      return;
    }

  gtk_widget_show (GTK_WIDGET (self));
  update_button (self);
}

static void
gf_input_sources_button_constructed (GObject *object)
{
  GfInputSourcesButton *self;

  self = GF_INPUT_SOURCES_BUTTON (object);

  G_OBJECT_CLASS (gf_input_sources_button_parent_class)->constructed (object);

  g_signal_connect_object (self->manager, "sources-changed",
                           G_CALLBACK (sources_changed_cb),
                           self, 0);

  g_signal_connect_object (self->manager, "current-source-changed",
                           G_CALLBACK (current_source_changed_cb),
                           self, 0);

  sources_changed_cb (self->manager, self);
}

static void
gf_input_sources_button_dispose (GObject *object)
{
  GfInputSourcesButton *self;

  self = GF_INPUT_SOURCES_BUTTON (object);

  g_clear_object (&self->manager);

  G_OBJECT_CLASS (gf_input_sources_button_parent_class)->dispose (object);
}

static void
gf_input_sources_button_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GfInputSourcesButton *self;

  self = GF_INPUT_SOURCES_BUTTON (object);

  switch (property_id)
    {
      case PROP_MANAGER:
        g_assert (self->manager == NULL);
        self->manager = g_value_dup_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_input_sources_button_show (GtkWidget *widget)
{
  GfInputSourcesButton *self;
  GList *input_sources;

  self = GF_INPUT_SOURCES_BUTTON (widget);
  input_sources = gf_input_source_manager_get_input_sources (self->manager);

  if (g_list_length (input_sources) <= 1)
    return;

  GTK_WIDGET_CLASS (gf_input_sources_button_parent_class)->show (widget);
}

static void
gf_input_sources_button_clicked (GtkButton *button)
{
  GfInputSourcesButton *self;

  self = GF_INPUT_SOURCES_BUTTON (button);

  gf_input_source_manager_activate_next_source (self->manager);
}

static void
install_properties (GObjectClass *object_class)
{
  button_properties[PROP_MANAGER] =
    g_param_spec_object ("manager",
                         "manager",
                         "manager",
                         GF_TYPE_INPUT_SOURCE_MANAGER,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, button_properties);
}

static void
gf_input_sources_button_class_init (GfInputSourcesButtonClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkButtonClass *button_class;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);
  button_class = GTK_BUTTON_CLASS (self_class);

  object_class->constructed = gf_input_sources_button_constructed;
  object_class->dispose = gf_input_sources_button_dispose;
  object_class->set_property = gf_input_sources_button_set_property;

  widget_class->show = gf_input_sources_button_show;

  button_class->clicked = gf_input_sources_button_clicked;

  install_properties (object_class);
}

static void
gf_input_sources_button_init (GfInputSourcesButton *self)
{
}

GtkWidget *
gf_input_sources_button_new (GfInputSourceManager *manager)
{
  return g_object_new (GF_TYPE_INPUT_SOURCES_BUTTON,
                       "focus-on-click", FALSE,
                       "manager", manager,
                       NULL);
}
