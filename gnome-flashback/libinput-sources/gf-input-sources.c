/*
 * Copyright (C) 2015-2020 Alberts Muktupāvels
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <locale.h>
#include <utime.h>

#include "dbus/gf-input-sources-gen.h"
#include "gf-ibus-manager.h"
#include "gf-input-sources-button.h"
#include "gf-input-sources.h"
#include "gf-input-source-ibus.h"
#include "gf-input-source-manager.h"
#include "gf-input-source.h"

struct _GfInputSources
{
  GObject               parent;

  guint                 bus_name_id;
  GfInputSourcesGen    *input_sources;

  GfIBusManager        *ibus_manager;
  GfInputSourceManager *input_source_manager;

  GfInputSource        *current_source;
};

G_DEFINE_TYPE (GfInputSources, gf_input_sources, G_TYPE_OBJECT)

static void
current_source_changed_cb (GfInputSourceManager *manager,
                           GfInputSource        *old_source,
                           GfInputSources       *sources)
{
  GfInputSource *source;

  source = gf_input_source_manager_get_current_source (manager);

  g_clear_object (&sources->current_source);
  sources->current_source = g_object_ref (source);

  gf_input_sources_gen_emit_changed (sources->input_sources);
}

static void
append_icon_info (GVariantBuilder *builder,
                  GfInputSources  *self)
{
  const char *display_name;
  const char *icon_text;

  display_name = gf_input_source_get_display_name (self->current_source);
  icon_text = gf_input_source_get_short_name (self->current_source);

  if (GF_IS_INPUT_SOURCE_IBUS (self->current_source))
    {
      GfInputSourceIBus *ibus_source;
      const char *icon;
      IBusPropList *prop_list;

      ibus_source = GF_INPUT_SOURCE_IBUS (self->current_source);

      icon = gf_input_source_ibus_get_icon (ibus_source);
      prop_list = gf_input_source_ibus_get_properties (ibus_source);

      if (icon != NULL)
        {
          g_variant_builder_add (builder, "{sv}", "icon-file",
                                 g_variant_new_string (icon));
        }

      if (prop_list != NULL)
        {
          guint i;

          for (i = 0; i < prop_list->properties->len; i++)
            {
              IBusProperty *prop;
              const char *key;
              IBusText *symbol;
              IBusText *label;
              const char *tmp;

              prop = ibus_prop_list_get (prop_list, i);

              if (!ibus_property_get_visible (prop))
                continue;

              key = ibus_property_get_key (prop);
              if (g_strcmp0 (key, "InputMode") != 0)
                continue;

              symbol = ibus_property_get_symbol (prop);
              label = ibus_property_get_label (prop);

              if (symbol != NULL)
                tmp = ibus_text_get_text (symbol);
              else
                tmp = ibus_text_get_text (label);

              if (tmp != NULL && *tmp != '\0' && g_utf8_strlen (tmp, -1) < 3)
                icon_text = tmp;
            }
        }
    }

  g_variant_builder_add (builder, "{sv}", "tooltip",
                         g_variant_new_string (display_name));

  g_variant_builder_add (builder, "{sv}", "icon-text",
                         g_variant_new_string (icon_text));
}

static const char *
prop_type_to_string (IBusPropType type)
{
  switch (type)
    {
      case PROP_TYPE_NORMAL:
        return "normal";

      case PROP_TYPE_TOGGLE:
        return "toggle";

      case PROP_TYPE_RADIO:
        return "radio";

      case PROP_TYPE_MENU:
        return "menu";

      case PROP_TYPE_SEPARATOR:
        return "separator";

      default:
        break;
    }

  return "unknown";
}

static const char *
prop_state_to_string (IBusPropState state)
{
  switch (state)
    {
      case PROP_STATE_UNCHECKED:
        return "unchecked";

      case PROP_STATE_CHECKED:
        return "checked";

      case PROP_STATE_INCONSISTENT:
        return "inconsistent";

      default:
        break;
    }

  return "unknown";
}

static GVariant *
prop_list_to_variant (IBusPropList *prop_list)
{
  GVariantBuilder builder;
  guint i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sa{sv})"));

  if (prop_list == NULL)
    return g_variant_builder_end (&builder);

  for (i = 0; i < prop_list->properties->len; i++)
    {
      IBusProperty *prop;
      GVariantBuilder prop_builder;
      IBusText *label;
      IBusText *tooltip;
      IBusPropType type;
      IBusPropState state;
      IBusPropList *sub_props;
      const char *string;

      prop = ibus_prop_list_get (prop_list, i);

      if (!ibus_property_get_visible (prop))
        continue;

      g_variant_builder_init (&prop_builder, G_VARIANT_TYPE ("a{sv}"));

      label = ibus_property_get_label (prop);
      tooltip = ibus_property_get_tooltip (prop);
      type = ibus_property_get_prop_type (prop);
      state = ibus_property_get_state (prop);
      sub_props = ibus_property_get_sub_props (prop);

      string = ibus_property_get_icon (prop);
      g_variant_builder_add (&prop_builder, "{sv}", "icon",
                             g_variant_new_string (string));

      string = ibus_text_get_text (label);
      g_variant_builder_add (&prop_builder, "{sv}", "label",
                             g_variant_new_string (string));

      string = ibus_text_get_text (tooltip);
      g_variant_builder_add (&prop_builder, "{sv}", "tooltip",
                             g_variant_new_string (string));

      string = prop_type_to_string (type);
      g_variant_builder_add (&prop_builder, "{sv}", "type",
                             g_variant_new_string (string));

      string = prop_state_to_string (state);
      g_variant_builder_add (&prop_builder, "{sv}", "state",
                             g_variant_new_string (string));

      if (type == PROP_TYPE_MENU)
        {
          g_variant_builder_add (&prop_builder, "{sv}", "menu",
                                 prop_list_to_variant (sub_props));
        }

      g_variant_builder_add (&builder,
                             "(sa{sv})",
                             ibus_property_get_key (prop),
                             &prop_builder);
    }

  return g_variant_builder_end (&builder);
}

static void
append_properties (GVariantBuilder *builder,
                   GfInputSources  *self)
{
  IBusPropList *prop_list;
  GVariant *properties;

  if (GF_IS_INPUT_SOURCE_IBUS (self->current_source))
    {
      GfInputSourceIBus *ibus_source;

      ibus_source = GF_INPUT_SOURCE_IBUS (self->current_source);
      prop_list = gf_input_source_ibus_get_properties (ibus_source);
    }
  else
    {
      prop_list = NULL;
    }

  properties = prop_list_to_variant (prop_list);

  g_variant_builder_add (builder, "{sv}", "properties", properties);
}

static void
append_layout_info (GVariantBuilder *builder,
                    GfInputSources  *self)
{
  const char *layout;
  const char *layout_variant;
  GVariant *variant;

  layout = NULL;
  layout_variant = NULL;

  if (!gf_input_source_get_layout (self->current_source,
                                   &layout,
                                   &layout_variant))
    return;

  if (layout != NULL)
    {
      variant = g_variant_new_string (layout);
      g_variant_builder_add (builder, "{sv}", "layout", variant);
    }

  if (layout_variant != NULL)
    {
      variant = g_variant_new_string (layout_variant);
      g_variant_builder_add (builder, "{sv}", "layout-variant", variant);
    }
}

static GVariant *
get_input_sources (GfInputSources *self)
{
  GVariantBuilder builder;
  GList *input_sources;
  GList *l;

  g_variant_builder_init (&builder,
                          G_VARIANT_TYPE ("a(ussb)"));

  input_sources = gf_input_source_manager_get_input_sources (self->input_source_manager);

  for (l = input_sources; l != NULL; l = l->next)
    {
      GfInputSource *input_source;

      input_source = GF_INPUT_SOURCE (l->data);

      g_variant_builder_add (&builder,
                             "(ussb)",
                             gf_input_source_get_index (input_source),
                             gf_input_source_get_short_name (input_source),
                             gf_input_source_get_display_name (input_source),
                             input_source == self->current_source);
    }

  g_list_free (input_sources);

  return g_variant_builder_end (&builder);
}

static GVariant *
get_current_source (GfInputSources *self)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder,
                          G_VARIANT_TYPE ("a{sv}"));

  append_icon_info (&builder, self);
  append_layout_info (&builder, self);
  append_properties (&builder, self);

  return g_variant_builder_end (&builder);
}

static IBusProperty *
find_ibus_property (IBusPropList *prop_list,
                    const char   *find_key)
{
  guint i;

  if (prop_list == NULL)
    return NULL;

  for (i = 0; i < prop_list->properties->len; i++)
    {
      IBusProperty *prop;
      const char *key;
      IBusPropList *sub_props;

      prop = ibus_prop_list_get (prop_list, i);
      key = ibus_property_get_key (prop);

      if (g_strcmp0 (key, find_key) == 0)
        return prop;

      sub_props = ibus_property_get_sub_props (prop);
      prop = find_ibus_property (sub_props, find_key);

      if (prop != NULL)
        return prop;
    }

  return NULL;
}

static void
activate_ibus_property (GfInputSources *self,
                        const char     *key,
                        IBusProperty   *property)
{
  IBusPropType type;
  IBusPropState state;

  type = ibus_property_get_prop_type (property);
  state = ibus_property_get_state (property);

  switch (type)
    {
      case PROP_TYPE_TOGGLE:
      case PROP_TYPE_RADIO:
        if (state == PROP_STATE_CHECKED)
          state = PROP_STATE_UNCHECKED;
        else
          state = PROP_STATE_CHECKED;

        ibus_property_set_state (property, state);
        break;

      case PROP_TYPE_MENU:
      case PROP_TYPE_SEPARATOR:
        g_assert_not_reached ();
        break;

      case PROP_TYPE_NORMAL:
      default:
        break;
    }

  gf_ibus_manager_activate_property (self->ibus_manager, key, state);
}

static gboolean
handle_get_input_sources_cb (GfInputSourcesGen     *object,
                             GDBusMethodInvocation *invocation,
                             GfInputSources        *self)
{
  GVariant *input_sources;
  GVariant *current_source;

  input_sources = get_input_sources (self);
  current_source = get_current_source (self);

  gf_input_sources_gen_complete_get_input_sources (object,
                                                   invocation,
                                                   input_sources,
                                                   current_source);

  return TRUE;
}

static gboolean
handle_activate_cb (GfInputSourcesGen     *object,
                    GDBusMethodInvocation *invocation,
                    guint                  index,
                    GfInputSources        *self)
{
  GfInputSource *input_source;
  GList *input_sources;
  GList *l;

  input_source = NULL;
  input_sources = gf_input_source_manager_get_input_sources (self->input_source_manager);

  for (l = input_sources; l != NULL; l = g_list_next (l))
    {
      input_source = GF_INPUT_SOURCE (l->data);

      if (gf_input_source_get_index (input_source) == index)
        break;

      input_source = NULL;
    }

  g_list_free (input_sources);

  if (input_source == NULL)
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Input source with index %d does not exist",
                                             index);

      return TRUE;
    }

  gf_input_source_activate (input_source, TRUE);
  gf_input_sources_gen_complete_activate (object, invocation);

  return TRUE;
}

static gboolean
handle_activate_property_cb (GfInputSourcesGen     *object,
                             GDBusMethodInvocation *invocation,
                             const char            *key,
                             GfInputSources        *self)
{
  GfInputSourceIBus *ibus_source;
  IBusPropList *prop_list;
  IBusProperty *prop;

  if (!GF_IS_INPUT_SOURCE_IBUS (self->current_source))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Input source does not have properties");

      return TRUE;
    }

  ibus_source = GF_INPUT_SOURCE_IBUS (self->current_source);
  prop_list = gf_input_source_ibus_get_properties (ibus_source);

  if (prop_list == NULL)
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Input source does not have properties");

      return TRUE;
    }

  prop = find_ibus_property (prop_list, key);

  if (prop == NULL)
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Input source does not have %s property",
                                             key);

      return TRUE;
    }

  activate_ibus_property (self, key, prop);
  gf_input_sources_gen_complete_activate_property (object, invocation);

  return TRUE;
}

static void
bus_acquired_cb (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  GfInputSources *sources;
  GDBusInterfaceSkeleton *skeleton;
  GError *error;
  gboolean exported;

  sources = GF_INPUT_SOURCES (user_data);
  skeleton = G_DBUS_INTERFACE_SKELETON (sources->input_sources);

  g_signal_connect (skeleton, "handle-get-input-sources",
                    G_CALLBACK (handle_get_input_sources_cb),
                    sources);

  g_signal_connect (skeleton, "handle-activate",
                    G_CALLBACK (handle_activate_cb),
                    sources);

  g_signal_connect (skeleton, "handle-activate-property",
                    G_CALLBACK (handle_activate_property_cb),
                    sources);

  error = NULL;
  exported = g_dbus_interface_skeleton_export (skeleton,
                                               connection,
                                               "/org/gnome/Flashback/InputSources",
                                               &error);

  if (!exported)
    {
      g_warning ("Failed to export interface: %s", error->message);
      g_error_free (error);

      return;
    }
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
name_lost_cb (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
}

static void
gf_input_sources_dispose (GObject *object)
{
  GfInputSources *sources;

  sources = GF_INPUT_SOURCES (object);

  if (sources->bus_name_id != 0)
    {
      g_bus_unown_name (sources->bus_name_id);
      sources->bus_name_id = 0;
    }

  if (sources->input_sources)
    {
      GDBusInterfaceSkeleton *skeleton;

      skeleton = G_DBUS_INTERFACE_SKELETON (sources->input_sources);

      g_dbus_interface_skeleton_unexport (skeleton);
      g_clear_object (&sources->input_sources);
    }

  g_clear_object (&sources->ibus_manager);
  g_clear_object (&sources->input_source_manager);

  g_clear_object (&sources->current_source);

  G_OBJECT_CLASS (gf_input_sources_parent_class)->dispose (object);
}

static void
gf_input_sources_class_init (GfInputSourcesClass *sources_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (sources_class);

  object_class->dispose = gf_input_sources_dispose;
}

static void
gf_input_sources_init (GfInputSources *sources)
{
  sources->input_sources = gf_input_sources_gen_skeleton_new ();

  sources->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                         "org.gnome.Flashback.InputSources",
                                         G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                         G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                         bus_acquired_cb,
                                         name_acquired_cb,
                                         name_lost_cb,
                                         sources,
                                         NULL);

  sources->ibus_manager = gf_ibus_manager_new ();
  sources->input_source_manager = gf_input_source_manager_new (sources->ibus_manager);

  g_signal_connect (sources->input_source_manager, "current-source-changed",
                    G_CALLBACK (current_source_changed_cb), sources);

  gf_input_source_manager_reload (sources->input_source_manager);
}

GfInputSources *
gf_input_sources_new (void)
{
  return g_object_new (GF_TYPE_INPUT_SOURCES, NULL);
}

GtkWidget *
gf_input_sources_create_button (GfInputSources *self)
{
  return gf_input_sources_button_new (self->input_source_manager);
}
