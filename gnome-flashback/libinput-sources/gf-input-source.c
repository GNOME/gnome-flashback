/*
 * Copyright (C) 2015 Sebastian Geiger
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
#include "gf-input-source.h"

#include <string.h>

typedef struct
{
  gchar         *type;
  gchar         *id;
  gchar         *display_name;
  gchar         *short_name;
  guint          index;

  char          *icon_file;
} GfInputSourcePrivate;

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

  PROP_TYPE,
  PROP_ID,
  PROP_DISPLAY_NAME,
  PROP_SHORT_NAME,
  PROP_INDEX,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GfInputSource, gf_input_source, G_TYPE_OBJECT)

static void
gf_input_source_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GfInputSource *source;
  GfInputSourcePrivate *priv;

  source = GF_INPUT_SOURCE (object);
  priv = gf_input_source_get_instance_private (source);

  switch (prop_id)
    {
      case PROP_TYPE:
        g_value_set_string (value, priv->type);
        break;

      case PROP_ID:
        g_value_set_string (value, priv->id);
        break;

      case PROP_DISPLAY_NAME:
        g_value_set_string (value, priv->display_name);
        break;

      case PROP_SHORT_NAME:
        g_value_set_string (value, priv->short_name);
        break;

      case PROP_INDEX:
        g_value_set_uint (value, priv->index);
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
  GfInputSourcePrivate *priv;

  source = GF_INPUT_SOURCE (object);
  priv = gf_input_source_get_instance_private (source);

  switch (prop_id)
    {
      case PROP_TYPE:
        priv->type = g_value_dup_string (value);
        break;

      case PROP_ID:
        priv->id = g_value_dup_string (value);
        break;

      case PROP_DISPLAY_NAME:
        priv->display_name = g_value_dup_string (value);
        break;

      case PROP_SHORT_NAME:
        priv->short_name = g_value_dup_string (value);
        break;

      case PROP_INDEX:
        priv->index = g_value_get_uint (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gf_input_source_finalize (GObject *object)
{
  GfInputSource *source;
  GfInputSourcePrivate *priv;

  source = GF_INPUT_SOURCE (object);
  priv = gf_input_source_get_instance_private (source);

  g_free (priv->type);
  g_free (priv->id);
  g_free (priv->display_name);
  g_free (priv->short_name);
  g_free (priv->icon_file);

  G_OBJECT_CLASS (gf_input_source_parent_class)->finalize (object);
}

static void
gf_input_source_class_init (GfInputSourceClass *source_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (source_class);

  object_class->finalize = gf_input_source_finalize;
  object_class->get_property = gf_input_source_get_property;
  object_class->set_property = gf_input_source_set_property;

  signals[SIGNAL_ACTIVATE] =
    g_signal_new ("activate", G_OBJECT_CLASS_TYPE (source_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                  G_TYPE_BOOLEAN);

  signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", G_OBJECT_CLASS_TYPE (source_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

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

const gchar *
gf_input_source_get_source_type (GfInputSource *source)
{
  GfInputSourcePrivate *priv;

  priv = gf_input_source_get_instance_private (source);

  return priv->type;
}

const gchar *
gf_input_source_get_id (GfInputSource *source)
{
  GfInputSourcePrivate *priv;

  priv = gf_input_source_get_instance_private (source);

  return priv->id;
}

const gchar *
gf_input_source_get_display_name (GfInputSource *source)
{
  GfInputSourcePrivate *priv;

  priv = gf_input_source_get_instance_private (source);

  return priv->display_name;
}

const gchar *
gf_input_source_get_short_name (GfInputSource *source)
{
  GfInputSourcePrivate *priv;

  priv = gf_input_source_get_instance_private (source);

  return priv->short_name;
}

void
gf_input_source_set_short_name  (GfInputSource *source,
                                 const gchar   *short_name)
{
  GfInputSourcePrivate *priv;

  priv = gf_input_source_get_instance_private (source);

  if (g_strcmp0 (priv->short_name, short_name) == 0)
    return;

  g_free (priv->short_name);
  priv->short_name = g_strdup (short_name);

  g_signal_emit (source, signals[SIGNAL_CHANGED], 0);
}

guint
gf_input_source_get_index (GfInputSource *source)
{
  GfInputSourcePrivate *priv;

  priv = gf_input_source_get_instance_private (source);

  return priv->index;
}

const gchar *
gf_input_source_get_xkb_id (GfInputSource *source)
{
  GfInputSourceClass *input_source_class;
  GfInputSourcePrivate *priv;

  input_source_class = GF_INPUT_SOURCE_GET_CLASS (source);

  if (input_source_class->get_xkb_id != NULL)
    return input_source_class->get_xkb_id (source);

  priv = gf_input_source_get_instance_private (source);

  return priv->id;
}

void
gf_input_source_activate (GfInputSource *source,
                          gboolean       interactive)
{
  g_signal_emit (source, signals[SIGNAL_ACTIVATE], 0, interactive);
}

const char *
gf_input_source_get_icon_file (GfInputSource *self)
{
  GfInputSourcePrivate *priv;

  priv = gf_input_source_get_instance_private (self);

  return priv->icon_file;
}

void
gf_input_source_set_icon_file (GfInputSource *self,
                               const char    *icon_file)
{
  GfInputSourcePrivate *priv;

  priv = gf_input_source_get_instance_private (self);

  g_clear_pointer (&priv->icon_file, g_free);
  priv->icon_file = g_strdup (icon_file);
}
