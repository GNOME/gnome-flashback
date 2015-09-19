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

#include "gf-input-source-manager.h"
#include "gf-input-source-settings.h"
#include "gf-ibus-manager.h"

struct _GfInputSourceManager
{
  GObject                parent;

  GfInputSourceSettings *settings;

  GfIBusManager         *ibus_manager;
};

G_DEFINE_TYPE (GfInputSourceManager, gf_input_source_manager, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_IBUS_MANAGER,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

static void
sources_changed_cb (GfInputSourceSettings *settings,
                    gpointer               user_data)
{
}

static void
xkb_options_changed_cb (GfInputSourceSettings *settings,
                        gpointer               user_data)
{
}

static void
per_window_changed_cb (GfInputSourceSettings *settings,
                       gpointer               user_data)
{
}

static void
input_source_settings_init (GfInputSourceManager *manager)
{
  manager->settings = gf_input_source_settings_new ();

  g_signal_connect (manager->settings, "sources-changed",
                    G_CALLBACK (sources_changed_cb), manager);
  g_signal_connect (manager->settings, "xkb-options-changed",
                    G_CALLBACK (xkb_options_changed_cb), manager);
  g_signal_connect (manager->settings, "per-window-changed",
                    G_CALLBACK (per_window_changed_cb), manager);
}

static void
gf_input_source_manager_dispose (GObject *object)
{
  GfInputSourceManager *manager;

  manager = GF_INPUT_SOURCE_MANAGER (object);

  g_clear_object (&manager->settings);

  G_OBJECT_CLASS (gf_input_source_manager_parent_class)->dispose (object);
}

static void
gf_input_source_manager_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GfInputSourceManager *manager;

  manager = GF_INPUT_SOURCE_MANAGER (object);

  switch (property_id)
    {
      case PROP_IBUS_MANAGER:
        manager->ibus_manager = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_input_source_manager_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GfInputSourceManager *manager;

  manager = GF_INPUT_SOURCE_MANAGER (object);

  switch (property_id)
    {
      case PROP_IBUS_MANAGER:
        g_value_set_object (value, manager->ibus_manager);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_input_source_manager_class_init (GfInputSourceManagerClass *manager_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (manager_class);

  object_class->dispose = gf_input_source_manager_dispose;
  object_class->get_property = gf_input_source_manager_get_property;
  object_class->set_property = gf_input_source_manager_set_property;

  properties[PROP_IBUS_MANAGER] =
    g_param_spec_object ("ibus-manager", "IBus Manager",
                         "An instance of GfIBusManager",
                         GF_TYPE_IBUS_MANAGER,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gf_input_source_manager_init (GfInputSourceManager *manager)
{
  input_source_settings_init (manager);
}

GfInputSourceManager *
gf_input_source_manager_new (GfIBusManager *ibus_manager)
{
  return g_object_new (GF_TYPE_INPUT_SOURCE_MANAGER,
                       "ibus-manager", ibus_manager,
                       NULL);
}
