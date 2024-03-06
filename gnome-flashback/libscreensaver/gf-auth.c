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
#include "gf-auth.h"

#include <glib/gi18n.h>
#include <security/pam_appl.h>

#include "gf-screensaver-enum-types.h"

typedef struct
{
  GfAuth            *self;

  GfAuthMessageType  type;
  char              *message;

  unsigned int      *id;
} GfMessageData;

struct _GfAuth
{
  GObject       parent;

  GCancellable *cancellable;

  GMutex        mutex;
  GCond         cond;

  char         *username;
  char         *display;

  GTask        *task;

  GList        *message_ids;

  gboolean      awaits_response;
  char         *response;
};

enum
{
  PROP_0,

  PROP_USERNAME,
  PROP_DISPLAY,

  LAST_PROP
};

static GParamSpec *auth_properties[LAST_PROP] = { NULL };

enum
{
  MESSAGE,
  COMPLETE,

  LAST_SIGNAL
};

static guint auth_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfAuth, gf_auth, G_TYPE_OBJECT)

static GQuark
gf_auth_error_quark (void)
{
  static GQuark quark;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("gf-auth-error-quark");

  return quark;
}

static GfMessageData *
gf_message_data_new (GfAuth            *self,
                     GfAuthMessageType  type,
                     const char        *message)
{
  GfMessageData *msg;

  msg = g_new0 (GfMessageData, 1);
  msg->self = g_object_ref (self);

  msg->type = type;
  msg->message = g_strdup (message);

  return msg;
}

static void
gf_message_data_free (gpointer data)
{
  GfMessageData *msg;

  msg = data;

  g_object_unref (msg->self);
  g_free (msg->message);
  g_free (msg);
}

static GfAuthMessageType
message_type_from_msg_style (int msg_style)
{
  GfAuthMessageType type;

  switch (msg_style)
    {
      case PAM_PROMPT_ECHO_OFF:
        type = GF_AUTH_MESSAGE_PROMPT_ECHO_OFF;
        break;

      case PAM_PROMPT_ECHO_ON:
        type = GF_AUTH_MESSAGE_PROMPT_ECHO_ON;
        break;

      case PAM_ERROR_MSG:
        type = GF_AUTH_MESSAGE_ERROR_MSG;
        break;

      case PAM_TEXT_INFO:
        type = GF_AUTH_MESSAGE_TEXT_INFO;
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  return type;
}

static gboolean
message_cb (gpointer user_data)
{
  GfMessageData *data;
  GfAuth *self;
  GList *l;

  data = user_data;
  self = data->self;

  g_signal_emit (self, auth_signals[MESSAGE], 0, data->type, data->message);

  l = g_list_find (self->message_ids, data->id);
  g_assert (l != NULL);

  self->message_ids = g_list_remove_link (self->message_ids, l);
  g_list_free_full (l, g_free);

  return G_SOURCE_REMOVE;
}

static void
emit_message_idle (GfAuth     *self,
                   int         msg_style,
                   const char *msg)
{
  GfAuthMessageType message_type;
  GfMessageData *data;
  unsigned int *message_id;

  message_type = message_type_from_msg_style (msg_style);
  data = gf_message_data_new (self, message_type, msg);

  message_id = data->id = g_new0 (guint, 1);
  self->message_ids = g_list_append (self->message_ids, message_id);

  *message_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                 message_cb,
                                 data,
                                 gf_message_data_free);

  g_source_set_name_by_id (*message_id, "[gnome-flashback] message_cb");
}

static int
conversation_cb (int                        num_msg,
                 const struct pam_message **msg,
                 struct pam_response      **resp,
                 void                      *appdata_ptr)
{
  GTask *task;
  GCancellable *cancellable;
  GfAuth *self;
  struct pam_response *response;
  int i;

  task = G_TASK (appdata_ptr);
  cancellable = g_task_get_cancellable (task);
  self = g_task_get_task_data (task);

  if (g_task_return_error_if_cancelled (task))
    return PAM_CONV_ERR;

  if (num_msg <= 0)
    return PAM_CONV_ERR;

  response = calloc (num_msg, sizeof (struct pam_response));

  if (response == NULL)
    return PAM_CONV_ERR;

  for (i = 0; i < num_msg; i++)
    {
      gboolean failed;

      failed = FALSE;

      g_mutex_lock (&self->mutex);
      emit_message_idle (self, msg[i]->msg_style, msg[i]->msg);

      switch (msg[i]->msg_style)
        {
          case PAM_PROMPT_ECHO_ON:
          case PAM_PROMPT_ECHO_OFF:
            self->awaits_response = TRUE;
            g_cond_wait (&self->cond, &self->mutex);
            self->awaits_response = FALSE;

            if (self->response != NULL)
              {
                response[i].resp = strdup (self->response);
                g_clear_pointer (&self->response, g_free);
              }
            else
              {
                failed = TRUE;
              }
            break;

          case PAM_ERROR_MSG:
          case PAM_TEXT_INFO:
            break;

          default:
            failed = TRUE;
            break;
        }

      g_mutex_unlock (&self->mutex);

      if (failed || g_cancellable_is_cancelled (cancellable))
        {
          int j;

          for (j = 0; j <= i; j++)
            {
              free (response[j].resp);
              response[j].resp = NULL;
            }

          free (response);
          response = NULL;

          return PAM_CONV_ERR;
        }
    }

  *resp = response;

  return PAM_SUCCESS;
}

static void
terminate_pam_transaction (pam_handle_t *handle,
                           int           status)
{
  status = pam_end (handle, status);

  if (status == PAM_SUCCESS)
    return;

  g_debug ("Failed to terminate PAM transaction (%d): %s",
           status, pam_strerror (NULL, status));
}

static void
verify_in_thread_cb (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  GfAuth *self;
  struct pam_conv conv;
  pam_handle_t *handle;
  int status;

  if (g_task_return_error_if_cancelled (task))
    return;

  self = g_task_get_task_data (task);

  conv.conv = conversation_cb;
  conv.appdata_ptr = task;

  handle = NULL;

  status = pam_start ("gnome-flashback", self->username, &conv, &handle);

  if (status != PAM_SUCCESS)
    {
      g_task_return_new_error (task,
                               gf_auth_error_quark (),
                               status,
                               "%s",
                               pam_strerror (handle, status));

      terminate_pam_transaction (handle, status);
      return;
    }

  status = pam_set_item (handle, PAM_TTY, self->display);

  if (status != PAM_SUCCESS)
    {
      g_task_return_new_error (task,
                               gf_auth_error_quark (),
                               status,
                               "%s",
                               pam_strerror (handle, status));

      terminate_pam_transaction (handle, status);
      return;
    }

  status = pam_authenticate (handle, 0);

  if (g_task_return_error_if_cancelled (task))
    {
      terminate_pam_transaction (handle, status);
      return;
    }

  if (status != PAM_SUCCESS)
    {
      g_task_return_new_error (task,
                               gf_auth_error_quark (),
                               status,
                               "%s",
                               pam_strerror (handle, status));

      terminate_pam_transaction (handle, status);
      return;
    }

  status = pam_acct_mgmt (handle, 0);

  if (status != PAM_SUCCESS)
    {
      g_task_return_new_error (task,
                               gf_auth_error_quark (),
                               status,
                               "%s",
                               pam_strerror (handle, status));

      terminate_pam_transaction (handle, status);
      return;
    }

  status = pam_setcred (handle, PAM_REINITIALIZE_CRED);

  if (status != PAM_SUCCESS)
    {
      g_task_return_new_error (task,
                               gf_auth_error_quark (),
                               status,
                               "%s",
                               pam_strerror (handle, status));

      terminate_pam_transaction (handle, status);
      return;
    }

  terminate_pam_transaction (handle, status);
  g_task_return_boolean (task, TRUE);
}

static void
verify_done_cb (GObject      *object,
                GAsyncResult *res,
                gpointer      user_data)
{
  GError *error;
  gboolean verified;
  GfAuth *self;
  const char *message;

  error = NULL;
  verified = g_task_propagate_boolean (G_TASK (res), &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  self = GF_AUTH (user_data);

  message = "";
  if (error != NULL)
    {
      g_debug ("verify_done_cb (%d): %s", error->code, error->message);

      if (error->domain == gf_auth_error_quark ())
        {
          switch (error->code)
            {
              case PAM_SUCCESS:
              case PAM_IGNORE:
                break;

              case PAM_ACCT_EXPIRED:
              case PAM_AUTHTOK_EXPIRED:
                message = _("Your account was given a time limit that has now passed.");
                break;

              default:
                message = _("Sorry, that didn’t work. Please try again.");
                break;
            }
        }
    }

  g_signal_emit (self,
                 auth_signals[COMPLETE],
                 0,
                 verified,
                 message);

  g_clear_error (&error);
}

static void
task_finalized_cb (gpointer  data,
                   GObject  *where_the_object_was)
{
  GfAuth *self;

  self = GF_AUTH (data);
  self->task = NULL;
}

static void
gf_auth_dispose (GObject *object)
{
  GfAuth *self;

  self = GF_AUTH (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_mutex_lock (&self->mutex);

  if (self->message_ids != NULL)
    {
      GList *l;

      for (l = self->message_ids; l != NULL; l = l->next)
        g_source_remove (*((unsigned int *) l->data));

      g_list_free_full (self->message_ids, g_free);
      self->message_ids = NULL;
    }

  g_assert (!self->awaits_response);

  g_mutex_unlock (&self->mutex);

  G_OBJECT_CLASS (gf_auth_parent_class)->dispose (object);
}

static void
gf_auth_finalize (GObject *object)
{
  GfAuth *self;

  self = GF_AUTH (object);

  g_clear_pointer (&self->username, g_free);
  g_clear_pointer (&self->display, g_free);

  g_clear_pointer (&self->response, g_free);

  g_mutex_clear (&self->mutex);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (gf_auth_parent_class)->finalize (object);
}

static void
gf_auth_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  GfAuth *self;

  self = GF_AUTH (object);

  switch (property_id)
    {
      case PROP_USERNAME:
        g_assert (self->username == NULL);
        self->username = g_value_dup_string (value);
        break;

      case PROP_DISPLAY:
        g_assert (self->display == NULL);
        self->display = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  auth_properties[PROP_USERNAME] =
    g_param_spec_string ("username",
                         "username",
                         "username",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  auth_properties[PROP_DISPLAY] =
    g_param_spec_string ("display",
                         "display",
                         "display",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, auth_properties);
}

static void
install_signals (void)
{
  auth_signals[MESSAGE] =
    g_signal_new ("message",
                  GF_TYPE_AUTH,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  2,
                  GF_TYPE_AUTH_MESSAGE_TYPE,
                  G_TYPE_STRING);

  auth_signals[COMPLETE] =
    g_signal_new ("complete",
                  GF_TYPE_AUTH,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_BOOLEAN,
                  G_TYPE_STRING);
}

static void
gf_auth_class_init (GfAuthClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_auth_dispose;
  object_class->finalize = gf_auth_finalize;
  object_class->set_property = gf_auth_set_property;

  install_properties (object_class);
  install_signals ();
}

static void
gf_auth_init (GfAuth *self)
{
  self->cancellable = g_cancellable_new ();

  g_mutex_init (&self->mutex);
  g_cond_init (&self->cond);
}

GfAuth *
gf_auth_new (const char *username,
             const char *display)
{
  return g_object_new (GF_TYPE_AUTH,
                       "username", username,
                       "display", display,
                       NULL);
}

gboolean
gf_auth_awaits_response (GfAuth *self)
{
  return self->awaits_response;
}

void
gf_auth_set_response (GfAuth     *self,
                      const char *response)
{
  if (!self->awaits_response)
    return;

  g_assert (self->response == NULL);
  self->response = g_strdup (response);

  g_mutex_lock (&self->mutex);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->mutex);
}

void
gf_auth_verify (GfAuth *self)
{
  if (self->task != NULL)
    return;

  self->task = g_task_new (NULL, self->cancellable, verify_done_cb, self);
  g_task_set_task_data (self->task, g_object_ref (self), g_object_unref);

  g_object_weak_ref (G_OBJECT (self->task), task_finalized_cb, self);

  g_task_run_in_thread (self->task, verify_in_thread_cb);
  g_object_unref (self->task);
}

void
gf_auth_cancel (GfAuth *self)
{
  g_cancellable_cancel (self->cancellable);
  gf_auth_set_response (self, NULL);
}
