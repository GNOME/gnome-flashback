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
	gint                    bus_name;
	GDBusInterfaceSkeleton *iface;
};

G_DEFINE_TYPE (FlashbackDisplayConfig, flashback_display_config, G_TYPE_OBJECT);

static void
handle_get_resources (DBusDisplayConfig     *object,
                      GDBusMethodInvocation *invocation,
                      gpointer               user_data)
{
	g_warning ("GetResources is not implemented!");
}

static void
handle_apply_configuration (DBusDisplayConfig     *object,
                            GDBusMethodInvocation *invocation,
                            guint                  serial,
                            gboolean               persistent,
                            GVariant              *crtcs,
                            GVariant              *outputs,
                            gpointer               user_data)
{
	g_warning ("ApplyConfiguration is not implemented!");
}

static void
handle_change_backlight (DBusDisplayConfig     *object,
                         GDBusMethodInvocation *invocation,
                         guint                  serial,
                         guint                  output_index,
                         gint                   value,
                         gpointer               user_data)
{
	g_warning ("ChangeBacklight is not implemented!");
}

static void
handle_get_crtc_gamma (DBusDisplayConfig     *object,
                       GDBusMethodInvocation *invocation,
                       guint                  serial,
                       guint                  crtc_id,
                       gpointer               user_data)
{
	g_warning ("GetCrtcGamma is not implemented!");
}

static void
handle_set_crtc_gamma (DBusDisplayConfig     *object,
                       GDBusMethodInvocation *invocation,
                       guint                  serial,
                       guint                  crtc_id,
                       GVariant              *red_v,
                       GVariant              *green_v,
                       GVariant              *blue_v,
                       gpointer               user_data)
{
	g_warning ("SetCrtcGamma is not implemented!");
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
	FlashbackDisplayConfig *config = FLASHBACK_DISPLAY_CONFIG (user_data);
	GError *error = NULL;

	config->priv->iface = G_DBUS_INTERFACE_SKELETON (dbus_display_config_skeleton_new ());

	g_signal_connect (config->priv->iface, "handle-get-resources", G_CALLBACK (handle_get_resources), config);
	g_signal_connect (config->priv->iface, "handle-apply-configuration", G_CALLBACK (handle_apply_configuration), config);
	g_signal_connect (config->priv->iface, "handle-change-backlight", G_CALLBACK (handle_change_backlight), config);
	g_signal_connect (config->priv->iface, "handle-get-crtc-gamma", G_CALLBACK (handle_get_crtc_gamma), config);
	g_signal_connect (config->priv->iface, "handle-set-crtc-gamma", G_CALLBACK (handle_set_crtc_gamma), config);

	if (!g_dbus_interface_skeleton_export (config->priv->iface,
	                                       connection,
	                                       "/org/gnome/Mutter/DisplayConfig",
	                                       &error)) {
		g_warning ("Failed to export interface: %s", error->message);
		g_error_free (error);
		return;
	}
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

	if (config->priv->iface) {
		g_dbus_interface_skeleton_unexport (config->priv->iface);

		g_object_unref (config->priv->iface);
		config->priv->iface = NULL;
	}

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
