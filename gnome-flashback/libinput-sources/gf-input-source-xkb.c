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
#include "gf-input-source-xkb.h"

#include <libgnome-desktop/gnome-xkb-info.h>

struct _GfInputSourceXkb
{
  GfInputSource  parent;

  GnomeXkbInfo  *xkb_info;
};

enum
{
  PROP_0,

  PROP_XKB_INFO,

  LAST_PROP
};

static GParamSpec *xkb_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfInputSourceXkb, gf_input_source_xkb, GF_TYPE_INPUT_SOURCE)

static void
gf_input_source_xkb_dispose (GObject *object)
{
  GfInputSourceXkb *self;

  self = GF_INPUT_SOURCE_XKB (object);

  g_clear_object (&self->xkb_info);

  G_OBJECT_CLASS (gf_input_source_xkb_parent_class)->dispose (object);
}

static void
gf_input_source_xkb_set_property (GObject      *object,
                                  unsigned int  prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GfInputSourceXkb *self;

  self = GF_INPUT_SOURCE_XKB (object);

  switch (prop_id)
    {
      case PROP_XKB_INFO:
        self->xkb_info = g_value_dup_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  xkb_properties[PROP_XKB_INFO] =
    g_param_spec_object ("xkb-info",
                         "xkb-info",
                         "xkb-info",
                         GNOME_TYPE_XKB_INFO,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, xkb_properties);
}

static void
gf_input_source_xkb_class_init (GfInputSourceXkbClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_input_source_xkb_dispose;
  object_class->set_property = gf_input_source_xkb_set_property;

  install_properties (object_class);
}

static void
gf_input_source_xkb_init (GfInputSourceXkb *self)
{
}
