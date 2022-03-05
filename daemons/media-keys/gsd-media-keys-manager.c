/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001-2003 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2006-2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "gsd-media-keys-manager.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "dbus/gf-shell-gen.h"
#include "gsd-screenshot-utils.h"
#include "shortcuts-list.h"

#define SHELL_DBUS_NAME "org.gnome.Shell"
#define SHELL_DBUS_PATH "/org/gnome/Shell"

#define SHELL_GRABBER_CALL_TIMEOUT G_MAXINT
#define SHELL_GRABBER_RETRY_INTERVAL_MS 1000

#define GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE(o) (gsd_media_keys_manager_get_instance_private (o))

typedef struct {
        gint ref_count;

        MediaKeyType key_type;
        ShellActionMode modes;
        MetaKeyBindingFlags grab_flags;
        const char *settings_key;
        gboolean static_setting;
        GArray *accel_ids;
} MediaKey;

typedef struct {
        GsdMediaKeysManager *manager;
        GPtrArray *keys;

        /* NOTE: This is to implement a custom cancellation handling where
         *       we immediately emit an ungrab call if grabbing was cancelled.
         */
        gboolean cancelled;
} GrabUngrabData;

typedef struct
{
        GSettings       *settings;

        GPtrArray       *keys;

        /* Shell stuff */
        GfShellGen      *shell_proxy;
        GfShellGen      *key_grabber;
        GCancellable    *grab_cancellable;
        GHashTable      *keys_to_sync;
        guint            keys_sync_source_id;
        GrabUngrabData  *keys_sync_data;

        /* Screencast stuff */
        GDBusProxy      *screencast_proxy;
        guint            screencast_timeout_id;
        gboolean         screencast_recording;
        GCancellable    *screencast_cancellable;

        guint            start_idle_id;
} GsdMediaKeysManagerPrivate;

static void     gsd_media_keys_manager_finalize    (GObject                  *object);
static void     keys_sync_queue                    (GsdMediaKeysManager *manager,
                                                    gboolean             immediate,
                                                    gboolean             retry);
static void     keys_sync_continue                 (GsdMediaKeysManager *manager);

G_DEFINE_TYPE_WITH_PRIVATE (GsdMediaKeysManager, gsd_media_keys_manager, G_TYPE_OBJECT)

static void
media_key_unref (MediaKey *key)
{
        if (key == NULL)
                return;
        if (!g_atomic_int_dec_and_test (&key->ref_count))
                return;
        g_clear_pointer (&key->accel_ids, g_array_unref);
        g_free (key);
}

static MediaKey *
media_key_ref (MediaKey *key)
{
        g_atomic_int_inc (&key->ref_count);
        return key;
}

static MediaKey *
media_key_new (void)
{
        MediaKey *key = g_new0 (MediaKey, 1);

        key->accel_ids = g_array_new (FALSE, TRUE, sizeof(guint));

        return media_key_ref (key);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MediaKey, media_key_unref)

static void
grab_ungrab_data_free (GrabUngrabData *data)
{
        /* NOTE: The manager pointer is not owned and is invalid if the
         *       operation was cancelled.
         */

        if (!data->cancelled) {
                GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (data->manager);

                if (priv->keys_sync_data == data)
                        priv->keys_sync_data = NULL;
        }

        data->manager = NULL;
        g_clear_pointer (&data->keys, g_ptr_array_unref);
        g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GrabUngrabData, grab_ungrab_data_free)

static char *
get_key_string (MediaKey *key)
{
	if (key->settings_key != NULL)
		return g_strdup_printf ("settings:%s", key->settings_key);
	else
		g_assert_not_reached ();
}

static GStrv
get_bindings (GsdMediaKeysManager *manager,
	      MediaKey            *key)
{
	GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);
	GPtrArray *array;
	gchar *binding;

	if (key->settings_key != NULL) {
		g_autofree gchar *static_settings_key = NULL;
		g_autofree GStrv keys = NULL;
		g_autofree GStrv static_keys = NULL;
		gchar **item;

		if (!key->static_setting)
			return g_settings_get_strv (priv->settings, key->settings_key);

		static_settings_key = g_strconcat (key->settings_key, "-static", NULL);
		keys = g_settings_get_strv (priv->settings, key->settings_key);
		static_keys = g_settings_get_strv (priv->settings, static_settings_key);

		array = g_ptr_array_new ();
		/* Steals all strings from the settings */
		for (item = keys; *item; item++)
			g_ptr_array_add (array, *item);
		for (item = static_keys; *item; item++)
			g_ptr_array_add (array, *item);
		g_ptr_array_add (array, NULL);

		return (GStrv) g_ptr_array_free (array, FALSE);
	}

	else
		g_assert_not_reached ();

        array = g_ptr_array_new ();
        g_ptr_array_add (array, binding);
        g_ptr_array_add (array, NULL);

        return (GStrv) g_ptr_array_free (array, FALSE);
}

static void
ungrab_accelerators_complete (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
        g_autoptr(GrabUngrabData) data = user_data;
        gboolean success = FALSE;
        g_autoptr(GError) error = NULL;
        guint i;

        g_debug ("Ungrab call completed!");

        if (!gf_shell_gen_call_ungrab_accelerators_finish (GF_SHELL_GEN (object),
                                                           &success, result, &error)) {
                g_warning ("Failed to ungrab accelerators: %s", error->message);

                if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)) {
                        keys_sync_queue (data->manager, FALSE, TRUE);
                        return;
                }

                /* We are screwed at this point; we'll still keep going assuming that we don't
                 * have the bindings registered anymore.
                 * The only alternative would be to die and force cleanup of all registered
                 * grabs that way.
                 */
        } else if (!success) {
                g_warning ("Failed to ungrab some accelerators, they were probably not registered!");
        }

        /* Clear the accelerator IDs. */
        for (i = 0; i < data->keys->len; i++) {
                MediaKey *key;

                key = g_ptr_array_index (data->keys, i);

                /* Always clear, as it would just fail again the next time. */
                g_array_set_size (key->accel_ids, 0);
        }

        /* Nothing left to do if the operation was cancelled */
        if (data->cancelled)
                return;

        keys_sync_continue (data->manager);
}

static void
grab_accelerators_complete (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
        g_autoptr(GrabUngrabData) data = user_data;
        g_autoptr(GVariant) actions = NULL;
        g_autoptr(GError) error = NULL;
        guint i;

        g_debug ("Grab call completed!");

        if (!gf_shell_gen_call_grab_accelerators_finish (GF_SHELL_GEN (object),
                                                         &actions, result, &error)) {
                g_warning ("Failed to grab accelerators: %s", error->message);

                if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)) {
                        keys_sync_queue (data->manager, FALSE, TRUE);
                        return;
                }

                /* We are screwed at this point as we can't grab the keys. Most likely
                 * this means we are not running on GNOME, or ran into some other weird
                 * error.
                 * Either way, finish the operation as there is no way we can recover
                 * from this.
                 */
                keys_sync_continue (data->manager);
                return;
        }

        /* Do an immediate ungrab if the operation was cancelled.
         * This may happen on daemon shutdown for example. */
        if (data->cancelled) {
                g_debug ("Doing an immediate ungrab on the grabbed accelerators!");

                gf_shell_gen_call_ungrab_accelerators (GF_SHELL_GEN (object),
                                                       actions,
                                                       NULL,
                                                       ungrab_accelerators_complete,
                                                       g_steal_pointer (&data));

                return;
        }

        /* We need to stow away the accel_ids that have been registered successfully. */
        for (i = 0; i < data->keys->len; i++) {
                MediaKey *key;

                key = g_ptr_array_index (data->keys, i);
                g_assert (key->accel_ids->len == 0);
        }
        for (i = 0; i < data->keys->len; i++) {
                MediaKey *key;
                guint accel_id;

                key = g_ptr_array_index (data->keys, i);

                g_variant_get_child (actions, i, "u", &accel_id);
                if (accel_id == 0) {
                        g_autofree gchar *tmp = NULL;
                        tmp = get_key_string (key);
                        g_warning ("Failed to grab accelerator for keybinding %s", tmp);
                } else {
                        g_array_append_val (key->accel_ids, accel_id);
                }
        }

        keys_sync_continue (data->manager);
}

static void
keys_sync_continue (GsdMediaKeysManager *manager)
{
	GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);
        g_auto(GVariantBuilder) ungrab_builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("au"));
        g_auto(GVariantBuilder) grab_builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(suu)"));
        g_autoptr(GPtrArray) keys_being_ungrabbed = NULL;
        g_autoptr(GPtrArray) keys_being_grabbed = NULL;
        g_autoptr(GrabUngrabData) data = NULL;
        GHashTableIter iter;
        MediaKey *key;
        gboolean need_ungrab = FALSE;

        /* Syncing keys is a two step process in principle, i.e. we first ungrab all keys
         * and then grab the new ones.
         * To make this work, this function will be called multiple times and it will
         * either emit an ungrab or grab call or do nothing when done.
         */

        /* If the keys_to_sync hash table is empty at this point, then we are done.
         * priv->keys_sync_data will be cleared automatically when it is unref'ed.
         */
        if (g_hash_table_size (priv->keys_to_sync) == 0)
                return;

        keys_being_ungrabbed = g_ptr_array_new_with_free_func ((GDestroyNotify) media_key_unref);
        keys_being_grabbed = g_ptr_array_new_with_free_func ((GDestroyNotify) media_key_unref);

        g_hash_table_iter_init (&iter, priv->keys_to_sync);
        while (g_hash_table_iter_next (&iter, (gpointer*) &key, NULL)) {
                g_auto(GStrv) bindings = NULL;
                gchar **pos = NULL;
                guint i;

                for (i = 0; i < key->accel_ids->len; i++) {
                        g_variant_builder_add (&ungrab_builder, "u", g_array_index (key->accel_ids, guint, i));
                        g_ptr_array_add (keys_being_ungrabbed, media_key_ref (key));

                        need_ungrab = TRUE;
                }

                /* Keys that are synced but aren't in the internal list are being removed. */
                if (!g_ptr_array_find (priv->keys, key, NULL))
                        continue;

                bindings = get_bindings (manager, key);
                pos = bindings;
                while (*pos) {
                        /* Do not try to register empty keybindings. */
                        if (strlen (*pos) > 0) {
                                g_variant_builder_add (&grab_builder, "(suu)", *pos, key->modes, key->grab_flags);
                                g_ptr_array_add (keys_being_grabbed, media_key_ref (key));
                        }
                        pos++;
                }
        }

        data = g_new0 (GrabUngrabData, 1);
        data->manager = manager;

        /* These calls intentionally do not get a cancellable. See comment in
         * GrabUngrabData.
         */
        priv->keys_sync_data = data;

        if (need_ungrab) {
                data->keys = g_steal_pointer (&keys_being_ungrabbed);

                gf_shell_gen_call_ungrab_accelerators (priv->key_grabber,
                                                       g_variant_builder_end (&ungrab_builder),
                                                       NULL,
                                                       ungrab_accelerators_complete,
                                                       g_steal_pointer (&data));
        } else {
                data->keys = g_steal_pointer (&keys_being_grabbed);

                g_hash_table_remove_all (priv->keys_to_sync);

                gf_shell_gen_call_grab_accelerators (priv->key_grabber,
                                                     g_variant_builder_end (&grab_builder),
                                                     NULL,
                                                     grab_accelerators_complete,
                                                     g_steal_pointer (&data));
        }
}

static gboolean
keys_sync_start (gpointer user_data)
{
        GsdMediaKeysManager *manager = GSD_MEDIA_KEYS_MANAGER (user_data);
        GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);

        priv->keys_sync_source_id = 0;
        g_assert (priv->keys_sync_data == NULL);
        keys_sync_continue (manager);

        return G_SOURCE_REMOVE;
}

static void
keys_sync_queue (GsdMediaKeysManager *manager, gboolean immediate, gboolean retry)
{
        GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);
        guint i;

        if (priv->keys_sync_source_id)
                g_source_remove (priv->keys_sync_source_id);

        if (retry) {
                /* Abort the currently running operation, and don't retry
                 * immediately to avoid race condition if an operation was
                 * already active. */
                if (priv->keys_sync_data) {
                        priv->keys_sync_data->cancelled = TRUE;
                        priv->keys_sync_data = NULL;

                        immediate = FALSE;
                }

                /* Mark all existing keys for sync. */
                for (i = 0; i < priv->keys->len; i++) {
                        MediaKey *key = g_ptr_array_index (priv->keys, i);
                        g_hash_table_add (priv->keys_to_sync, media_key_ref (key));
                }
        } else if (priv->keys_sync_data) {
                /* We are already actively syncing, no need to do anything. */
                return;
        }

        priv->keys_sync_source_id =
                g_timeout_add (immediate ? 0 : (retry ? SHELL_GRABBER_RETRY_INTERVAL_MS : 50),
                               keys_sync_start,
                               manager);
}

static void
gsettings_changed_cb (GSettings           *settings,
                      const gchar         *settings_key,
                      GsdMediaKeysManager *manager)
{
	GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);
        guint i;

        /* Give up if we don't have proxy to the shell */
        if (!priv->key_grabber)
                return;

	/* not needed here */
        if (g_str_equal (settings_key, "max-screencast-length"))
		return;

        /* Find the key that was modified */
        if (priv->keys == NULL)
                return;

        for (i = 0; i < priv->keys->len; i++) {
                MediaKey *key;

                key = g_ptr_array_index (priv->keys, i);

                /* Skip over hard-coded and GConf keys */
                if (key->settings_key == NULL)
                        continue;
                if (strcmp (settings_key, key->settings_key) == 0) {
                        g_hash_table_add (priv->keys_to_sync, media_key_ref (key));
                        keys_sync_queue (manager, FALSE, FALSE);
                        break;
                }
        }
}

static void
add_key (GsdMediaKeysManager *manager, guint i)
{
	GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);
	MediaKey *key;

	key = media_key_new ();
	key->key_type = media_keys[i].key_type;
	key->settings_key = media_keys[i].settings_key;
	key->static_setting = media_keys[i].static_setting;
	key->modes = media_keys[i].modes;
	key->grab_flags = media_keys[i].grab_flags;

	g_ptr_array_add (priv->keys, key);
}

static void
init_kbd (GsdMediaKeysManager *manager)
{
        guint i;

        for (i = 0; i < G_N_ELEMENTS (media_keys); i++)
                add_key (manager, i);

        keys_sync_queue (manager, TRUE, TRUE);
}

static void
screencast_stop (GsdMediaKeysManager *manager)
{
        GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);

        if (priv->screencast_timeout_id > 0) {
                g_source_remove (priv->screencast_timeout_id);
                priv->screencast_timeout_id = 0;
        }

        g_dbus_proxy_call (priv->screencast_proxy,
                           "StopScreencast", NULL,
                           G_DBUS_CALL_FLAGS_NONE, -1,
                           priv->screencast_cancellable,
                           NULL, NULL);

        priv->screencast_recording = FALSE;
}

static gboolean
screencast_timeout (gpointer user_data)
{
        GsdMediaKeysManager *manager = user_data;
        screencast_stop (manager);
        return G_SOURCE_REMOVE;
}

static void
screencast_start (GsdMediaKeysManager *manager)
{
        GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);
        guint max_length;
        g_dbus_proxy_call (priv->screencast_proxy,
                           "Screencast",
                           g_variant_new_parsed ("(%s, @a{sv} {})",
                                                 /* Translators: this is a filename used for screencast
                                                  * recording, where "%d" and "%t" date and time, e.g.
                                                  * "Screencast from 07-17-2013 10:00:46 PM.webm" */
                                                 /* xgettext:no-c-format */
                                                 _("Screencast from %d %t.webm")),
                           G_DBUS_CALL_FLAGS_NONE, -1,
                           priv->screencast_cancellable,
                           NULL, NULL);

        max_length = g_settings_get_uint (priv->settings, "max-screencast-length");

        if (max_length > 0) {
                priv->screencast_timeout_id = g_timeout_add_seconds (max_length,
                                                                     screencast_timeout,
                                                                     manager);
                g_source_set_name_by_id (priv->screencast_timeout_id, "[gnome-settings-daemon] screencast_timeout");
        }
        priv->screencast_recording = TRUE;
}

static void
do_screencast_action (GsdMediaKeysManager *manager)
{
        GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);

        if (priv->screencast_proxy == NULL)
                return;

        if (!priv->screencast_recording)
                screencast_start (manager);
        else
                screencast_stop (manager);
}

static gboolean
do_action (GsdMediaKeysManager *manager,
           MediaKeyType         type)
{
        g_debug ("Launching action for key type '%d'", type);

        switch (type) {
        case SCREENSHOT_KEY:
        case SCREENSHOT_CLIP_KEY:
        case WINDOW_SCREENSHOT_KEY:
        case WINDOW_SCREENSHOT_CLIP_KEY:
        case AREA_SCREENSHOT_KEY:
        case AREA_SCREENSHOT_CLIP_KEY:
                gsd_screenshot_take (type);
                break;
        case SCREENCAST_KEY:
                do_screencast_action (manager);
                break;
        default:
                break;
        }

        return FALSE;
}

static void
on_accelerator_activated (GfShellGen          *grabber,
                          guint                accel_id,
                          GVariant            *parameters,
                          GsdMediaKeysManager *manager)
{
        GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);
        guint i;

        g_debug ("Received accel id %u", accel_id);

        for (i = 0; i < priv->keys->len; i++) {
                MediaKey *key;
                guint j;

                key = g_ptr_array_index (priv->keys, i);

                for (j = 0; j < key->accel_ids->len; j++) {
                        if (g_array_index (key->accel_ids, guint, j) == accel_id)
                                break;
                }
                if (j >= key->accel_ids->len)
                        continue;

                do_action (manager, key->key_type);
                return;
        }

        g_warning ("Could not find accelerator for accel id %u", accel_id);
}

static void
on_screencast_proxy_ready (GObject      *source,
                           GAsyncResult *result,
                           gpointer      data)
{
        GsdMediaKeysManager *manager = data;
        GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);
        GError *error = NULL;

        priv->screencast_proxy =
                g_dbus_proxy_new_for_bus_finish (result, &error);

        if (!priv->screencast_proxy) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to create proxy for screencast: %s", error->message);
                g_error_free (error);
        }
}

static void
on_key_grabber_ready (GObject      *source,
                      GAsyncResult *result,
                      gpointer      data)
{
        GsdMediaKeysManager *manager = data;
        GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);
        GError *error = NULL;

        priv->key_grabber = gf_shell_gen_proxy_new_for_bus_finish (result, &error);

        if (!priv->key_grabber) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to create proxy for key grabber: %s", error->message);
                g_error_free (error);
                return;
        }

        g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (priv->key_grabber),
                                          SHELL_GRABBER_CALL_TIMEOUT);

        g_signal_connect (priv->key_grabber, "accelerator-activated",
                          G_CALLBACK (on_accelerator_activated), manager);

        init_kbd (manager);
}

static void
shell_presence_changed (GsdMediaKeysManager *manager)
{
        GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);
        gchar *name_owner;

        name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (priv->shell_proxy));

        g_ptr_array_set_size (priv->keys, 0);
        g_clear_object (&priv->key_grabber);
        g_clear_object (&priv->screencast_proxy);

        if (name_owner) {
                gf_shell_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                                0,
                                                name_owner,
                                                SHELL_DBUS_PATH,
                                                priv->grab_cancellable,
                                                on_key_grabber_ready, manager);
                g_free (name_owner);
        }
}

static GfShellGen *
get_shell_proxy (void)
{
        GfShellGen *shell_proxy;
        GError *error;

        error =  NULL;
        shell_proxy = gf_shell_gen_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                           G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                           G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                           SHELL_DBUS_NAME,
                                                           SHELL_DBUS_PATH,
                                                           NULL,
                                                           &error);
        if (error) {
                g_warning ("Failed to connect to the shell: %s", error->message);
                g_error_free (error);
        }

        return shell_proxy;
}

static gboolean
start_media_keys_idle_cb (GsdMediaKeysManager *manager)
{
        GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);

        priv->keys = g_ptr_array_new_with_free_func ((GDestroyNotify) media_key_unref);
        priv->keys_to_sync = g_hash_table_new_full (g_direct_hash, g_direct_equal, (GDestroyNotify) media_key_unref, NULL);

        priv->settings = g_settings_new (SETTINGS_BINDING_DIR);
        g_signal_connect (G_OBJECT (priv->settings), "changed",
                          G_CALLBACK (gsettings_changed_cb), manager);

        priv->grab_cancellable = g_cancellable_new ();
        priv->screencast_cancellable = g_cancellable_new ();

        priv->shell_proxy = get_shell_proxy ();
        g_signal_connect_swapped (priv->shell_proxy, "notify::g-name-owner",
                                  G_CALLBACK (shell_presence_changed), manager);
        shell_presence_changed (manager);

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  0, NULL,
                                  SHELL_DBUS_NAME ".Screencast",
                                  SHELL_DBUS_PATH "/Screencast",
                                  SHELL_DBUS_NAME ".Screencast",
                                  priv->screencast_cancellable,
                                  on_screencast_proxy_ready, manager);

        priv->start_idle_id = 0;

        return FALSE;
}

gboolean
gsd_media_keys_manager_start (GsdMediaKeysManager *manager,
                              GError             **error)
{
        GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);

        priv->start_idle_id = g_idle_add ((GSourceFunc) start_media_keys_idle_cb, manager);
        g_source_set_name_by_id (priv->start_idle_id, "[gnome-settings-daemon] start_media_keys_idle_cb");

        return TRUE;
}

void
gsd_media_keys_manager_stop (GsdMediaKeysManager *manager)
{
        GsdMediaKeysManagerPrivate *priv = GSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);

        if (priv->start_idle_id != 0) {
                g_source_remove (priv->start_idle_id);
                priv->start_idle_id = 0;
        }

        g_clear_object (&priv->settings);
        g_clear_object (&priv->screencast_proxy);

        if (priv->keys_sync_data) {
                /* Cancel ongoing sync. */
                priv->keys_sync_data->cancelled = TRUE;
                priv->keys_sync_data = NULL;
        }
        if (priv->keys_sync_source_id)
                g_source_remove (priv->keys_sync_source_id);
        priv->keys_sync_source_id = 0;

        /* Remove all grabs; i.e.:
         *  - add all keys to the sync queue
         *  - remove all keys from the internal keys list
         *  - call the function to start a sync
         *  - "cancel" the sync operation as the manager will be gone
         */
        if (priv->keys != NULL) {
                while (priv->keys->len) {
                        MediaKey *key = g_ptr_array_index (priv->keys, 0);
                        g_hash_table_add (priv->keys_to_sync, media_key_ref (key));
                        g_ptr_array_remove_index_fast (priv->keys, 0);
                }

                keys_sync_start (manager);

                g_clear_pointer (&priv->keys, g_ptr_array_unref);
        }

        g_clear_pointer (&priv->keys_to_sync, g_hash_table_destroy);

        g_clear_object (&priv->key_grabber);

        if (priv->grab_cancellable != NULL) {
                g_cancellable_cancel (priv->grab_cancellable);
                g_clear_object (&priv->grab_cancellable);
        }

        if (priv->screencast_cancellable != NULL) {
                g_cancellable_cancel (priv->screencast_cancellable);
                g_clear_object (&priv->screencast_cancellable);
        }

        g_clear_object (&priv->shell_proxy);
}

static void
gsd_media_keys_manager_class_init (GsdMediaKeysManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gsd_media_keys_manager_finalize;
}

static void
gsd_media_keys_manager_init (GsdMediaKeysManager *manager)
{
}

static void
gsd_media_keys_manager_finalize (GObject *object)
{
        GsdMediaKeysManager *manager = GSD_MEDIA_KEYS_MANAGER (object);

        gsd_media_keys_manager_stop (manager);

        G_OBJECT_CLASS (gsd_media_keys_manager_parent_class)->finalize (object);
}

GsdMediaKeysManager *
gsd_media_keys_manager_new (void)
{
        return g_object_new (GSD_TYPE_MEDIA_KEYS_MANAGER, NULL);
}
