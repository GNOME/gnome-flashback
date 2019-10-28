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
#include "gf-background.h"

struct _GfBackground
{
  GObject    parent;

  GtkWidget *window;
};

enum
{
  PROP_0,

  PROP_WINDOW,

  LAST_PROP
};

static GParamSpec *background_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfBackground, gf_background, G_TYPE_OBJECT)

static void
gf_background_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GfBackground *self;

  self = GF_BACKGROUND (object);

  switch (property_id)
    {
      case PROP_WINDOW:
        g_assert (self->window == NULL);
        self->window = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  background_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "window",
                         "window",
                         GTK_TYPE_WIDGET,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     background_properties);
}

static void
gf_background_class_init (GfBackgroundClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->set_property = gf_background_set_property;

  install_properties (object_class);
}

static void
gf_background_init (GfBackground *self)
{
}

GfBackground *
gf_background_new (GtkWidget *window)
{
  return g_object_new (GF_TYPE_BACKGROUND,
                       "window", window,
                       NULL);
}
