/*
 * Copyright (C) 2014 Alberts Muktupāvels
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

#include "flashback-application.h"
#include "gf-session.h"

static gboolean initialize = FALSE;
static gboolean replace = FALSE;

static GOptionEntry entries[] = {
	{
		"initialize", 0, G_OPTION_FLAG_NONE,
		G_OPTION_ARG_NONE, &initialize,
		N_("Initialize GNOME Flashback session"),
		NULL
	},
	{
		"replace", 'r', G_OPTION_FLAG_NONE,
		G_OPTION_ARG_NONE, &replace,
		N_("Replace a currently running application"),
		NULL
	},
	{
		NULL
	}
};

static gboolean
parse_context_options (int *argc, char ***argv)
{
	GError *error;
	GOptionContext *context;

	error = NULL;
	context = g_option_context_new (NULL);

	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));

	if (!g_option_context_parse (context, argc, argv, &error)) {
		if (error != NULL) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}

		return FALSE;
	}

	g_option_context_free (context);

	return TRUE;
}

static gboolean
on_term_signal (gpointer user_data)
{
        gtk_main_quit ();

        return FALSE;
}

static gboolean
on_int_signal (gpointer user_data)
{
        gtk_main_quit ();

        return FALSE;
}

int
main (int argc, char *argv[])
{
	FlashbackApplication *application;
	FlashbackSession *session;

	g_set_prgname ("gnome-flashback");

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	g_unix_signal_add (SIGTERM, on_term_signal, NULL);
	g_unix_signal_add (SIGINT, on_int_signal, NULL);

	if (!parse_context_options (&argc, &argv))
		return 1;

	session = flashback_session_new (replace);
	if (session == NULL)
		return 1;

	if (initialize) {
		flashback_session_set_environment (session, "XDG_MENU_PREFIX", "gnome-flashback-");
		flashback_session_register_client (session);
	} else {
		application = flashback_application_new ();
		flashback_session_register_client (session);
		gtk_main ();
	}

	if (!initialize)
		g_object_unref (application);
	g_object_unref (session);

	return 0;
}
