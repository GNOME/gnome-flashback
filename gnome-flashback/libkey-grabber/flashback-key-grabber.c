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

#include <config.h>
#include <gtk/gtk.h>

#include "flashback-key-grabber.h"
#include "flashback-key-bindings.h"
#include "dbus-key-grabber.h"

#define KEY_GRABBER_DBUS_NAME "org.gnome.Shell"
#define KEY_GRABBER_DBUS_PATH "/org/gnome/Shell"

struct _FlashbackKeyGrabberPrivate {
	gint                    bus_name;
	GDBusInterfaceSkeleton *iface;

	GHashTable             *grabbed_accelerators;
	GHashTable             *grabbers;

	FlashbackKeyBindings   *bindings;
};

G_DEFINE_TYPE_WITH_PRIVATE (FlashbackKeyGrabber, flashback_key_grabber, G_TYPE_OBJECT)

static void
binding_activated (FlashbackKeyBindings *bindings,
                   guint                 action,
                   guint                 device,
                   guint                 timestamp,
                   gpointer              user_data)
{
	FlashbackKeyGrabber *grabber;

	grabber = FLASHBACK_KEY_GRABBER (user_data);

	dbus_key_grabber_emit_accelerator_activated (DBUS_KEY_GRABBER (grabber->priv->iface),
	                                             action,
	                                             device,
	                                             timestamp);
}

static gint
real_grab (FlashbackKeyGrabber *grabber,
           const gchar         *accelerator)
{
	return flashback_key_bindings_grab (grabber->priv->bindings, accelerator);
}

static gboolean
real_ungrab (FlashbackKeyGrabber *grabber,
             gint                 action)
{
	return flashback_key_bindings_ungrab (grabber->priv->bindings, action);
}

typedef struct {
	const gchar *sender;
	FlashbackKeyGrabber *grabber;
} Data;

static void
ungrab_accelerator (gpointer key,
                    gpointer value,
                    gpointer user_data)
{
	guint action;
	gchar *sender;
	Data *data;

	action = GPOINTER_TO_UINT (key);
	sender = (gchar *) value;
	data = (Data *) user_data;

	if (g_str_equal (sender, data->sender)) {
		if (real_ungrab (data->grabber, action))
			g_hash_table_remove (data->grabber->priv->grabbed_accelerators, key);
	}
}

static void
name_vanished_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
	FlashbackKeyGrabber *grabber;
	guint id;
	Data *data;

	grabber = FLASHBACK_KEY_GRABBER (user_data);
	id = GPOINTER_TO_UINT (g_hash_table_lookup (grabber->priv->grabbers, name));
	data = g_new0 (Data, 1);

	data->sender = name;
	data->grabber = grabber;

	g_hash_table_foreach (grabber->priv->grabbed_accelerators, (GHFunc) ungrab_accelerator, data);
	g_free (data);

	g_bus_unwatch_name (id);
	g_hash_table_remove (grabber->priv->grabbers, name);
}

static guint
grab_accelerator (FlashbackKeyGrabber *grabber,
                  const gchar         *accelerator,
                  guint                flags,
                  const gchar         *sender)
{
	guint action;

	action = real_grab (grabber, accelerator);
	g_hash_table_insert (grabber->priv->grabbed_accelerators,
	                     GUINT_TO_POINTER (action),
	                     g_strdup (sender));

	if (g_hash_table_lookup (grabber->priv->grabbers, sender) == NULL) {
		guint id = g_bus_watch_name (G_BUS_TYPE_SESSION,
		                             sender,
		                             G_BUS_NAME_WATCHER_FLAGS_NONE,
		                             NULL,
		                             (GBusNameVanishedCallback) name_vanished_handler,
		                             grabber,
		                             NULL);
		g_hash_table_insert (grabber->priv->grabbers,
		                     g_strdup (sender),
		                     GUINT_TO_POINTER (id));
	}

	return action;
}

static gboolean
handle_grab_accelerator (DBusKeyGrabber        *object,
                         GDBusMethodInvocation *invocation,
                         const gchar           *accelerator,
                         guint                  flags,
                         FlashbackKeyGrabber   *grabber)
{
	const gchar *sender;
	guint action;

	sender = g_dbus_method_invocation_get_sender (invocation);
	action = grab_accelerator (grabber, accelerator, flags, sender);

	dbus_key_grabber_complete_grab_accelerator (object, invocation, action);

	return TRUE;
}

static gboolean
handle_grab_accelerators (DBusKeyGrabber        *object,
                          GDBusMethodInvocation *invocation,
                          GVariant              *accelerators,
                          FlashbackKeyGrabber   *grabber)
{
	GVariantBuilder builder;
	GVariantIter iter;
	GVariant *child;
	const gchar *sender;

	g_variant_builder_init (&builder, G_VARIANT_TYPE("au"));
	g_variant_iter_init (&iter, accelerators);

	sender = g_dbus_method_invocation_get_sender (invocation);

	while ((child = g_variant_iter_next_value (&iter))) {
		gchar *accelerator;
		guint flags;
		guint action;

		g_variant_get (child, "(su)", &accelerator, &flags);

		action = grab_accelerator (grabber, accelerator, flags, sender);
		g_variant_builder_add (&builder, "u", action);

		g_free (accelerator);
		g_variant_unref (child);
	}

	dbus_key_grabber_complete_grab_accelerators (object, invocation, g_variant_builder_end (&builder));

	return TRUE;
}

static gboolean
handle_ungrab_accelerator (DBusKeyGrabber        *object,
                           GDBusMethodInvocation *invocation,
                           guint                  action,
                           FlashbackKeyGrabber   *grabber)
{
	gchar *sender;
	gboolean ret;

	ret = FALSE;
	sender = (gchar *) g_hash_table_lookup (grabber->priv->grabbed_accelerators,
	                                        GUINT_TO_POINTER (action));

	if (g_str_equal (sender, g_dbus_method_invocation_get_sender (invocation))) {
		ret = real_ungrab (grabber, action);

		if (ret)
			g_hash_table_remove (grabber->priv->grabbed_accelerators, GUINT_TO_POINTER (action));
	}

	dbus_key_grabber_complete_ungrab_accelerator (object, invocation, ret);

	return TRUE;
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
	FlashbackKeyGrabber *grabber;
	DBusKeyGrabber *skeleton;
	GError *error;

	grabber = FLASHBACK_KEY_GRABBER (user_data);
	skeleton = dbus_key_grabber_skeleton_new ();

	grabber->priv->iface = G_DBUS_INTERFACE_SKELETON (skeleton);

	g_signal_connect (grabber->priv->iface, "handle-grab-accelerator",
	                  G_CALLBACK (handle_grab_accelerator), grabber);
	g_signal_connect (grabber->priv->iface, "handle-grab-accelerators",
	                  G_CALLBACK (handle_grab_accelerators), grabber);
	g_signal_connect (grabber->priv->iface, "handle-ungrab-accelerator",
	                  G_CALLBACK (handle_ungrab_accelerator), grabber);

	error = NULL;
	if (!g_dbus_interface_skeleton_export (grabber->priv->iface,
	                                       connection,
	                                       KEY_GRABBER_DBUS_PATH,
	                                       &error)) {
		g_warning ("Failed to export interface: %s", error->message);
		g_error_free (error);
		return;
	}
}

static void
flashback_key_grabber_finalize (GObject *object)
{
	FlashbackKeyGrabber *grabber;

	grabber = FLASHBACK_KEY_GRABBER (object);

	if (grabber->priv->bus_name) {
		g_bus_unown_name (grabber->priv->bus_name);
		grabber->priv->bus_name = 0;
	}

	if (grabber->priv->grabbed_accelerators) {
		g_hash_table_destroy (grabber->priv->grabbed_accelerators);
		grabber->priv->grabbed_accelerators = NULL;
	}

	if (grabber->priv->grabbers) {
		g_hash_table_destroy (grabber->priv->grabbers);
		grabber->priv->grabbers = NULL;
	}

	g_clear_object (&grabber->priv->bindings);

	G_OBJECT_CLASS (flashback_key_grabber_parent_class)->finalize (object);
}

static void
flashback_key_grabber_init (FlashbackKeyGrabber *grabber)
{
	grabber->priv = flashback_key_grabber_get_instance_private (grabber);

	grabber->priv->grabbed_accelerators = g_hash_table_new_full (NULL, NULL, NULL, g_free);
	grabber->priv->grabbers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	grabber->priv->bindings = flashback_key_bindings_new ();
	g_signal_connect (grabber->priv->bindings, "binding-activated",
	                  G_CALLBACK (binding_activated), grabber);

	grabber->priv->bus_name = g_bus_own_name (G_BUS_TYPE_SESSION,
	                                          KEY_GRABBER_DBUS_NAME,
	                                          G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
	                                          G_BUS_NAME_OWNER_FLAGS_REPLACE,
	                                          on_bus_acquired,
	                                          NULL,
	                                          NULL,
	                                          grabber,
	                                          NULL);
}

static void
flashback_key_grabber_class_init (FlashbackKeyGrabberClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = flashback_key_grabber_finalize;
}

FlashbackKeyGrabber *
flashback_key_grabber_new (void)
{
	return g_object_new (FLASHBACK_TYPE_KEY_GRABBER, NULL);
}
