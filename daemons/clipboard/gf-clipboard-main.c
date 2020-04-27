/*
 * Copyright (C) 2020 Alberts Muktupāvels
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

#include <glib-unix.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "dbus/gf-session-manager-gen.h"
#include "dbus/gf-sm-client-private-gen.h"
#include "gf-clipboard.h"

static char *startup_id = NULL;
static GMainLoop *loop = NULL;
static GfSmClientPrivateGen *client_private = NULL;

static gboolean
on_term_signal (gpointer user_data)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static gboolean
on_int_signal (gpointer user_data)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static void
respond_to_end_session (void)
{
  gf_sm_client_private_gen_call_end_session_response (client_private,
                                                      TRUE,
                                                      "",
                                                      NULL,
                                                      NULL,
                                                      NULL);
}

static void
end_session_cb (GfSmClientPrivateGen *object,
                guint                 flags,
                gpointer              user_data)
{
  respond_to_end_session ();
}

static void
query_end_session_cb (GfSmClientPrivateGen *object,
                      guint                 flags,
                      gpointer              user_data)
{
  respond_to_end_session ();
}

static void
stop_cb (GfSmClientPrivateGen *object,
         gpointer              user_data)
{
  g_main_loop_quit (loop);
}

static void
client_private_ready_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  GError *error;

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

  g_signal_connect (client_private,
                    "end-session",
                    G_CALLBACK (end_session_cb),
                    NULL);

  g_signal_connect (client_private,
                    "query-end-session",
                    G_CALLBACK (query_end_session_cb),
                    NULL);

  g_signal_connect (client_private,
                    "stop",
                    G_CALLBACK (stop_cb),
                    NULL);
}

static void
register_client_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  GError *error;
  char *client_id;

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

  gf_sm_client_private_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                              G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                              "org.gnome.SessionManager",
                                              client_id,
                                              NULL,
                                              client_private_ready_cb,
                                              NULL);

  g_free (client_id);
}

static void
session_manager_ready_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  GError *error;
  GfSessionManagerGen *session_manager;

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

  gf_session_manager_gen_call_register_client (session_manager,
                                               "gnome-flashback-clipboard",
                                               startup_id,
                                               NULL,
                                               register_client_cb,
                                               NULL);

  g_object_unref (session_manager);
}

int
main (int argc,
      char *argv[])
{
  const char *autostart_id;
  GfClipboard *clipboard;

  autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
  startup_id = g_strdup (autostart_id != NULL ? autostart_id : "");
  g_unsetenv ("DESKTOP_AUTOSTART_ID");

  gtk_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);
  clipboard = gf_clipboard_new ();

  g_unix_signal_add (SIGTERM, on_term_signal, NULL);
  g_unix_signal_add (SIGINT, on_int_signal, NULL);

  gf_session_manager_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                            G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                            "org.gnome.SessionManager",
                                            "/org/gnome/SessionManager",
                                            NULL,
                                            session_manager_ready_cb,
                                            NULL);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  g_object_unref (clipboard);
  g_clear_object (&client_private);
  g_free (startup_id);

  return EXIT_SUCCESS;
}
