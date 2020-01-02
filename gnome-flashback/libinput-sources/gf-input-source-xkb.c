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

struct _GfInputSourceXkb
{
  GfInputSource  parent;

  GnomeXkbInfo  *xkb_info;

  char          *display_name;
  char          *short_name;
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
gf_input_source_xkb_constructed (GObject *object)
{
  GfInputSourceXkb *self;
  const char *id;
  const char *display_name;
  const char *short_name;

  self = GF_INPUT_SOURCE_XKB (object);

  G_OBJECT_CLASS (gf_input_source_xkb_parent_class)->constructed (object);

  id = gf_input_source_get_id (GF_INPUT_SOURCE (self));

  gnome_xkb_info_get_layout_info (self->xkb_info,
                                  id,
                                  &display_name,
                                  &short_name,
                                  NULL,
                                  NULL);

  self->display_name = g_strdup (display_name);
  self->short_name = g_strdup (short_name);
}

static void
gf_input_source_xkb_dispose (GObject *object)
{
  GfInputSourceXkb *self;

  self = GF_INPUT_SOURCE_XKB (object);

  g_clear_object (&self->xkb_info);

  G_OBJECT_CLASS (gf_input_source_xkb_parent_class)->dispose (object);
}

static void
gf_input_source_xkb_finalize (GObject *object)
{
  GfInputSourceXkb *self;

  self = GF_INPUT_SOURCE_XKB (object);

  g_clear_pointer (&self->display_name, g_free);
  g_clear_pointer (&self->short_name, g_free);

  G_OBJECT_CLASS (gf_input_source_xkb_parent_class)->finalize (object);
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

static const char *
gf_input_source_xkb_get_display_name (GfInputSource *input_source)
{
  GfInputSourceXkb *self;

  self = GF_INPUT_SOURCE_XKB (input_source);

  return self->display_name;
}

static const char *
gf_input_source_xkb_get_short_name (GfInputSource *input_source)
{
  GfInputSourceXkb *self;

  self = GF_INPUT_SOURCE_XKB (input_source);

  return self->short_name;
}

static gboolean
gf_input_source_xkb_set_short_name (GfInputSource *input_source,
                                    const char    *short_name)
{
  GfInputSourceXkb *self;

  self = GF_INPUT_SOURCE_XKB (input_source);

  if (g_strcmp0 (self->short_name, short_name) == 0)
    return FALSE;

  g_clear_pointer (&self->short_name, g_free);
  self->short_name = g_strdup (short_name);

  return TRUE;
}

static gboolean
gf_input_source_xkb_get_layout (GfInputSource  *input_source,
                                const char    **layout,
                                const char    **variant)
{
  GfInputSourceXkb *self;

  self = GF_INPUT_SOURCE_XKB (input_source);

  return gnome_xkb_info_get_layout_info (self->xkb_info,
                                         gf_input_source_get_id (input_source),
                                         NULL,
                                         NULL,
                                         layout,
                                         variant);
}

static const char *
gf_input_source_xkb_get_xkb_id (GfInputSource *input_source)
{
  return gf_input_source_get_id (input_source);
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
  GfInputSourceClass *input_source_class;

  object_class = G_OBJECT_CLASS (self_class);
  input_source_class = GF_INPUT_SOURCE_CLASS (self_class);

  object_class->constructed = gf_input_source_xkb_constructed;
  object_class->dispose = gf_input_source_xkb_dispose;
  object_class->finalize = gf_input_source_xkb_finalize;
  object_class->set_property = gf_input_source_xkb_set_property;

  input_source_class->get_display_name = gf_input_source_xkb_get_display_name;
  input_source_class->get_short_name = gf_input_source_xkb_get_short_name;
  input_source_class->set_short_name = gf_input_source_xkb_set_short_name;
  input_source_class->get_layout = gf_input_source_xkb_get_layout;
  input_source_class->get_xkb_id = gf_input_source_xkb_get_xkb_id;

  install_properties (object_class);
}

static void
gf_input_source_xkb_init (GfInputSourceXkb *self)
{
}

GfInputSource *
gf_input_source_xkb_new (GnomeXkbInfo *xkb_info,
                         const char   *id,
                         int           index)
{
  return g_object_new (GF_TYPE_INPUT_SOURCE_XKB,
                       "xkb-info", xkb_info,
                       "type", INPUT_SOURCE_TYPE_XKB,
                       "id", id,
                       "index", index,
                       NULL);
}
