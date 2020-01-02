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

struct _GfInputSourceIBus
{
  GfInputSource  parent;

  GfIBusManager *ibus_manager;

  IBusPropList  *prop_list;

  char          *display_name;
  char          *short_name;

  char          *icon;

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

static const char *
get_layout (IBusEngineDesc *engine_desc)
{
  const char *layout;

  layout = ibus_engine_desc_get_layout (engine_desc);

  if (g_strcmp0 (layout, "default") == 0)
    return "us";

  return layout;
}

static gchar *
get_symbol_from_char_code (gunichar code)
{
  gchar buffer[6];
  gint length;

  length = g_unichar_to_utf8 (code, buffer);
  buffer[length] = '\0';

  return g_strdup_printf ("%s", buffer);
}

static gchar *
make_engine_short_name (IBusEngineDesc *engine_desc)
{
  const gchar *symbol;
  const gchar *language;

  symbol = ibus_engine_desc_get_symbol (engine_desc);

  if (symbol != NULL && symbol[0] != '\0')
    return g_strdup (symbol);

  language = ibus_engine_desc_get_language (engine_desc);

  if (language != NULL && language[0] != '\0')
    {
      gchar **codes;

      codes = g_strsplit (language, "_", 2);

      if (strlen (codes[0]) == 2 || strlen (codes[0]) == 3)
        {
          gchar *short_name;

          short_name = g_ascii_strdown (codes[0], -1);
          g_strfreev (codes);

          return short_name;
        }

      g_strfreev (codes);
    }

  return get_symbol_from_char_code (0x2328);
}

static void
gf_input_source_ibus_constructed (GObject *object)
{
  GfInputSourceIBus *self;
  const char *id;
  IBusEngineDesc *engine_desc;
  const char *language_code;
  const char *language;
  const char *longname;
  const char *textdomain;
  const char *layout_variant;
  const char *layout;

  self = GF_INPUT_SOURCE_IBUS (object);

  G_OBJECT_CLASS (gf_input_source_ibus_parent_class)->constructed (object);

  id = gf_input_source_get_id (GF_INPUT_SOURCE (self));

  engine_desc = gf_ibus_manager_get_engine_desc (self->ibus_manager, id);
  g_assert (engine_desc != NULL);

  language_code = ibus_engine_desc_get_language (engine_desc);
  language = ibus_get_language_name (language_code);
  longname = ibus_engine_desc_get_longname (engine_desc);
  textdomain = ibus_engine_desc_get_textdomain (engine_desc);

  if (*textdomain != '\0' && *longname != '\0')
    longname = g_dgettext (textdomain, longname);

  self->display_name = g_strdup_printf ("%s (%s)", language, longname);
  self->short_name = make_engine_short_name (engine_desc);

  self->icon = g_strdup (ibus_engine_desc_get_icon (engine_desc));

  layout = get_layout (engine_desc);
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

  g_clear_pointer (&self->display_name, g_free);
  g_clear_pointer (&self->short_name, g_free);
  g_clear_pointer (&self->icon, g_free);
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
gf_input_source_ibus_get_display_name (GfInputSource *input_source)
{
  GfInputSourceIBus *self;

  self = GF_INPUT_SOURCE_IBUS (input_source);

  return self->display_name;
}

static const char *
gf_input_source_ibus_get_short_name (GfInputSource *input_source)
{
  GfInputSourceIBus *self;

  self = GF_INPUT_SOURCE_IBUS (input_source);

  return self->short_name;
}

static gboolean
gf_input_source_ibus_set_short_name (GfInputSource *input_source,
                                     const char    *short_name)
{
  GfInputSourceIBus *self;

  self = GF_INPUT_SOURCE_IBUS (input_source);

  if (g_strcmp0 (self->short_name, short_name) == 0)
    return FALSE;

  g_clear_pointer (&self->short_name, g_free);
  self->short_name = g_strdup (short_name);

  return TRUE;
}

static gboolean
gf_input_source_ibus_get_layout (GfInputSource  *input_source,
                                 const char    **layout,
                                 const char    **variant)
{
  GfInputSourceIBus *self;
  const char *id;
  IBusEngineDesc *engine_desc;

  self = GF_INPUT_SOURCE_IBUS (input_source);

  id = gf_input_source_get_id (input_source);
  engine_desc = gf_ibus_manager_get_engine_desc (self->ibus_manager, id);

  if (engine_desc == NULL)
    return FALSE;

  *layout = get_layout (engine_desc);
  *variant = ibus_engine_desc_get_layout_variant (engine_desc);

  return TRUE;
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

  input_source_class->get_display_name = gf_input_source_ibus_get_display_name;
  input_source_class->get_short_name = gf_input_source_ibus_get_short_name;
  input_source_class->set_short_name = gf_input_source_ibus_set_short_name;
  input_source_class->get_layout = gf_input_source_ibus_get_layout;
  input_source_class->get_xkb_id = gf_input_source_ibus_get_xkb_id;

  install_properties (object_class);
}

static void
gf_input_source_ibus_init (GfInputSourceIBus *self)
{
}

GfInputSource *
gf_input_source_ibus_new (GfIBusManager *ibus_manager,
                          const char    *id,
                          int            index)
{
  return g_object_new (GF_TYPE_INPUT_SOURCE_IBUS,
                       "ibus-manager", ibus_manager,
                       "type", INPUT_SOURCE_TYPE_IBUS,
                       "id", id,
                       "index", index,
                       NULL);
}

const char *
gf_input_source_ibus_get_icon (GfInputSourceIBus *self)
{
  return self->icon;
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
