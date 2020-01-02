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
#include "gf-input-source-ibus.h"

#include "gf-ibus-manager.h"

struct _GfInputSourceIBus
{
  GfInputSource  parent;

  GfIBusManager *ibus_manager;

  IBusPropList  *prop_list;

  char          *xkb_id;
};

enum
{
  PROP_0,

  PROP_IBUS_MANAGER,

  LAST_PROP
};

static GParamSpec *ibus_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfInputSourceIBus, gf_input_source_ibus, GF_TYPE_INPUT_SOURCE)

static void
gf_input_source_ibus_constructed (GObject *object)
{
  GfInputSourceIBus *self;
  const char *id;
  IBusEngineDesc *engine_desc;
  const char *layout_variant;
  const char *layout;

  self = GF_INPUT_SOURCE_IBUS (object);

  G_OBJECT_CLASS (gf_input_source_ibus_parent_class)->constructed (object);

  id = gf_input_source_get_id (GF_INPUT_SOURCE (self));

  engine_desc = gf_ibus_manager_get_engine_desc (self->ibus_manager, id);
  g_assert (engine_desc != NULL);

  layout = ibus_engine_desc_get_layout (engine_desc);
  layout_variant = ibus_engine_desc_get_layout_variant (engine_desc);

  if (layout_variant != NULL && *layout_variant != '\0')
    self->xkb_id = g_strdup_printf ("%s+%s", layout, layout_variant);
  else
    self->xkb_id = g_strdup (layout);
}

static void
gf_input_source_ibus_dispose (GObject *object)
{
  GfInputSourceIBus *self;

  self = GF_INPUT_SOURCE_IBUS (object);

  g_clear_object (&self->ibus_manager);
  g_clear_object (&self->prop_list);

  G_OBJECT_CLASS (gf_input_source_ibus_parent_class)->dispose (object);
}

static void
gf_input_source_ibus_finalize (GObject *object)
{
  GfInputSourceIBus *self;

  self = GF_INPUT_SOURCE_IBUS (object);

  g_clear_pointer (&self->xkb_id, g_free);

  G_OBJECT_CLASS (gf_input_source_ibus_parent_class)->finalize (object);
}

static void
gf_input_source_ibus_set_property (GObject      *object,
                                   unsigned int  prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GfInputSourceIBus *self;

  self = GF_INPUT_SOURCE_IBUS (object);

  switch (prop_id)
    {
      case PROP_IBUS_MANAGER:
        self->ibus_manager = g_value_dup_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static const char *
gf_input_source_ibus_get_xkb_id (GfInputSource *input_source)
{
  GfInputSourceIBus *self;

  self = GF_INPUT_SOURCE_IBUS (input_source);

  return self->xkb_id;
}

static void
install_properties (GObjectClass *object_class)
{
  ibus_properties[PROP_IBUS_MANAGER] =
    g_param_spec_object ("ibus-manager",
                         "ibus-manager",
                         "ibus-manager",
                         GF_TYPE_IBUS_MANAGER,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, ibus_properties);
}

static void
gf_input_source_ibus_class_init (GfInputSourceIBusClass *self_class)
{
  GObjectClass *object_class;
  GfInputSourceClass *input_source_class;

  object_class = G_OBJECT_CLASS (self_class);
  input_source_class = GF_INPUT_SOURCE_CLASS (self_class);

  object_class->constructed = gf_input_source_ibus_constructed;
  object_class->dispose = gf_input_source_ibus_dispose;
  object_class->finalize = gf_input_source_ibus_finalize;
  object_class->set_property = gf_input_source_ibus_set_property;

  input_source_class->get_xkb_id = gf_input_source_ibus_get_xkb_id;

  install_properties (object_class);
}

static void
gf_input_source_ibus_init (GfInputSourceIBus *self)
{
}

IBusPropList *
gf_input_source_ibus_get_properties (GfInputSourceIBus *self)
{
  return self->prop_list;
}

void
gf_input_source_ibus_set_properties (GfInputSourceIBus *self,
                                     IBusPropList      *prop_list)
{
  g_clear_object (&self->prop_list);

  if (prop_list != NULL)
    self->prop_list = g_object_ref (prop_list);
}
