/*
 * Copyright (C) 2014 - 2019 Alberts Muktupāvels
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
#include "gf-session.h"

#include "dbus/gf-session-manager-gen.h"
#include "dbus/gf-sm-client-private-gen.h"

struct _GfSession
{
  GObject               parent;

  gboolean              replace;
  char                 *startup_id;

  guint                 name_id;
  gboolean              name_acquired;

  GCancellable         *cancellable;

  GfSessionManagerGen  *session_manager;

  char                 *client_id;
  GfSmClientPrivateGen *client_private;
};

enum
{
  PROP_0,

  PROP_REPLACE,
  PROP_STARTUP_ID,

  LAST_PROP
};

static GParamSpec *session_properties[LAST_PROP] = { NULL };

enum
{
  NAME_LOST,

  SESSION_READY,
  END_SESSION,

  LAST_SIGNAL
};

static guint session_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfSession, gf_session, G_TYPE_OBJECT)

static void
respond_to_end_session (GfSession *self)
{
  gf_sm_client_private_gen_call_end_session_response (self->client_private,
                                                      TRUE,
                                                      "",
                                                      self->cancellable,
                                                      NULL,
                                                      NULL);
}

static void
end_session_cb (GfSmClientPrivateGen *object,
                guint                 flags,
                GfSession            *self)
{
  respond_to_end_session (self);
}

static void
query_end_session_cb (GfSmClientPrivateGen *object,
                      guint                 flags,
                      GfSession            *self)
{
  respond_to_end_session (self);
}

static void
stop_cb (GfSmClientPrivateGen *object,
         GfSession            *self)
{
  g_signal_emit (self, session_signals[END_SESSION], 0);
}

static void
client_private_ready_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  GError *error;
  GfSmClientPrivateGen *client_private;
  GfSession *self;

  error = NULL;
  client_private = gf_sm_client_private_gen_proxy_new_for_bus_finish (res,
                                                                      &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to get a client private proxy: %s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_SESSION (user_data);
  self->client_private = client_private;

  g_signal_connect (self->client_private,
                    "end-session",
                    G_CALLBACK (end_session_cb),
                    self);

  g_signal_connect (self->client_private,
                    "query-end-session",
                    G_CALLBACK (query_end_session_cb),
                    self);

  g_signal_connect (self->client_private,
                    "stop",
                    G_CALLBACK (stop_cb),
                    self);
}

static void
register_client_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  GError *error;
  char *client_id;
  GfSession *self;

  error = NULL;
  gf_session_manager_gen_call_register_client_finish (GF_SESSION_MANAGER_GEN (source_object),
                                                      &client_id,
                                                      res,
                                                      &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to register client: %s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_SESSION (user_data);
  self->client_id = client_id;

  gf_sm_client_private_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                              G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                              "org.gnome.SessionManager",
                                              self->client_id,
                                              self->cancellable,
                                              client_private_ready_cb,
                                              self);
}

static void
setenv_cb (GObject      *source_object,
           GAsyncResult *res,
           gpointer      user_data)
{
  GError *error;

  error = NULL;
  gf_session_manager_gen_call_setenv_finish (GF_SESSION_MANAGER_GEN (source_object),
                                             res,
                                             &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to set the environment: %s", error->message);

      g_error_free (error);
      return;
    }
}

static void
is_session_running_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GError *error;
  gboolean is_session_running;
  GfSession *self;

  error = NULL;
  gf_session_manager_gen_call_is_session_running_finish (GF_SESSION_MANAGER_GEN (source_object),
                                                         &is_session_running,
                                                         res,
                                                         &error);

  if (error != NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_error_free (error);
          return;
        }
      else
        {
          is_session_running = TRUE;

          g_warning ("Failed to check if session has entered the Running phase: %s",
                     error->message);

          g_error_free (error);
        }
    }

  self = GF_SESSION (user_data);

  g_signal_emit (self, session_signals[SESSION_READY], 0, is_session_running);
}

static void
session_manager_ready_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  GError *error;
  GfSessionManagerGen *session_manager;
  GfSession *self;

  error = NULL;
  session_manager = gf_session_manager_gen_proxy_new_for_bus_finish (res,
                                                                     &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to get session manager proxy: %s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_SESSION (user_data);
  self->session_manager = session_manager;

  gf_session_manager_gen_call_is_session_running (self->session_manager,
                                                  self->cancellable,
                                                  is_session_running_cb,
                                                  self);
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
  GfSession *self;
  GDBusProxyFlags flags;

  self = GF_SESSION (user_data);
  self->name_acquired = TRUE;

  flags = G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
          G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS;

  gf_session_manager_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                            flags,
                                            "org.gnome.SessionManager",
                                            "/org/gnome/SessionManager",
                                            self->cancellable,
                                            session_manager_ready_cb,
                                            self);
}

static void
name_lost_cb (GDBusConnection *connection,
              const char      *name,
              gpointer         user_data)
{
  GfSession *self;

  self = GF_SESSION (user_data);

  g_signal_emit (self, session_signals[NAME_LOST], 0, self->name_acquired);
}

static void
gf_session_constructed (GObject *object)
{
  GfSession *self;
  GBusNameOwnerFlags flags;

  self = GF_SESSION (object);

  G_OBJECT_CLASS (gf_session_parent_class)->constructed (object);

  flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
  if (self->replace)
    flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

  self->name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                  "org.gnome.Flashback",
                                  flags,
                                  NULL,
                                  name_acquired_cb,
                                  name_lost_cb,
                                  self,
                                  NULL);
}

static void
gf_session_dispose (GObject *object)
{
  GfSession *self;

  self = GF_SESSION (object);

  if (self->name_id != 0)
    {
      g_bus_unown_name (self->name_id);
      self->name_id = 0;
    }

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->session_manager);
  g_clear_object (&self->client_private);

  G_OBJECT_CLASS (gf_session_parent_class)->dispose (object);
}

static void
gf_session_finalize (GObject *object)
{
  GfSession *self;

  self = GF_SESSION (object);

  g_clear_pointer (&self->startup_id, g_free);
  g_clear_pointer (&self->client_id, g_free);

  G_OBJECT_CLASS (gf_session_parent_class)->finalize (object);
}

static void
gf_session_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GfSession *self;

  self = GF_SESSION (object);

  switch (property_id)
    {
      case PROP_REPLACE:
        self->replace = g_value_get_boolean (value);
        break;

      case PROP_STARTUP_ID:
        self->startup_id = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  session_properties[PROP_REPLACE] =
    g_param_spec_boolean ("replace",
                          "replace",
                          "replace",
                          FALSE,
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_WRITABLE |
                          G_PARAM_STATIC_STRINGS);

  session_properties[PROP_STARTUP_ID] =
    g_param_spec_string ("startup-id",
                         "startup-id",
                         "startup-id",
                         "",
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     LAST_PROP,
                                     session_properties);
}

static void
install_signals (void)
{
  session_signals[NAME_LOST] =
    g_signal_new ("name-lost",
                  GF_TYPE_SESSION,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  session_signals[SESSION_READY] =
    g_signal_new ("session-ready",
                  GF_TYPE_SESSION,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  session_signals[END_SESSION] =
    g_signal_new ("end-session",
                  GF_TYPE_SESSION,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
gf_session_class_init (GfSessionClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = gf_session_constructed;
  object_class->dispose = gf_session_dispose;
  object_class->finalize = gf_session_finalize;
  object_class->set_property = gf_session_set_property;

  install_properties (object_class);
  install_signals ();
}

static void
gf_session_init (GfSession *self)
{
  self->cancellable = g_cancellable_new ();
}

GfSession *
gf_session_new (gboolean    replace,
                const char *startup_id)
{
  return g_object_new (GF_TYPE_SESSION,
                       "replace", replace,
                       "startup-id", startup_id,
                       NULL);
}

void
gf_session_set_environment (GfSession  *self,
                            const char *name,
                            const char *value)
{
  gf_session_manager_gen_call_setenv (self->session_manager,
                                      name,
                                      value,
                                      self->cancellable,
                                      setenv_cb,
                                      self);
}

void
gf_session_register (GfSession *self)
{
  gf_session_manager_gen_call_register_client (self->session_manager,
                                               "gnome-flashback",
                                               self->startup_id,
                                               self->cancellable,
                                               register_client_cb,
                                               self);
}
