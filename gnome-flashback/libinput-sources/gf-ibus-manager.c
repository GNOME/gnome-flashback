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

#include "gf-candidate-popup.h"
#include "gf-ibus-manager.h"

/*
 * This is the longest we'll keep the keyboard frozen until an input
 * source is active.
 */
#define MAX_INPUT_SOURCE_ACTIVATION_TIME 4000

struct _GfIBusManager
{
  GObject           parent;

  GfCandidatePopup *candidate_popup;
  IBusBus          *ibus;

  GSubprocess      *subprocess;

  GHashTable       *engines;

  IBusPanelService *panel_service;

  gboolean          ready;

  gchar            *current_engine_name;

  gulong            register_properties_id;
};

enum
{
  SIGNAL_READY,
  SIGNAL_PROPERTIES_REGISTERED,
  SIGNAL_PROPERTY_UPDATED,
  SIGNAL_SET_CONTENT_TYPE,

  SIGNAL_ENGINE_SET,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfIBusManager, gf_ibus_manager, G_TYPE_OBJECT)

static void
spawn (GfIBusManager *manager)
{
  GError *error;

  error = NULL;
  manager->subprocess = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                          &error, "ibus-daemon", "--xim",
                                          "--panel", "disable", NULL);

  if (error != NULL)
    {
      g_warning ("Failed to launch ibus-daemon: %s", error->message);
      g_error_free (error);
    }
}

static void
clear (GfIBusManager *manager)
{
  gf_candidate_popup_set_panel_service (manager->candidate_popup, NULL);
  g_clear_object (&manager->panel_service);

  if (manager->engines != NULL)
    {
      g_hash_table_destroy (manager->engines);
      manager->engines = NULL;
    }

  manager->ready = FALSE;

  g_free (manager->current_engine_name);
  manager->current_engine_name = NULL;

  manager->register_properties_id = 0;

  g_signal_emit (manager, signals[SIGNAL_READY], 0, FALSE);

  spawn (manager);
}

static void
update_readiness (GfIBusManager *manager)
{
  manager->ready = FALSE;

  if (g_hash_table_size (manager->engines) > 0 &&
      manager->panel_service != NULL)
    {
      manager->ready = TRUE;
    }

  g_signal_emit (manager, signals[SIGNAL_READY], 0, manager->ready);
}

static void
add_engine_to_hash_table (gpointer data,
                          gpointer user_data)
{
  IBusEngineDesc *engine_desc;
  GfIBusManager *manager;
  const gchar *name;

  engine_desc = (IBusEngineDesc *) data;
  manager = GF_IBUS_MANAGER (user_data);

  name = ibus_engine_desc_get_name (engine_desc);
  g_hash_table_insert (manager->engines, g_strdup (name),
                       g_object_ref (engine_desc));
}

static void
list_engines_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GfIBusManager *manager;
  GList *engines;
  GError *error;

  manager = GF_IBUS_MANAGER (user_data);

  error = NULL;
  engines = ibus_bus_list_engines_async_finish (manager->ibus, res, &error);

  if (engines != NULL)
    {
      if (manager->engines == NULL)
        manager->engines = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, g_object_unref);

      g_list_foreach (engines, add_engine_to_hash_table, manager);
      g_list_free (engines);

      update_readiness (manager);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to list ibus engines - %s", error->message);
      g_error_free (error);

      clear (manager);
    }
}

static void
update_property_cb (IBusPanelService *panel_service,
                    IBusProperty     *property,
                    gpointer          user_data)
{
  GfIBusManager *manager;

  manager = GF_IBUS_MANAGER (user_data);

  g_signal_emit (manager, signals[SIGNAL_PROPERTY_UPDATED], 0,
                 manager->current_engine_name, property);
}

static void
set_content_type_cb (IBusPanelService *panel_service,
                     guint             purpose,
                     guint             hints,
                     gpointer          user_data)
{
  GfIBusManager *manager;

  manager = GF_IBUS_MANAGER (user_data);

  g_signal_emit (manager, signals[SIGNAL_SET_CONTENT_TYPE], 0, purpose, hints);
}

static void
register_properties_cb (IBusPanelService *service,
                        IBusPropList     *prop_list,
                        gpointer          user_data)
{
  GfIBusManager *manager;

  manager = GF_IBUS_MANAGER (user_data);

  if (ibus_prop_list_get (prop_list, 0) == NULL)
    return;

  g_signal_handler_disconnect (service, manager->register_properties_id);
  manager->register_properties_id = 0;

  g_signal_emit (manager, signals[SIGNAL_PROPERTIES_REGISTERED], 0,
                 manager->current_engine_name, prop_list);
}

static void
global_engine_changed_cb (IBusBus     *ibus,
                          const gchar *name,
                          gpointer     user_data)
{
  GfIBusManager *manager;

  manager = GF_IBUS_MANAGER (user_data);

  if (!manager->ready)
    return;

  g_free (manager->current_engine_name);
  manager->current_engine_name = g_strdup (name);

  if (manager->register_properties_id != 0)
    return;

  manager->register_properties_id =
    g_signal_connect (manager->panel_service, "register-properties",
                      G_CALLBACK (register_properties_cb), manager);
}

static void
get_global_engine_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GfIBusManager *manager;
  IBusEngineDesc *engine_desc;
  GError *error;
  const gchar *name;

  manager = GF_IBUS_MANAGER (user_data);

  error = NULL;
  engine_desc = ibus_bus_get_global_engine_async_finish (manager->ibus, res,
                                                         &error);

  if (engine_desc == NULL)
    {
      g_error_free (error);

      return;
    }

  name = ibus_engine_desc_get_name (engine_desc);

  global_engine_changed_cb (manager->ibus, name, manager);
}

static void
request_name_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GfIBusManager *manager;
  guint result;
  GError *error;
  GDBusConnection *connection;

  manager = GF_IBUS_MANAGER (user_data);

  error = NULL;
  result = ibus_bus_request_name_async_finish (manager->ibus, res, &error);

  if (result == 0)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Name request failed - %s", error->message);
      g_error_free (error);

      clear (manager);

      return;
    }

  connection = ibus_bus_get_connection (manager->ibus);
  manager->panel_service = ibus_panel_service_new (connection);

  gf_candidate_popup_set_panel_service (manager->candidate_popup,
                                        manager->panel_service);

  g_signal_connect (manager->panel_service, "update-property",
                    G_CALLBACK (update_property_cb), manager);

  if (IBUS_CHECK_VERSION (1, 5, 10))
    g_signal_connect (manager->panel_service, "set-content-type",
                      G_CALLBACK (set_content_type_cb), manager);

  ibus_bus_get_global_engine_async (manager->ibus, -1, NULL,
                                    get_global_engine_cb, manager);

  update_readiness (manager);
}

static void
connected_cb (IBusBus  *ibus,
              gpointer  user_data)
{
  IBusBusNameFlag flags;

  flags = IBUS_BUS_NAME_FLAG_ALLOW_REPLACEMENT |
          IBUS_BUS_NAME_FLAG_REPLACE_EXISTING;

  ibus_bus_list_engines_async (ibus, -1, NULL, list_engines_cb, user_data);
  ibus_bus_request_name_async (ibus, IBUS_SERVICE_PANEL, flags, -1, NULL,
                               request_name_cb, user_data);
}

static void
disconnected_cb (IBusBus  *ibus,
                 gpointer  user_data)
{
  GfIBusManager *manager;

  manager = GF_IBUS_MANAGER (user_data);

  clear (manager);
}

static void
set_global_engine_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GfIBusManager *manager;
  gboolean result;
  GError *error;

  manager = GF_IBUS_MANAGER (user_data);

  error = NULL;
  result = ibus_bus_set_global_engine_async_finish (manager->ibus, res, &error);

  if (!result)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to set ibus engine - %s", error->message);
      g_error_free (error);
    }

  g_signal_emit (manager, signals[SIGNAL_ENGINE_SET], 0);
}

static void
preload_engines_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  GfIBusManager *manager;
  gboolean result;
  GError *error;

  manager = GF_IBUS_MANAGER (user_data);

  error = NULL;
  result = ibus_bus_preload_engines_async_finish (manager->ibus, res, &error);

  if (!result)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to preload engines - %s", error->message);
      g_error_free (error);
    }
}

static void
gf_ibus_manager_dispose (GObject *object)
{
  GfIBusManager *manager;

  manager = GF_IBUS_MANAGER (object);

  if (manager->candidate_popup != NULL)
    {
      gtk_widget_destroy (GTK_WIDGET (manager->candidate_popup));
      manager->candidate_popup = NULL;
    }

  g_clear_object (&manager->ibus);
  g_clear_object (&manager->subprocess);

  if (manager->engines != NULL)
    {
      g_hash_table_destroy (manager->engines);
      manager->engines = NULL;
    }

  g_clear_object (&manager->panel_service);

  G_OBJECT_CLASS (gf_ibus_manager_parent_class)->dispose (object);
}

static void
gf_ibus_manager_finalize (GObject *object)
{
  GfIBusManager *manager;

  manager = GF_IBUS_MANAGER (object);

  g_free (manager->current_engine_name);

  G_OBJECT_CLASS (gf_ibus_manager_parent_class)->finalize (object);
}

static void
gf_ibus_manager_class_init (GfIBusManagerClass *manager_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (manager_class);

  object_class->dispose = gf_ibus_manager_dispose;
  object_class->finalize = gf_ibus_manager_finalize;

  signals[SIGNAL_READY] =
    g_signal_new ("ready", G_TYPE_FROM_CLASS (manager_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals[SIGNAL_PROPERTIES_REGISTERED] =
    g_signal_new ("properties-registered", G_TYPE_FROM_CLASS (manager_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_STRING, IBUS_TYPE_PROP_LIST);

  signals[SIGNAL_PROPERTY_UPDATED] =
    g_signal_new ("property-updated", G_TYPE_FROM_CLASS (manager_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_STRING, IBUS_TYPE_PROPERTY);

  signals[SIGNAL_SET_CONTENT_TYPE] =
    g_signal_new ("set-content-type", G_TYPE_FROM_CLASS (manager_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

  signals[SIGNAL_ENGINE_SET] =
    g_signal_new ("engine-set", G_TYPE_FROM_CLASS (manager_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gf_ibus_manager_init (GfIBusManager *manager)
{
  if (IBUS_CHECK_VERSION (1, 5, 2) == FALSE)
    {
      g_warning ("Found IBus version %d.%d.%d but required is 1.5.2 or newer.",
                 IBUS_MAJOR_VERSION, IBUS_MINOR_VERSION, IBUS_MICRO_VERSION);
      return;
    }

  ibus_init ();

  manager->candidate_popup = gf_candidate_popup_new ();
  manager->ibus = ibus_bus_new_async ();

  g_signal_connect (manager->ibus, "connected",
                    G_CALLBACK (connected_cb), manager);

  g_signal_connect (manager->ibus, "disconnected",
                    G_CALLBACK (disconnected_cb), manager);

  /* Need to set this to get 'global-engine-changed' emissions */
  ibus_bus_set_watch_ibus_signal (manager->ibus, TRUE);

  g_signal_connect (manager->ibus, "global-engine-changed",
                    G_CALLBACK (global_engine_changed_cb), manager);

  spawn (manager);
}

GfIBusManager *
gf_ibus_manager_new (void)
{
  return g_object_new (GF_TYPE_IBUS_MANAGER, NULL);
}

void
gf_ibus_manager_activate_property (GfIBusManager *manager,
                                   const gchar   *prop_name,
                                   guint          prop_state)
{
  if (!manager->panel_service)
    return;

  ibus_panel_service_property_activate (manager->panel_service,
                                        prop_name, prop_state);
}

IBusEngineDesc *
gf_ibus_manager_get_engine_desc (GfIBusManager *manager,
                                 const gchar   *id)
{
  if (!manager->ready || manager->engines == NULL)
    return NULL;

  return g_hash_table_lookup (manager->engines, id);
}

void
gf_ibus_manager_set_engine (GfIBusManager *manager,
                            const gchar   *id)
{
  if (!manager->ready)
    {
      g_signal_emit (manager, signals[SIGNAL_ENGINE_SET], 0);
      return;
    }

  ibus_bus_set_global_engine_async (manager->ibus, id,
                                    MAX_INPUT_SOURCE_ACTIVATION_TIME,
                                    NULL, set_global_engine_cb, manager);
}

void
gf_ibus_manager_preload_engines (GfIBusManager  *manager,
                                 gchar         **engines)
{
  if (manager->ibus == NULL || g_strv_length (engines) == 0)
    return;

  ibus_bus_preload_engines_async (manager->ibus, (const gchar *const *) engines,
                                  -1, NULL, preload_engines_cb, manager);
}
