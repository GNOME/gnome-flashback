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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib-unix.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "gf-application.h"
#include "gf-session.h"

static GMainLoop *loop = NULL;
static GfApplication *application = NULL;

static gboolean replace = FALSE;
static gboolean version = FALSE;

static GOptionEntry entries[] =
{
  {
    "replace", 'r', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, &replace,
    N_("Replace a currently running application"),
    NULL
  },
  {
    "version", 'v', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, &version,
    N_("Print version and exit"),
    NULL
  },
  {
    NULL
  }
};

static gboolean
parse_arguments (int    *argc,
                 char ***argv)
{
  GOptionContext *context;
  GOptionGroup *gtk_group;
  GError *error;

  context = g_option_context_new (NULL);
  gtk_group = gtk_get_option_group (FALSE);

  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gtk_group);

  error = NULL;
  if (g_option_context_parse (context, argc, argv, &error) == FALSE)
    {
      g_warning ("Failed to parse command line arguments: %s", error->message);
      g_error_free (error);

      g_option_context_free (context);

      return FALSE;
    }

  g_option_context_free (context);

  return TRUE;
}

static void
main_loop_quit (void)
{
  if (application != NULL)
    g_clear_object (&application);

  g_main_loop_quit (loop);
}

static gboolean
on_term_signal (gpointer user_data)
{
  main_loop_quit ();

  return G_SOURCE_REMOVE;
}

static gboolean
on_int_signal (gpointer user_data)
{
  main_loop_quit ();

  return G_SOURCE_REMOVE;
}

static void
session_ready_cb (GfSession *session,
                  gpointer   user_data)
{
  g_unix_signal_add (SIGTERM, on_term_signal, NULL);
  g_unix_signal_add (SIGINT, on_int_signal, NULL);

  application = gf_application_new ();
  gf_session_set_environment (session, "XDG_MENU_PREFIX", "gnome-flashback-");
#ifdef HAVE_COMPIZ_SESSION
  gf_session_set_environment (session, "COMPIZ_CONFIG_PROFILE", "gnome-flashback");
#endif
  gf_session_register (session);
}

static void
session_end_cb (GfSession *session,
                gpointer   user_data)
{
  main_loop_quit ();
}

int
main (int argc, char *argv[])
{
  GfSession *session;

  bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gtk_init (&argc, &argv);

  if (!parse_arguments (&argc, &argv))
    return EXIT_FAILURE;

  if (version)
    {
      g_print (PACKAGE_STRING "\n");

      return EXIT_FAILURE;
    }

  loop = g_main_loop_new (NULL, FALSE);
  session = gf_session_new (replace, session_ready_cb, session_end_cb, NULL);

  g_main_loop_run (loop);

  g_object_unref (session);
  g_main_loop_unref (loop);

  return EXIT_SUCCESS;
}
