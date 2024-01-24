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
#include <pwd.h>

#include "flashback-authenticator.h"
#include "flashback-polkit-dialog.h"

struct _FlashbackAuthenticator
{
  GObject                   parent;

  PolkitAuthority          *authority;

  gchar                    *action_id;
  gchar                    *message;
  gchar                    *icon_name;
  PolkitDetails            *details;
  gchar                    *cookie;
  GList                    *identities;

  PolkitActionDescription  *action_desc;
  gchar                   **users;

  GtkWidget                *dialog;

  gboolean                  gained_authorization;
  gboolean                  was_cancelled;
  gboolean                  new_user_selected;
  gchar                    *selected_user;

  PolkitAgentSession       *session;

  GMainLoop                *loop;

  gulong                    idle_id;
};

enum
{
  SIGNAL_COMPLETED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (FlashbackAuthenticator, flashback_authenticator, G_TYPE_OBJECT)

static gpointer
copy_identities (gconstpointer src,
                 gpointer      data)
{
  return g_object_ref ((gpointer) src);
}

static void
session_request (PolkitAgentSession *session,
                 const char         *request,
                 gboolean            echo_on,
                 gpointer            user_data)
{
  FlashbackAuthenticator *authenticator;
  FlashbackPolkitDialog *dialog;
  gchar *modified_request;
  gchar *password;

  authenticator = FLASHBACK_AUTHENTICATOR (user_data);
  dialog = FLASHBACK_POLKIT_DIALOG (authenticator->dialog);

  /*g_debug ("in conversation_pam_prompt, request='%s', echo_on=%d", request, echo_on);*/

  /* Fix up, and localize, password prompt if it's password auth */
  if (g_ascii_strncasecmp (request, "password:", 9) == 0)
    {
      if (strcmp (g_get_user_name (), authenticator->selected_user) != 0)
        {
          modified_request = g_strdup_printf (_("_Password for %s:"), authenticator->selected_user);
        }
      else
        {
          modified_request = g_strdup (_("_Password:"));
        }
    }
  else
    {
      modified_request = g_strdup (request);
    }

  flashback_polkit_dialog_present (dialog);

  password = flashback_polkit_dialog_run_until_response_for_prompt (dialog,
                                                                    modified_request,
                                                                    echo_on,
                                                                    &authenticator->was_cancelled,
                                                                    &authenticator->new_user_selected);

  g_free (modified_request);

  /* cancel auth unless user provided a password */
  if (password == NULL)
    {
      flashback_authenticator_cancel (authenticator);
      return;
    }

  polkit_agent_session_response (authenticator->session, password);
  g_free (password);
}

static void
session_show_info (PolkitAgentSession *session,
                   const gchar        *msg,
                   gpointer            user_data)
{
  FlashbackAuthenticator *authenticator;
  FlashbackPolkitDialog *dialog;
  gchar *s;

  authenticator = FLASHBACK_AUTHENTICATOR (user_data);
  dialog = FLASHBACK_POLKIT_DIALOG (authenticator->dialog);

  s = g_strconcat ("<b>", msg, "</b>", NULL);
  flashback_polkit_dialog_set_info_message (dialog, s);
  g_free (s);

  flashback_polkit_dialog_present (dialog);
}

static void
session_show_error (PolkitAgentSession *session,
                    const gchar        *msg,
                    gpointer            user_data)
{
  FlashbackAuthenticator *authenticator;
  FlashbackPolkitDialog *dialog;
  gchar *s;

  authenticator = FLASHBACK_AUTHENTICATOR (user_data);
  dialog = FLASHBACK_POLKIT_DIALOG (authenticator->dialog);

  s = g_strconcat ("<b>", msg, "</b>", NULL);
  flashback_polkit_dialog_set_info_message (dialog, s);
  g_free (s);
}

static void
session_completed (PolkitAgentSession *session,
                   gboolean            gained_authorization,
                   gpointer            user_data)
{
  FlashbackAuthenticator *authenticator;

  authenticator = FLASHBACK_AUTHENTICATOR (user_data);

  authenticator->gained_authorization = gained_authorization;

  //g_debug ("in conversation_done gained=%d", gained_authorization);

  g_main_loop_quit (authenticator->loop);
}

static gboolean
do_initiate (gpointer user_data)
{
  FlashbackAuthenticator *authenticator;
  FlashbackPolkitDialog *dialog;
  gint num_tries;

  authenticator = FLASHBACK_AUTHENTICATOR (user_data);
  dialog = FLASHBACK_POLKIT_DIALOG (authenticator->dialog);

  flashback_polkit_dialog_present (dialog);

  if (!flashback_polkit_dialog_run_until_user_is_selected (dialog))
    {
      authenticator->was_cancelled = TRUE;
      authenticator->idle_id = 0;

      g_signal_emit_by_name (authenticator, "completed",
                             authenticator->gained_authorization,
                             authenticator->was_cancelled);

      return G_SOURCE_REMOVE;
    }

  authenticator->loop = g_main_loop_new (NULL, TRUE);

  num_tries = 0;
  while (num_tries < 3)
    {
      PolkitIdentity *identity;

      g_free (authenticator->selected_user);
      authenticator->selected_user = flashback_polkit_dialog_get_selected_user (dialog);

      /*g_warning ("Authenticating user %s", authenticator->selected_user);*/
      identity = polkit_unix_user_new_for_name (authenticator->selected_user, NULL);

      authenticator->session = polkit_agent_session_new (identity, authenticator->cookie);
      g_object_unref (identity);

      g_signal_connect (authenticator->session, "request",
                        G_CALLBACK (session_request), authenticator);
      g_signal_connect (authenticator->session, "show-info",
                        G_CALLBACK (session_show_info), authenticator);
      g_signal_connect (authenticator->session, "show-error",
                        G_CALLBACK (session_show_error), authenticator);
      g_signal_connect (authenticator->session, "completed",
                        G_CALLBACK (session_completed), authenticator);

      polkit_agent_session_initiate (authenticator->session);
      g_main_loop_run (authenticator->loop);

      /*g_warning ("gained_authorization=%d was_cancelled=%d new_user_selected=%d.",
                 authenticator->gained_authorization, authenticator->was_cancelled,
                 authenticator->new_user_selected);*/

      if (authenticator->new_user_selected)
        {
          /*g_warning ("New user selected");*/
          authenticator->new_user_selected = FALSE;
          g_clear_object (&authenticator->session);

          continue;
        }

      num_tries++;

      if (!authenticator->gained_authorization && !authenticator->was_cancelled)
        {
          if (authenticator->dialog != NULL)
            {
              const gchar *s;
              gchar *m;

              s = _("Your authentication attempt was unsuccessful. Please try again.");
              m = g_strconcat ("<b>", s, "</b>", NULL);
              flashback_polkit_dialog_set_info_message (dialog, m);
              g_free (m);

              gtk_widget_queue_draw (authenticator->dialog);

              /* shake the dialog to indicate error */
              flashback_polkit_dialog_indicate_error (dialog);

              g_clear_object (&authenticator->session);

              continue;
            }
        }

      break;
    }

  authenticator->idle_id = 0;

  g_signal_emit_by_name (authenticator, "completed",
                         authenticator->gained_authorization,
                         authenticator->was_cancelled);

  return G_SOURCE_REMOVE;
}

static void
delete_event_cb (GtkWidget *widget,
                 GdkEvent  *event,
                 gpointer   user_data)
{
  FlashbackAuthenticator *authenticator;

  authenticator = FLASHBACK_AUTHENTICATOR (user_data);

  flashback_authenticator_cancel (authenticator);
}

static void
selected_user_cb (GObject    *object,
                  GParamSpec *pspec,
                  gpointer    user_data)
{
  FlashbackAuthenticator *authenticator;
  FlashbackPolkitDialog *dialog;

  authenticator = FLASHBACK_AUTHENTICATOR (user_data);

  dialog = FLASHBACK_POLKIT_DIALOG (authenticator->dialog);
  flashback_polkit_dialog_set_info_message (dialog, "");

  flashback_authenticator_cancel (authenticator);
  authenticator->new_user_selected = TRUE;
}

static PolkitActionDescription *
get_desc_for_action (FlashbackAuthenticator *authenticator)
{
  PolkitActionDescription *result;
  GList *descs;
  GList *l;

  result = NULL;
  descs = polkit_authority_enumerate_actions_sync (authenticator->authority,
                                                   NULL, NULL);

  for (l = descs; l != NULL; l = l->next)
    {
      PolkitActionDescription *action_desc;
      const gchar *action_id;

      action_desc = POLKIT_ACTION_DESCRIPTION (l->data);
      action_id = polkit_action_description_get_action_id (action_desc);

      if (g_strcmp0 (action_id, authenticator->action_id) == 0)
        {
          result = g_object_ref (action_desc);
          break;
        }
    }

  g_list_free_full (descs, g_object_unref);

  return result;
}

static void
flashback_authenticator_dispose (GObject *object)
{
  FlashbackAuthenticator *authenticator;

  authenticator = FLASHBACK_AUTHENTICATOR (object);

  if (authenticator->idle_id > 0)
    {
      g_source_remove (authenticator->idle_id);
      authenticator->idle_id = 0;
    }

  g_clear_object (&authenticator->authority);
  g_clear_object (&authenticator->details);
  g_clear_object (&authenticator->action_desc);

  if (authenticator->dialog != NULL)
    {
      gtk_widget_destroy (authenticator->dialog);
      authenticator->dialog = NULL;
    }

  g_clear_object (&authenticator->session);

  if (authenticator->loop != NULL)
    {
      g_main_loop_unref (authenticator->loop);
      authenticator->loop = NULL;
    }

  G_OBJECT_CLASS (flashback_authenticator_parent_class)->dispose (object);
}

static void
flashback_authenticator_finalize (GObject *object)
{
  FlashbackAuthenticator *authenticator;

  authenticator = FLASHBACK_AUTHENTICATOR (object);

  g_free (authenticator->action_id);
  g_free (authenticator->message);
  g_free (authenticator->icon_name);
  g_free (authenticator->cookie);

  g_list_free_full (authenticator->identities, g_object_unref);

  g_strfreev (authenticator->users);

  g_free (authenticator->selected_user);

  G_OBJECT_CLASS (flashback_authenticator_parent_class)->finalize (object);
}

static void
flashback_authenticator_class_init (FlashbackAuthenticatorClass *authenticator_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (authenticator_class);

  object_class->dispose = flashback_authenticator_dispose;
  object_class->finalize = flashback_authenticator_finalize;

  /**
   * FlashbackAuthenticator::completed
   * @authenticator: A #PolkitGnomeAuthenticator.
   * @gained_authorization: Whether the authorization was gained.
   * @dismissed: Whether the dialog was dismissed.
   *
   * Emitted when the authentication is completed. The user is supposed to
   * dispose of @authenticator upon receiving this signal.
   */
  signals[SIGNAL_COMPLETED] =
    g_signal_new ("completed",
                  G_OBJECT_CLASS_TYPE (authenticator_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 2, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
}

static void
flashback_authenticator_init (FlashbackAuthenticator *authenticator)
{
}

FlashbackAuthenticator *
flashback_authenticator_new (const gchar   *action_id,
                             const gchar   *message,
                             const gchar   *icon_name,
                             PolkitDetails *details,
                             const gchar   *cookie,
                             GList         *identities)
{
  FlashbackAuthenticator *authenticator;
  GError *error;
  gint users;
  gint i;
  GList *l;
  const gchar *vendor;
  const gchar *vendor_url;

  authenticator = g_object_new (FLASHBACK_TYPE_AUTHENTICATOR, NULL);

  error = NULL;
  authenticator->authority = polkit_authority_get_sync (NULL, &error);

  if (error != NULL)
    {
      g_critical ("Error getting authority: %s", error->message);
      g_error_free (error);

      g_object_unref (authenticator);
      return NULL;
    }

  authenticator->action_id = g_strdup (action_id);
  authenticator->message = g_strdup (message);
  authenticator->icon_name = g_strdup (icon_name);
  authenticator->details = details != NULL ? g_object_ref (details) : NULL;
  authenticator->cookie = g_strdup (cookie);
  authenticator->identities = g_list_copy_deep (identities, copy_identities, NULL);
  authenticator->action_desc = get_desc_for_action (authenticator);

  if (authenticator->action_desc == NULL)
    {
      g_object_unref (authenticator);
      return NULL;
    }

  users = g_list_length (authenticator->identities);
  authenticator->users = g_new0 (gchar *, users + 1);

  for (l = authenticator->identities, i = 0; l != NULL; l = l->next, i++)
    {
      PolkitUnixUser *user;
      uid_t uid;
      struct passwd *passwd;

      user = POLKIT_UNIX_USER (l->data);
      uid = polkit_unix_user_get_uid (user);
      passwd = getpwuid (uid);
      authenticator->users[i] = g_strdup (passwd->pw_name);
    }

  vendor = polkit_action_description_get_vendor_name (authenticator->action_desc);
  vendor_url = polkit_action_description_get_vendor_url (authenticator->action_desc);

  authenticator->dialog = flashback_polkit_dialog_new (authenticator->action_id,
                                                       vendor, vendor_url,
                                                       authenticator->icon_name,
                                                       authenticator->message,
                                                       authenticator->details,
                                                       authenticator->users);

  g_signal_connect (authenticator->dialog, "delete-event",
                    G_CALLBACK (delete_event_cb), authenticator);
  g_signal_connect (authenticator->dialog, "notify::selected-user",
                    G_CALLBACK (selected_user_cb), authenticator);

  return authenticator;
}

void
flashback_authenticator_initiate (FlashbackAuthenticator *authenticator)
{
  authenticator->idle_id = g_idle_add (do_initiate, authenticator);
  g_source_set_name_by_id (authenticator->idle_id, "[gnome-flashback] do_initiate");
}

void
flashback_authenticator_cancel (FlashbackAuthenticator *authenticator)
{
  FlashbackPolkitDialog *dialog;

  dialog = FLASHBACK_POLKIT_DIALOG (authenticator->dialog);
  flashback_polkit_dialog_cancel (dialog);

  authenticator->was_cancelled = TRUE;

  if (authenticator->session != NULL)
    polkit_agent_session_cancel (authenticator->session);
}

const gchar *
flashback_authenticator_get_cookie (FlashbackAuthenticator *authenticator)
{
  return authenticator->cookie;
}
