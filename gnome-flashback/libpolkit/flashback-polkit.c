/*
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
 */

#include "config.h"

#include "flashback-polkit.h"
#include "flashback-listener.h"

struct _FlashbackPolkit
{
  GObject              parent;

  PolkitSubject       *session;
  PolkitAgentListener *listener;

  gpointer             handle;
};

G_DEFINE_TYPE (FlashbackPolkit, flashback_polkit, G_TYPE_OBJECT)

static void
unix_session_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  FlashbackPolkit *polkit;
  GError *error;

  polkit = FLASHBACK_POLKIT (user_data);

  error = NULL;
  polkit->session = polkit_unix_session_new_for_process_finish (res, &error);

  if (error != NULL)
    {
      g_warning ("Unable to determine the session we are in: %s", error->message);
      g_error_free (error);

      return;
    }

  polkit->listener = flashback_listener_new ();

  error = NULL;
  polkit->handle = polkit_agent_listener_register (polkit->listener,
                                                   POLKIT_AGENT_REGISTER_FLAGS_NONE,
                                                   polkit->session,
                                                   "/org/gnome/PolicyKit1/AuthenticationAgent",
                                                   NULL, /* GCancellable */
                                                   &error);

  if (error != NULL)
    {
      g_warning ("Cannot register authentication agent: %s", error->message);
      g_error_free (error);

      return;
    }
}

static void
flashback_polkit_dispose (GObject *object)
{
  FlashbackPolkit *polkit;

  polkit = FLASHBACK_POLKIT (object);

  if (polkit->handle != NULL)
    {
      polkit_agent_listener_unregister (polkit->handle);
      polkit->handle = NULL;
    }

  g_clear_object (&polkit->session);
  g_clear_object (&polkit->listener);

  G_OBJECT_CLASS (flashback_polkit_parent_class)->dispose (object);
}

static void
flashback_polkit_class_init (FlashbackPolkitClass *polkit_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (polkit_class);

  object_class->dispose = flashback_polkit_dispose;
}

static void
flashback_polkit_init (FlashbackPolkit *polkit)
{
  polkit_unix_session_new_for_process (getpid (), NULL, unix_session_cb, polkit);
}

FlashbackPolkit *
flashback_polkit_new (void)
{
  return g_object_new (FLASHBACK_TYPE_POLKIT, NULL);
}
