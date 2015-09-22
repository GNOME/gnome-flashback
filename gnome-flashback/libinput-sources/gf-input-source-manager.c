/*
 * Copyright (C) 2015 Alberts Muktupāvels
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
#include <glib/gi18n.h>
#include <libcommon/gf-keybindings.h>

#include "gf-input-source-manager.h"
#include "gf-input-source-settings.h"
#include "gf-ibus-manager.h"
#include "gf-keyboard-manager.h"

#define DESKTOP_WM_KEYBINDINGS_SCHEMA "org.gnome.desktop.wm.keybindings"

#define KEY_SWITCH_INPUT_SOURCE "switch-input-source"
#define KEY_SWITCH_INPUT_SOURCE_BACKWARD "switch-input-source-backward"

#define INPUT_SOURCE_TYPE_XKB "xkb"
#define INPUT_SOURCE_TYPE_IBUS "ibus"

typedef struct _SourceInfo SourceInfo;
struct _SourceInfo
{
  gchar *type;

  gchar *id;

  gchar *display_name;
  gchar *short_name;
};

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
  gboolean               disable_ibus;
};

enum
{
  PROP_0,

  PROP_IBUS_MANAGER,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfInputSourceManager, gf_input_source_manager, G_TYPE_OBJECT)

static SourceInfo *
source_info_new (const gchar *type,
                 const gchar *id,
                 const gchar *display_name,
                 const gchar *short_name)
{
  SourceInfo *info;

  info = g_new0 (SourceInfo, 1);

  info->type = g_strdup (type);
  info->id = g_strdup (id);
  info->display_name = g_strdup (display_name);
  info->short_name = g_strdup (short_name);

  return info;
}

static void
source_info_free (gpointer data)
{
  SourceInfo *info;

  info = (SourceInfo *) data;

  g_free (info->type);
  g_free (info->id);
  g_free (info->display_name);
  g_free (info->short_name);

  g_free (info);
}

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

static gchar *
make_engine_short_name (IBusEngineDesc *engine_desc)
{
  const gchar *symbol;
  const gchar *language_code;
  gchar **codes;

  symbol = ibus_engine_desc_get_symbol (engine_desc);

  if (symbol != NULL && symbol[0] != '\0')
    return g_strdup (symbol);

  language_code = ibus_engine_desc_get_language (engine_desc);
  codes = g_strsplit (language_code, "_", 2);

  if (strlen (codes[0]) == 2 || strlen (codes[0]) == 3)
    {
      gchar *short_name;

      short_name = g_ascii_strdown (codes[0], -1);
      g_strfreev (codes);

      return short_name;
    }

  g_strfreev (codes);

  return g_strdup_printf ("\u2328");
}

static GList *
get_source_info_list (GfInputSourceManager *manager)
{
  GVariant *sources;
  GnomeXkbInfo *xkb_info;
  GList *list;
  gsize size;
  gsize i;

  sources = gf_input_source_settings_get_sources (manager->settings);
  xkb_info = gf_keyboard_manager_get_xkb_info (manager->keyboard_manager);

  list = NULL;
  size = g_variant_n_children (sources);

  for (i = 0; i < size; i++)
    {
      const gchar *type;
      const gchar *id;
      SourceInfo *info;

      g_variant_get_child (sources, i, "(&s&s)", &type, &id);
      info = NULL;

      if (g_strcmp0 (type, INPUT_SOURCE_TYPE_XKB) == 0)
        {
          gboolean exists;
          const gchar *display_name;
          const gchar *short_name;

          exists = gnome_xkb_info_get_layout_info (xkb_info, id, &display_name,
                                                   &short_name, NULL, NULL);

          if (exists)
            info = source_info_new (type, id, display_name, short_name);
        }
      else if (g_strcmp0 (type, INPUT_SOURCE_TYPE_IBUS) == 0)
        {
          IBusEngineDesc *engine_desc;
          const gchar *language_code;
          const gchar *language;
          const gchar *longname;
          const gchar *textdomain;
          gchar *display_name;
          gchar *short_name;

          if (manager->disable_ibus)
            continue;

          engine_desc = gf_ibus_manager_get_engine_desc (manager->ibus_manager,
                                                         id);

          if (engine_desc == NULL)
            continue;

          language_code = ibus_engine_desc_get_language (engine_desc);
          language = ibus_get_language_name (language_code);
          longname = ibus_engine_desc_get_longname (engine_desc);
          textdomain = ibus_engine_desc_get_textdomain (engine_desc);

          if (*textdomain != '\0' && *longname != '\0')
            longname = g_dgettext (textdomain, longname);

          display_name = g_strdup_printf ("%s (%s)", language, longname);
          short_name = make_engine_short_name (engine_desc);

          info = source_info_new (type, id, display_name, short_name);

          g_free (display_name);
          g_free (short_name);
        }

      if (info != NULL)
        list = g_list_append (list, info);
    }

  g_variant_unref (sources);

  if (list == NULL)
    {
      const gchar *type;
      const gchar *id;
      const gchar *display_name;
      const gchar *short_name;
      SourceInfo *info;

      type = INPUT_SOURCE_TYPE_XKB;
      id = gf_keyboard_manager_get_default_layout (manager->keyboard_manager);

      gnome_xkb_info_get_layout_info (xkb_info, id, &display_name,
                                      &short_name, NULL, NULL);

      info = source_info_new (type, id, display_name, short_name);
      list = g_list_append (list, info);
    }

  g_object_unref (xkb_info);

  return list;
}

static void
sources_changed_cb (GfInputSourceSettings *settings,
                    gpointer               user_data)
{
  GfInputSourceManager *manager;
  GList *source_infos;
  GList *l;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);
  source_infos = get_source_info_list (manager);

  for (l = source_infos; l != NULL; l = g_list_next (l))
    {
    }

  g_list_free_full (source_infos, source_info_free);
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
