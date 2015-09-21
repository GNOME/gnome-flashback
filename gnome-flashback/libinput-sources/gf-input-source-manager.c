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

#include <gio/gio.h>
#include <libcommon/gf-keybindings.h>

#include "gf-input-source-manager.h"
#include "gf-input-source-settings.h"
#include "gf-ibus-manager.h"
#include "gf-keyboard-manager.h"

#define DESKTOP_WM_KEYBINDINGS_SCHEMA "org.gnome.desktop.wm.keybindings"

#define KEY_SWITCH_INPUT_SOURCE "switch-input-source"
#define KEY_SWITCH_INPUT_SOURCE_BACKWARD "switch-input-source-backward"

struct _GfInputSourceManager
{
  GObject                parent;

  GSettings             *wm_keybindings;
  GfKeybindings         *keybindings;
  guint                  switch_source_action;
  guint                  switch_source_backward_action;

  GfInputSourceSettings *settings;

  GfKeyboardManager     *keyboard_manager;

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
switch_input_changed_cb (GSettings *settings,
                         gchar     *key,
                         gpointer   user_data)
{
  GfInputSourceManager *manager;
  guint action;
  gchar **keybindings;
  gint i;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);
  action = manager->switch_source_action;

  if (action != 0)
    {
      gf_keybindings_ungrab (manager->keybindings, action);
      action = 0;
    }

  keybindings = g_settings_get_strv (settings, KEY_SWITCH_INPUT_SOURCE);

  /* There might be multiple keybindings set, but we will grab only one. */
  for (i = 0; keybindings[i] != NULL; i++)
    {
      action = gf_keybindings_grab (manager->keybindings, keybindings[i]);

      if (action != 0)
        break;
    }

  g_free (keybindings);

  manager->switch_source_action = action;
}

static void
switch_input_backward_changed_cb (GSettings *settings,
                                  gchar     *key,
                                  gpointer   user_data)
{
  GfInputSourceManager *manager;
  guint action;
  gchar **keybindings;
  gint i;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);
  action = manager->switch_source_backward_action;

  if (action != 0)
    {
      gf_keybindings_ungrab (manager->keybindings, action);
      action = 0;
    }

  keybindings = g_settings_get_strv (settings, KEY_SWITCH_INPUT_SOURCE);

  /* There might be multiple keybindings set, but we will grab only one. */
  for (i = 0; keybindings[i] != NULL; i++)
    {
      action = gf_keybindings_grab (manager->keybindings, keybindings[i]);

      if (action != 0)
        break;
    }

  g_free (keybindings);

  manager->switch_source_backward_action = action;
}

static void
accelerator_activated_cb (GfKeybindings *keybindings,
                          guint          action,
                          GVariant      *parameters,
                          gpointer       user_data)
{
}

static void
keybindings_init (GfInputSourceManager *manager)
{
  manager->wm_keybindings = g_settings_new (DESKTOP_WM_KEYBINDINGS_SCHEMA);
  manager->keybindings = gf_keybindings_new ();

  g_signal_connect (manager->wm_keybindings,
                    "changed::" KEY_SWITCH_INPUT_SOURCE,
                    G_CALLBACK (switch_input_changed_cb),
                    manager);

  g_signal_connect (manager->wm_keybindings,
                    "changed::" KEY_SWITCH_INPUT_SOURCE_BACKWARD,
                    G_CALLBACK (switch_input_backward_changed_cb),
                    manager);

  g_signal_connect (manager->keybindings, "accelerator-activated",
                    G_CALLBACK (accelerator_activated_cb), manager);

  switch_input_changed_cb (manager->wm_keybindings, NULL, manager);
  switch_input_backward_changed_cb (manager->wm_keybindings, NULL, manager);
}

static void
sources_changed_cb (GfInputSourceSettings *settings,
                    gpointer               user_data)
{
}

static void
xkb_options_changed_cb (GfInputSourceSettings *settings,
                        gpointer               user_data)
{
  GfInputSourceManager *manager;
  gchar **options;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);

  options = gf_input_source_settings_get_xkb_options (manager->settings);

  gf_keyboard_manager_set_xkb_options (manager->keyboard_manager, options);
  g_strfreev (options);

  gf_keyboard_manager_reapply (manager->keyboard_manager);
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

  g_clear_object (&manager->wm_keybindings);
  g_clear_object (&manager->keybindings);

  g_clear_object (&manager->settings);

  g_clear_object (&manager->keyboard_manager);

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
  manager->keyboard_manager = gf_keyboard_manager_new ();

  keybindings_init (manager);
  input_source_settings_init (manager);
}

GfInputSourceManager *
gf_input_source_manager_new (GfIBusManager *ibus_manager)
{
  return g_object_new (GF_TYPE_INPUT_SOURCE_MANAGER,
                       "ibus-manager", ibus_manager,
                       NULL);
}

void
gf_input_source_manager_reload (GfInputSourceManager *manager)
{
  gchar **options;

  options = gf_input_source_settings_get_xkb_options (manager->settings);

  gf_keyboard_manager_set_xkb_options (manager->keyboard_manager, options);
  g_strfreev (options);

  sources_changed_cb (manager->settings, manager);
}
