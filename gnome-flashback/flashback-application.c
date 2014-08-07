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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "flashback-application.h"

struct _FlashbackApplicationPrivate {
};

G_DEFINE_TYPE (FlashbackApplication, flashback_application, GTK_TYPE_APPLICATION);

static void
flashback_application_activate (GApplication *application)
{
}

static void
flashback_application_startup (GApplication *application)
{
	G_APPLICATION_CLASS (flashback_application_parent_class)->startup (application);
}

static void
flashback_application_shutdown (GApplication *application)
{
	G_APPLICATION_CLASS (flashback_application_parent_class)->shutdown (application);
}

static void
flashback_application_init (FlashbackApplication *application)
{
}

static void
flashback_application_class_init (FlashbackApplicationClass *class)
{
	GApplicationClass *application_class = G_APPLICATION_CLASS (class);

	application_class->startup  = flashback_application_startup;
	application_class->shutdown = flashback_application_shutdown;
	application_class->activate = flashback_application_activate;
}

FlashbackApplication *
flashback_application_new (void)
{
	return g_object_new (FLASHBACK_TYPE_APPLICATION,
	                     "application-id", "org.gnome.gnome-flashback",
	                     "flags", G_APPLICATION_FLAGS_NONE,
	                     "register-session", TRUE,
	                     NULL);
}
