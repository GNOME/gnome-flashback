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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>
#include "config.h"
#include "dbus-display-config.h"
#include "flashback-display-config.h"

struct _FlashbackDisplayConfigPrivate {
	gint bus_name;
};

G_DEFINE_TYPE (FlashbackDisplayConfig, flashback_display_config, G_TYPE_OBJECT);

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
}

static void
on_name_acquired (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const char      *name,
              gpointer         user_data)
{
}

static void
flashback_display_config_finalize (GObject *object)
{
	FlashbackDisplayConfig *config = FLASHBACK_DISPLAY_CONFIG (object);

	if (config->priv->bus_name) {
		g_bus_unown_name (config->priv->bus_name);
		config->priv->bus_name = 0;
	}

	G_OBJECT_CLASS (flashback_display_config_parent_class)->finalize (object);
}

static void
flashback_display_config_init (FlashbackDisplayConfig *config)
{
	config->priv = G_TYPE_INSTANCE_GET_PRIVATE (config,
	                                            FLASHBACK_TYPE_DISPLAY_CONFIG,
	                                            FlashbackDisplayConfigPrivate);

	config->priv->bus_name = g_bus_own_name (G_BUS_TYPE_SESSION,
	                                         "org.gnome.Mutter.DisplayConfig",
	                                         G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
	                                         G_BUS_NAME_OWNER_FLAGS_REPLACE,
	                                         on_bus_acquired,
	                                         on_name_acquired,
	                                         on_name_lost,
	                                         config,
	                                         NULL);
}

static void
flashback_display_config_class_init (FlashbackDisplayConfigClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = flashback_display_config_finalize;

	g_type_class_add_private (class, sizeof (FlashbackDisplayConfigPrivate));
}

FlashbackDisplayConfig *
flashback_display_config_new (void)
{
	return g_object_new (FLASHBACK_TYPE_DISPLAY_CONFIG,
	                     NULL);
}
