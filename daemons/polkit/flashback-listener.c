/*
 * Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2015 Alberts Muktupāvels
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <glib/gi18n.h>

#include "flashback-authenticator.h"
#include "flashback-listener.h"

struct _FlashbackListener
{
  PolkitAgentListener     parent;

  GList                  *authenticators;
  FlashbackAuthenticator *active;
};

typedef struct
{
  FlashbackListener      *listener;
  FlashbackAuthenticator *authenticator;

  GTask                  *task;

  GCancellable           *cancellable;
  gulong                  cancel_id;
} AuthData;

G_DEFINE_TYPE (FlashbackListener, flashback_listener, POLKIT_AGENT_TYPE_LISTENER)

static void
flashback_listener_dispose (GObject *object)
{
  G_OBJECT_CLASS (flashback_listener_parent_class)->dispose (object);
}

static void
maybe_initiate_next_authenticator (FlashbackListener *listener)
{
  FlashbackAuthenticator *authenticator;

  if (listener->authenticators == NULL)
    return;

  if (listener->active != NULL)
    return;

  authenticator = FLASHBACK_AUTHENTICATOR (listener->authenticators->data);

  flashback_authenticator_initiate (authenticator);
  listener->active = authenticator;
}

static AuthData *
auth_data_new (FlashbackListener      *listener,
               FlashbackAuthenticator *authenticator,
               GTask                  *task,
               GCancellable           *cancellable)
{
  AuthData *data;

  data = g_new0 (AuthData, 1);

  data->listener = g_object_ref (listener);
  data->authenticator = g_object_ref (authenticator);
  data->task = g_object_ref (task);
  data->cancellable = g_object_ref (cancellable);

  return data;
}

static void
auth_data_free (AuthData *data)
{
  g_object_unref (data->listener);
  g_object_unref (data->authenticator);
  g_object_unref (data->task);

  if (data->cancellable != NULL)
    {
      if (data->cancel_id > 0)
        g_signal_handler_disconnect (data->cancellable, data->cancel_id);
      g_object_unref (data->cancellable);
    }

  g_free (data);
}

static void
cancelled_cb (GCancellable *cancellable,
              gpointer      user_data)
{
  AuthData *data;

  data = (AuthData *) user_data;

  flashback_authenticator_cancel (data->authenticator);
}

static void
completed_cb (FlashbackAuthenticator *authenticator,
              gboolean                gained_authorization,
              gboolean                dismissed,
              gpointer                user_data)
{
  AuthData *data;

  data = (AuthData *) user_data;

  data->listener->authenticators = g_list_remove (data->listener->authenticators, authenticator);
  if (authenticator == data->listener->active)
    data->listener->active = NULL;

  g_object_unref (authenticator);

  if (dismissed)
    {
      g_task_return_new_error (data->task, POLKIT_ERROR, POLKIT_ERROR_CANCELLED,
                               _("Authentication dialog was dismissed by the user"));
    }
  else
    {
      g_task_return_boolean (data->task, TRUE);
    }

  g_object_unref (data->task);

  maybe_initiate_next_authenticator (data->listener);
  auth_data_free (data);
}

static void
flashback_listener_initiate_authentication (PolkitAgentListener  *agent_listener,
                                            const gchar          *action_id,
                                            const gchar          *message,
                                            const gchar          *icon_name,
                                            PolkitDetails        *details,
                                            const gchar          *cookie,
                                            GList                *identities,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data)
{
  FlashbackListener *listener;
  GTask *task;
  FlashbackAuthenticator *authenticator;
  AuthData *data;

  listener = FLASHBACK_LISTENER (agent_listener);

  authenticator = flashback_authenticator_new (action_id, message, icon_name,
                                               details, cookie, identities);

  task = g_task_new (listener, cancellable, callback, user_data);

  if (authenticator == NULL)
    {
      g_task_return_new_error (task, POLKIT_ERROR, POLKIT_ERROR_FAILED,
                               "Error creating authentication object");
      g_object_unref (task);

      return;
    }

  data = auth_data_new (listener, authenticator, task, cancellable);

  if (cancellable != NULL)
    data->cancel_id = g_signal_connect (cancellable, "cancelled",
                                        G_CALLBACK (cancelled_cb), data);

  g_signal_connect (authenticator, "completed",
                    G_CALLBACK (completed_cb), data);

  listener->authenticators = g_list_append (listener->authenticators, authenticator);

  maybe_initiate_next_authenticator (listener);
}

static gboolean
flashback_listener_initiate_authentication_finish (PolkitAgentListener  *listener,
                                                   GAsyncResult         *res,
                                                   GError              **error)
{
  g_return_val_if_fail (g_task_is_valid (res, listener), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

static void
flashback_listener_class_init (FlashbackListenerClass *listener_class)
{
  GObjectClass *object_class;
  PolkitAgentListenerClass *agent_class;

  object_class = G_OBJECT_CLASS (listener_class);
  agent_class = POLKIT_AGENT_LISTENER_CLASS (listener_class);

  object_class->dispose = flashback_listener_dispose;

  agent_class->initiate_authentication = flashback_listener_initiate_authentication;
  agent_class->initiate_authentication_finish = flashback_listener_initiate_authentication_finish;
}

static void
flashback_listener_init (FlashbackListener *listener)
{
}

PolkitAgentListener *
flashback_listener_new (void)
{
  return g_object_new (FLASHBACK_TYPE_LISTENER, NULL);
}
