/*
 * Copyright (C) 2015 Sebastian Geiger
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

#include <string.h>

#include "gf-ibus-manager.h"
#include "gf-input-source.h"

struct _GfInputSource
{
  GObject        parent;

  GfIBusManager *ibus_manager;

  gchar         *type;
  gchar         *id;
  gchar         *display_name;
  gchar         *short_name;
  guint          index;

  gchar         *xkb_id;

  IBusPropList  *prop_list;
};

enum
{
  SIGNAL_CHANGED,
  SIGNAL_ACTIVATE,

  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

enum
{
  PROP_0,

  PROP_IBUS_MANAGER,

  PROP_TYPE,
  PROP_ID,
  PROP_DISPLAY_NAME,
  PROP_SHORT_NAME,
  PROP_INDEX,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfInputSource, gf_input_source, G_TYPE_OBJECT)

static gchar *
get_xkb_id (GfInputSource *source)
{
  IBusEngineDesc *engine_desc;
  const gchar *layout_variant;
  const gchar *layout;

  engine_desc = gf_ibus_manager_get_engine_desc (source->ibus_manager,
                                                 source->id);

  if (!engine_desc)
    return g_strdup (source->id);

  layout = ibus_engine_desc_get_layout (engine_desc);
  layout_variant = ibus_engine_desc_get_layout_variant (engine_desc);

  if (layout_variant && strlen (layout_variant) > 0)
    return g_strdup_printf ("%s+%s", layout, layout_variant);
  else
    return g_strdup (layout);
}

static void
gf_input_source_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GfInputSource *source;

  source = GF_INPUT_SOURCE (object);

  switch (prop_id)
    {
      case PROP_IBUS_MANAGER:
        g_value_set_object (value, source->ibus_manager);
        break;

      case PROP_TYPE:
        g_value_set_string (value, source->type);
        break;

      case PROP_ID:
        g_value_set_string (value, source->id);
        break;

      case PROP_DISPLAY_NAME:
        g_value_set_string (value, source->display_name);
        break;

      case PROP_SHORT_NAME:
        g_value_set_string (value, source->short_name);
        break;

      case PROP_INDEX:
        g_value_set_uint (value, source->index);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gf_input_source_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GfInputSource *source;

  source = GF_INPUT_SOURCE (object);

  switch (prop_id)
    {
      case PROP_IBUS_MANAGER:
        source->ibus_manager = g_value_get_object (value);
        break;

      case PROP_TYPE:
        source->type = g_value_dup_string (value);
        break;

      case PROP_ID:
        source->id = g_value_dup_string (value);
        break;

      case PROP_DISPLAY_NAME:
        source->display_name = g_value_dup_string (value);
        break;

      case PROP_SHORT_NAME:
        source->short_name = g_value_dup_string (value);
        break;

      case PROP_INDEX:
        source->index = g_value_get_uint (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gf_input_source_dispose (GObject *object)
{
  GfInputSource *source;

  source = GF_INPUT_SOURCE (object);

  g_clear_object (&source->prop_list);

  G_OBJECT_CLASS (gf_input_source_parent_class)->dispose (object);
}

static void
gf_input_source_finalize (GObject *object)
{
  GfInputSource *source;

  source = GF_INPUT_SOURCE (object);

  g_free (source->type);
  g_free (source->id);
  g_free (source->display_name);
  g_free (source->short_name);
  g_free (source->xkb_id);

  G_OBJECT_CLASS (gf_input_source_parent_class)->finalize (object);
}

static void
gf_input_source_constructed (GObject *object)
{
  GfInputSource *source;

  source = GF_INPUT_SOURCE (object);

  G_OBJECT_CLASS (gf_input_source_parent_class)->constructed (object);

  source->xkb_id = get_xkb_id (source);
}

static void
gf_input_source_class_init (GfInputSourceClass *source_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (source_class);

  object_class->constructed = gf_input_source_constructed;
  object_class->dispose = gf_input_source_dispose;
  object_class->finalize = gf_input_source_finalize;
  object_class->get_property = gf_input_source_get_property;
  object_class->set_property = gf_input_source_set_property;

  signals[SIGNAL_ACTIVATE] =
    g_signal_new ("activate", G_OBJECT_CLASS_TYPE (source_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", G_OBJECT_CLASS_TYPE (source_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  properties[PROP_IBUS_MANAGER] =
    g_param_spec_object ("ibus-manager", "IBus Manager",
                         "The instance of IBus Manager used by the input-sources module",
                         GF_TYPE_IBUS_MANAGER,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_TYPE] =
    g_param_spec_string ("type", "type", "The type of the input source",
                         NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_ID] =
    g_param_spec_string ("id", "ID", "The ID of the input source",
                         NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name", "Display name",
                         "The display name of the input source",
                         NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_SHORT_NAME] =
    g_param_spec_string ("short-name", "Short name",
                         "The short name of the input source",
                         NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_INDEX] =
    g_param_spec_uint ("index", "Index", "The index of the input source",
                       0, G_MAXUINT, 0,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gf_input_source_init (GfInputSource *source)
{
}

GfInputSource *
gf_input_source_new (GfIBusManager *ibus_manager,
                     const gchar   *type,
                     const gchar   *id,
                     const gchar   *display_name,
                     const gchar   *short_name,
                     guint          index)
{
  return g_object_new (GF_TYPE_INPUT_SOURCE,
                       "ibus-manager", ibus_manager,
                       "type", type,
                       "id", id,
                       "display-name", display_name,
                       "short-name", short_name,
                       "index", index,
                       NULL);
}

const gchar *
gf_input_source_get_source_type (GfInputSource *source)
{
  return source->type;
}

const gchar *
gf_input_source_get_id (GfInputSource *source)
{
  return source->id;
}

const gchar *
gf_input_source_get_display_name (GfInputSource *source)
{
  return source->display_name;
}

const gchar *
gf_input_source_get_short_name (GfInputSource *source)
{
  return source->short_name;
}

void
gf_input_source_set_short_name  (GfInputSource *source,
                                 const gchar   *short_name)
{
  if (g_strcmp0 (source->short_name, short_name) == 0)
    return;

  g_free (source->short_name);
  source->short_name = g_strdup (short_name);

  g_signal_emit (source, signals[SIGNAL_CHANGED], 0);
}

guint
gf_input_source_get_index (GfInputSource *source)
{
  return source->index;
}

const gchar *
gf_input_source_get_xkb_id (GfInputSource *source)
{
  return source->xkb_id;
}

void
gf_input_source_activate (GfInputSource *source)
{
  g_signal_emit (source, signals[SIGNAL_ACTIVATE], 0);
}

IBusPropList *
gf_input_source_get_properties (GfInputSource *source)
{
  return source->prop_list;
}

void
gf_input_source_set_properties (GfInputSource *source,
                                IBusPropList  *prop_list)
{
  g_clear_object (&source->prop_list);

  if (prop_list != NULL)
    source->prop_list = g_object_ref (prop_list);
}
