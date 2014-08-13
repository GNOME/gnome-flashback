/*
 * Copyright (C) 2008 William Jon McCann <mccann@jhu.edu>
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <cairo-xlib.h>

#include "config.h"
#include "flashback-inhibit-dialog.h"

#ifndef DEFAULT_ICON_SIZE
#define DEFAULT_ICON_SIZE 32
#endif

#define DIALOG_RESPONSE_LOCK_SCREEN 1

struct _FlashbackInhibitDialogPrivate {
	GtkBuilder        *xml;
	int                action;
	int                timeout;
	guint              timeout_id;
	gboolean           is_done;
	const char *const *inhibitor_paths;
	GtkListStore      *list_store;
	int                xrender_event_base;
	int                xrender_error_base;
};

enum {
	PROP_0,
	PROP_ACTION,
	PROP_TIMEOUT,
	PROP_INHIBITOR_PATHS
};

enum {
	INHIBIT_IMAGE_COLUMN = 0,
	INHIBIT_NAME_COLUMN,
	INHIBIT_REASON_COLUMN,
	INHIBIT_ID_COLUMN,
	INHIBIT_PROXY_COLUMN,
	NUMBER_OF_COLUMNS
};

static void flashback_inhibit_dialog_class_init  (FlashbackInhibitDialogClass *klass);
static void flashback_inhibit_dialog_init        (FlashbackInhibitDialog      *inhibit_dialog);
static void flashback_inhibit_dialog_finalize    (GObject                     *object);

G_DEFINE_TYPE (FlashbackInhibitDialog, flashback_inhibit_dialog, GTK_TYPE_DIALOG)

static void
lock_screen (FlashbackInhibitDialog *dialog)
{
	GError *error;
	error = NULL;

	g_spawn_command_line_async ("gnome-screensaver-command --lock", &error);

	if (error != NULL) {
		g_warning ("Couldn't lock screen: %s", error->message);
		g_error_free (error);
	}
}

static void
on_response (FlashbackInhibitDialog *dialog,
             gint                    response_id)

{
	if (dialog->priv->is_done) {
		g_signal_stop_emission_by_name (dialog, "response");
		return;
	}

	switch (response_id) {
	case DIALOG_RESPONSE_LOCK_SCREEN:
		g_signal_stop_emission_by_name (dialog, "response");
		lock_screen (dialog);
		break;
	default:
		dialog->priv->is_done = TRUE;
		break;
	}
}

static void
flashback_inhibit_dialog_set_action (FlashbackInhibitDialog *dialog,
                                     int                     action)
{
	dialog->priv->action = action;
}

static void
flashback_inhibit_dialog_set_timeout (FlashbackInhibitDialog *dialog,
                                      int                     timeout)
{
	dialog->priv->timeout = timeout;
}

static void
flashback_inhibit_dialog_set_inhibitor_paths (FlashbackInhibitDialog  *dialog,
                                              const char *const       *paths)
{
	dialog->priv->inhibitor_paths = (const char *const*)g_strdupv ((gchar **)paths);
}

static gchar *
inhibitor_get_app_id (GDBusProxy *proxy)
{
	GError *error = NULL;
	GVariant *res;
	gchar *app_id;

	res = g_dbus_proxy_call_sync (proxy, "GetAppId", NULL, 0, G_MAXINT, NULL, &error);
	if (res == NULL) {
		g_warning ("Failed to get Inhibitor app id: %s", error->message);
		g_error_free (error);
		return NULL;
	}
	g_variant_get (res, "(s)", &app_id);
	g_variant_unref (res);

	return app_id;
}

static gchar *
inhibitor_get_reason (GDBusProxy *proxy)
{
	GError *error = NULL;
	GVariant *res;
	gchar *reason;

	res = g_dbus_proxy_call_sync (proxy, "GetReason", NULL, 0, G_MAXINT, NULL, &error);
	if (res == NULL) {
		g_warning ("Failed to get Inhibitor reason: %s", error->message);
		g_error_free (error);
		return NULL;
	}
	g_variant_get (res, "(s)", &reason);
	g_variant_unref (res);

	return reason;
}

static void
add_inhibitor (FlashbackInhibitDialog *dialog,
               GDBusProxy             *inhibitor)
{
        /*GdkDisplay     *gdkdisplay;
        const char     *name;
        char           *app_id;
        char           *desktop_filename;
        GdkPixbuf      *pixbuf;
        GDesktopAppInfo *app_info;
        char          **search_dirs;
        char           *freeme;
        gchar          *reason;
        GKeyFile *keyfile;
        GIcon *gicon;

        gdkdisplay = gtk_widget_get_display (GTK_WIDGET (dialog));

        app_info = NULL;
        name = NULL;
        pixbuf = NULL;
        freeme = NULL;

        app_id = inhibitor_get_app_id (inhibitor);
        reason = inhibitor_get_reason (inhibitor);

        if (IS_STRING_EMPTY (app_id)) {
                desktop_filename = NULL;
        } else if (! g_str_has_suffix (app_id, ".desktop")) {
                desktop_filename = g_strdup_printf ("%s.desktop", app_id);
        } else {
                desktop_filename = g_strdup (app_id);
        }

        if (desktop_filename != NULL) {
                search_dirs = gsm_util_get_desktop_dirs (TRUE, FALSE);

                if (g_path_is_absolute (desktop_filename)) {
                        char *basename;

                        app_info = g_desktop_app_info_new_from_filename (desktop_filename);
                        if (app_info == NULL) {
                                g_warning ("Unable to load desktop file '%s'",
                                            desktop_filename);

                                basename = g_path_get_basename (desktop_filename);
                                g_free (desktop_filename);
                                desktop_filename = basename;
                        }
                }

                if (app_info == NULL) {
                        keyfile = g_key_file_new ();
                        if (g_key_file_load_from_dirs (keyfile, desktop_filename, (const gchar **)search_dirs, NULL, 0, NULL))
                                app_info = g_desktop_app_info_new_from_keyfile (keyfile);
                        g_key_file_free (keyfile);
                }

                // look for a file with a vendor prefix
                if (app_info == NULL) {
                        g_warning ("Unable to find desktop file '%s'",
                                   desktop_filename);
                        g_free (desktop_filename);
                        desktop_filename = g_strdup_printf ("gnome-%s.desktop", app_id);
                        keyfile = g_key_file_new ();
                        if (g_key_file_load_from_dirs (keyfile, desktop_filename, (const gchar **)search_dirs, NULL, 0, NULL))
                                app_info = g_desktop_app_info_new_from_keyfile (keyfile);
                        g_key_file_free (keyfile);
                }
                g_strfreev (search_dirs);

                if (app_info == NULL) {
                        g_warning ("Unable to find desktop file '%s'",
                                   desktop_filename);
                } else {
                        name = g_app_info_get_name (G_APP_INFO (app_info));
                        gicon = g_app_info_get_icon (G_APP_INFO (app_info));

                        if (pixbuf == NULL) {
                                GtkIconInfo *info;
                                info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
                                                                       gicon,
                                                                       DEFAULT_ICON_SIZE,
                                                                       0);
                                pixbuf = gtk_icon_info_load_icon (info, NULL);
                                gtk_icon_info_free (info);
                        }
                }
        }

        if (name == NULL) {
                if (! IS_STRING_EMPTY (app_id)) {
                        name = app_id;
                } else {
                        name = _("Unknown");
                }
        }

        if (pixbuf == NULL) {
                GtkIconInfo *info;
                info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (),
                                                   GSM_ICON_INHIBITOR_DEFAULT,
                                                   DEFAULT_ICON_SIZE,
                                                   0);
                pixbuf = gtk_icon_info_load_icon (info, NULL);
                gtk_icon_info_free (info);
        }

        gtk_list_store_insert_with_values (dialog->priv->list_store,
                                           NULL, 0,
                                           INHIBIT_IMAGE_COLUMN, pixbuf,
                                           INHIBIT_NAME_COLUMN, name,
                                           INHIBIT_REASON_COLUMN, reason,
                                           INHIBIT_ID_COLUMN, g_dbus_proxy_get_object_path (inhibitor),
                                           INHIBIT_PROXY_COLUMN, inhibitor,
                                           -1);

        g_free (desktop_filename);
        g_free (freeme);
        g_clear_object (&pixbuf);
        g_clear_object (&app_info);

        g_free (app_id);
        g_free (reason);*/
}

static gboolean
model_is_empty (GtkTreeModel *model)
{
        gint n;

        n = gtk_tree_model_iter_n_children (model, NULL);
        g_print ("model rows: %d\n", n);
        return n == 0;
}

static void flashback_inhibit_dialog_start_timer (FlashbackInhibitDialog *dialog);
static void flashback_inhibit_dialog_stop_timer (FlashbackInhibitDialog *dialog);

static void
update_dialog_text (FlashbackInhibitDialog *dialog)
{
        const char *header_text;
        gchar *description_text;
        GtkWidget  *widget;
        gchar *title;
        const gchar *user;
        gchar *markup;
        gboolean inhibited;
        gint seconds;

        user = g_get_real_name ();
        inhibited = !model_is_empty (GTK_TREE_MODEL (dialog->priv->list_store));

        g_print ("update dialog text: inhibited %d\n", inhibited);

        if (inhibited) {
                flashback_inhibit_dialog_stop_timer (dialog);
        }
        else {
                flashback_inhibit_dialog_start_timer (dialog);
        }

        if (dialog->priv->timeout <= 30) {
                seconds = dialog->priv->timeout;
        } else {
                seconds = (dialog->priv->timeout / 10) * 10;
                if (dialog->priv->timeout % 10) {
                        seconds += 10;
                }
        }

        if (dialog->priv->action == FLASHBACK_LOGOUT_ACTION_LOGOUT) {
                title = g_strdup_printf (_("Log Out %s"), user);
                if (inhibited) {
                        header_text = _("Some applications are still running:");
                        description_text = g_strdup (_("Click Log Out to quit these applications and log out of the system."));
                } else {
                        header_text = NULL;
                        description_text = g_strdup_printf (ngettext ("%s will be logged out automatically in %d second.",
                                                                      "%s will be logged out automatically in %d seconds.", seconds), user, seconds);
                }
        } else if (dialog->priv->action == FLASHBACK_LOGOUT_ACTION_SHUTDOWN) {
                title = g_strdup_printf (_("Power Off"));
                if (inhibited) {
                        header_text = _("Some applications are still running:");
                        description_text = g_strdup (_("Click Power Off to quit these applications and power off the system."));
                } else {
                        header_text = NULL;
                        description_text = g_strdup_printf (ngettext ("The system will power off automatically in %d second.",
                                                                      "The system will power off automatically in %d seconds.", seconds), seconds);
                }
        } else if (dialog->priv->action == FLASHBACK_LOGOUT_ACTION_REBOOT) {
                title = g_strdup_printf (_("Restart"));
                if (inhibited) {
                        header_text = _("Some applications are still running:");
                        description_text = g_strdup (_("Click Restart to quit these applications and restart the system."));
                } else {
                        header_text = NULL;
                        description_text = g_strdup_printf (ngettext ("The system will restart automatically in %d second.",
                                                                      "The system will restart automatically in %d seconds.", seconds), seconds);
                }
        }
        else {
                title = g_strdup ("");
                if (inhibited) {
                        header_text = _("Some applications are still running:");
                        description_text = g_strdup (_("Waiting for these application to finish.  Interrupting them can lead to loss of data."));

                } else {
                        header_text = NULL;
                        description_text = g_strdup_printf (ngettext ("The action will proceed automatically in %d second.",
                                                                      "The action will proceed automatically in %d seconds.", seconds), seconds);
                }
        }

        gtk_window_set_title (GTK_WINDOW (dialog), title);

        widget = GTK_WIDGET (gtk_builder_get_object (dialog->priv->xml, "header-label"));
        if (header_text) {
                markup = g_strdup_printf ("<b>%s</b>", header_text);
                gtk_label_set_markup (GTK_LABEL (widget), markup);
                g_free (markup);
                gtk_widget_show (widget);
        } else {
                gtk_widget_hide (widget);
        }

        widget = GTK_WIDGET (gtk_builder_get_object (dialog->priv->xml, "scrolledwindow1"));
        if (inhibited) {
                gtk_widget_show (widget);
        } else {
                gtk_widget_hide (widget);
        }

        widget = GTK_WIDGET (gtk_builder_get_object (dialog->priv->xml, "description-label"));
        gtk_label_set_text (GTK_LABEL (widget), description_text);

        g_free (description_text);
        g_free (title);
}

static void
flashback_inhibit_dialog_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
	FlashbackInhibitDialog *dialog = FLASHBACK_INHIBIT_DIALOG (object);

	switch (prop_id) {
	case PROP_ACTION:
		flashback_inhibit_dialog_set_action (dialog, g_value_get_int (value));
		break;
	case PROP_TIMEOUT:
		flashback_inhibit_dialog_set_timeout (dialog, g_value_get_int (value));
		break;
	case PROP_INHIBITOR_PATHS:
		flashback_inhibit_dialog_set_inhibitor_paths (dialog, g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
flashback_inhibit_dialog_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
	FlashbackInhibitDialog *dialog = FLASHBACK_INHIBIT_DIALOG (object);

	switch (prop_id) {
	case PROP_ACTION:
		g_value_set_int (value, dialog->priv->action);
		break;
	case PROP_TIMEOUT:
		g_value_set_int (value, dialog->priv->action);
		break;
	case PROP_INHIBITOR_PATHS:
		g_value_set_boxed (value, dialog->priv->inhibitor_paths);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
name_cell_data_func (GtkTreeViewColumn       *tree_column,
                     GtkCellRenderer         *cell,
                     GtkTreeModel            *model,
                     GtkTreeIter             *iter,
                     FlashbackInhibitDialog  *dialog)
{
	char *name;
	char *reason;
	char *markup;

	name = NULL;
	reason = NULL;
	gtk_tree_model_get (model,
	                    iter,
	                    INHIBIT_NAME_COLUMN, &name,
	                    INHIBIT_REASON_COLUMN, &reason,
	                    -1);

	markup = g_strdup_printf ("<b>%s</b>\n"
	                          "<span size=\"small\">%s</span>",
	                          name ? name : "(null)",
	                          reason ? reason : "(null)");

	g_free (name);
	g_free (reason);

	g_object_set (cell, "markup", markup, NULL);
	g_free (markup);
}

static void
on_inhibitor_created (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
	FlashbackInhibitDialog *dialog = user_data;
	GError *error = NULL;
	GDBusProxy *proxy;

	proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (proxy == NULL) {
		g_warning ("Failed to create Inhibitor proxy: %s", error->message);
		g_error_free (error);
		return;
	}

	add_inhibitor (dialog, proxy);

	g_object_unref (proxy);
}

static void
populate_model (FlashbackInhibitDialog *dialog)
{
	gint i;
	for (i = 0; dialog->priv->inhibitor_paths && dialog->priv->inhibitor_paths[i]; i++) {
		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
		                          0,
		                          NULL,
		                          "org.gnome.SessionManager",
		                          dialog->priv->inhibitor_paths[i],
		                          "org.gnome.SessionManager.Inhibitor",
		                          NULL,
		                          on_inhibitor_created,
		                          dialog);
	}
	update_dialog_text (dialog);
}

static void
setup_dialog (FlashbackInhibitDialog *dialog)
{
	const char        *button_text;
	GtkWidget         *treeview;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *renderer;

	g_print ("setting up dialog\n");
	switch (dialog->priv->action) {
	case FLASHBACK_LOGOUT_ACTION_LOGOUT:
		button_text = _("Log Out");
		break;
	case FLASHBACK_LOGOUT_ACTION_SHUTDOWN:
		button_text = _("Power Off");
		break;
	case FLASHBACK_LOGOUT_ACTION_REBOOT:
		button_text = _("Restart");
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Lock Screen"), DIALOG_RESPONSE_LOCK_SCREEN);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Cancel"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), button_text, GTK_RESPONSE_ACCEPT);
	g_signal_connect (dialog, "response", G_CALLBACK (on_response), dialog);

	dialog->priv->list_store = gtk_list_store_new (NUMBER_OF_COLUMNS,
	                                               GDK_TYPE_PIXBUF,
	                                               G_TYPE_STRING,
	                                               G_TYPE_STRING,
	                                               G_TYPE_STRING,
	                                               G_TYPE_OBJECT);
	g_print ("empty model: %d\n", gtk_tree_model_iter_n_children (GTK_TREE_MODEL (dialog->priv->list_store), NULL));

	treeview = GTK_WIDGET (gtk_builder_get_object (dialog->priv->xml, "inhibitors-treeview"));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (dialog->priv->list_store));

	/* IMAGE COLUMN */
	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	gtk_tree_view_column_set_attributes (column,
		                                 renderer,
		                                 "pixbuf", INHIBIT_IMAGE_COLUMN,
		                                 NULL);

	g_object_set (renderer, "xalign", 1.0, NULL);

	/* NAME COLUMN */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
	gtk_tree_view_column_set_cell_data_func (column,
		                                     renderer,
		                                     (GtkTreeCellDataFunc) name_cell_data_func,
		                                     dialog,
		                                     NULL);

	gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview), INHIBIT_REASON_COLUMN);

	populate_model (dialog);
}

static GObject *
flashback_inhibit_dialog_constructor (GType                  type,
                                      guint                  n_construct_properties,
                                      GObjectConstructParam *construct_properties)
{
	GObject *object;
	FlashbackInhibitDialog *dialog;

	object = G_OBJECT_CLASS (flashback_inhibit_dialog_parent_class)->constructor (type,
	                                                                              n_construct_properties,
	                                                                              construct_properties);
	dialog = FLASHBACK_INHIBIT_DIALOG (object);

	setup_dialog (dialog);

	return G_OBJECT (dialog);
}

static void
flashback_inhibit_dialog_dispose (GObject *object)
{
	FlashbackInhibitDialog *dialog = FLASHBACK_INHIBIT_DIALOG (object);

	flashback_inhibit_dialog_stop_timer (dialog);

	g_clear_object (&dialog->priv->list_store);
	g_clear_object (&dialog->priv->xml);

	G_OBJECT_CLASS (flashback_inhibit_dialog_parent_class)->dispose (object);
}

static gboolean
flashback_inhibit_dialog_timeout (FlashbackInhibitDialog *dialog)
{
	if (dialog->priv->timeout == 0) {
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
		return G_SOURCE_REMOVE;
	}

	update_dialog_text (dialog);

	dialog->priv->timeout--;

	return G_SOURCE_CONTINUE;
}

static void
flashback_inhibit_dialog_start_timer (FlashbackInhibitDialog *dialog)
{
	if (dialog->priv->timeout_id == 0) {
		dialog->priv->timeout_id = g_timeout_add (1000, (GSourceFunc)flashback_inhibit_dialog_timeout, dialog);
	}
}

static void
flashback_inhibit_dialog_stop_timer (FlashbackInhibitDialog *dialog)
{
	if (dialog->priv->timeout_id != 0) {
		g_source_remove (dialog->priv->timeout_id);
		dialog->priv->timeout_id = 0;
	}
}

static void
flashback_inhibit_dialog_show (GtkWidget *widget)
{
	FlashbackInhibitDialog *dialog = FLASHBACK_INHIBIT_DIALOG (widget);

	GTK_WIDGET_CLASS (flashback_inhibit_dialog_parent_class)->show (widget);

	update_dialog_text (dialog);
}

static void
flashback_inhibit_dialog_class_init (FlashbackInhibitDialogClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = flashback_inhibit_dialog_get_property;
	object_class->set_property = flashback_inhibit_dialog_set_property;
	object_class->constructor = flashback_inhibit_dialog_constructor;
	object_class->dispose = flashback_inhibit_dialog_dispose;
	object_class->finalize = flashback_inhibit_dialog_finalize;

	widget_class->show = flashback_inhibit_dialog_show;

	g_object_class_install_property (object_class,
	                                 PROP_ACTION,
	                                 g_param_spec_int ("action",
	                                                   "action",
	                                                   "action",
	                                                   -1,
	                                                   G_MAXINT,
	                                                   -1,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_TIMEOUT,
	                                 g_param_spec_int ("timeout",
	                                                   "timeout",
	                                                   "timeout",
	                                                   -1,
	                                                   G_MAXINT,
	                                                   -1,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
	                                 PROP_INHIBITOR_PATHS,
	                                 g_param_spec_boxed ("inhibitor-paths",
	                                                     "inhibitor-paths",
	                                                     "inhibitor-paths",
	                                                     G_TYPE_STRV,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (klass, sizeof (FlashbackInhibitDialogPrivate));
}

static void
flashback_inhibit_dialog_init (FlashbackInhibitDialog *dialog)
{
	GtkWidget *content_area;
	GtkWidget *widget;
	GError    *error;

	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (dialog, FLASHBACK_TYPE_INHIBIT_DIALOG, FlashbackInhibitDialogPrivate);

	dialog->priv->xml = gtk_builder_new ();
	gtk_builder_set_translation_domain (dialog->priv->xml, GETTEXT_PACKAGE);

	error = NULL;
	if (!gtk_builder_add_from_file (dialog->priv->xml,
	                                GTKBUILDER_DIR "/flashback-inhibit-dialog.ui",
	                                &error)) {
		if (error) {
			g_warning ("Could not load inhibitor UI file: %s", error->message);
			g_error_free (error);
		} else {
			g_warning ("Could not load inhibitor UI file.");
		}
	}

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	widget = GTK_WIDGET (gtk_builder_get_object (dialog->priv->xml, "main-box"));
	gtk_container_add (GTK_CONTAINER (content_area), widget);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "system-log-out");
	gtk_window_set_title (GTK_WINDOW (dialog), "");
	g_object_set (dialog, "resizable", FALSE, NULL);
}

static void
flashback_inhibit_dialog_finalize (GObject *object)
{
	G_OBJECT_CLASS (flashback_inhibit_dialog_parent_class)->finalize (object);
}

GtkWidget *
flashback_inhibit_dialog_new (int action,
                              int seconds,
                              const char *const *inhibitor_paths)
{
	GObject *object;

	object = g_object_new (FLASHBACK_TYPE_INHIBIT_DIALOG,
	                       "action", action,
	                       "timeout", seconds,
	                       "inhibitor-paths", inhibitor_paths,
	                       NULL);

	return GTK_WIDGET (object);
}
