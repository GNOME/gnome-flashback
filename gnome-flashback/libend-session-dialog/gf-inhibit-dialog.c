/*
 * Copyright (C) 2008 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2015 Alberts Muktupāvels
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

#include "dbus/gf-screensaver-gen.h"
#include "gf-inhibit-dialog.h"

#define IS_STRING_EMPTY(string) ((string) == NULL || (string)[0] == '\0')
#define DEFAULT_ICON_SIZE 32

typedef struct _GfInhibitDialogPrivate GfInhibitDialogPrivate;
struct _GfInhibitDialogPrivate
{
  gint           action;
  gint           timeout;
  guint          timeout_id;
  gchar        **inhibitor_paths;
  GtkListStore  *list_store;

  GtkWidget     *main_box;
  GtkWidget     *inhibitors_treeview;
  GtkWidget     *description_label;
  GtkWidget     *lock_screen_button;
  GtkWidget     *cancel_button;
  GtkWidget     *accept_button;
};

enum
{
  PROP_0,

  PROP_ACTION,
  PROP_TIMEOUT,
  PROP_INHIBITOR_PATHS
};

enum
{
  SIGNAL_RESPONSE,
  SIGNAL_CLOSE,

  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

enum
{
  INHIBIT_IMAGE_COLUMN = 0,
  INHIBIT_NAME_COLUMN,
  INHIBIT_REASON_COLUMN,
  INHIBIT_ID_COLUMN,
  INHIBIT_PROXY_COLUMN,

  NUMBER_OF_COLUMNS
};

static void gf_inhibit_dialog_start_timer (GfInhibitDialog *dialog);
static void gf_inhibit_dialog_stop_timer  (GfInhibitDialog *dialog);
static void populate_model                (GfInhibitDialog *dialog);
static void update_dialog_text            (GfInhibitDialog *dialog);

G_DEFINE_TYPE_WITH_PRIVATE (GfInhibitDialog, gf_inhibit_dialog, GTK_TYPE_WINDOW)

static void
lock_cb (GObject      *object,
         GAsyncResult *res,
         gpointer      user_data)
{
  GError *error;

  error = NULL;
  gf_screensaver_gen_call_lock_finish (GF_SCREENSAVER_GEN (object),
                                       res,
                                       &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Couldn't lock screen: %s", error->message);

      g_error_free (error);
      return;
    }
}

static void
screensaver_ready_cb (GObject      *object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GError *error;
  GfScreensaverGen *proxy;

  error = NULL;
  proxy = gf_screensaver_gen_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Couldn't lock screen: %s", error->message);

      g_error_free (error);
      return;
    }

  gf_screensaver_gen_call_lock (proxy, NULL, lock_cb, NULL);
  g_object_unref (proxy);
}

static void
gf_inhibit_dialog_set_action (GfInhibitDialog *dialog,
                              gint             action)
{
  GfInhibitDialogPrivate *priv;

  priv = gf_inhibit_dialog_get_instance_private (dialog);

  priv->action = action;
}

static void
gf_inhibit_dialog_set_timeout (GfInhibitDialog *dialog,
                               gint             timeout)
{
  GfInhibitDialogPrivate *priv;

  priv = gf_inhibit_dialog_get_instance_private (dialog);

  priv->timeout = timeout;
}

static void
gf_inhibit_dialog_set_inhibitor_paths (GfInhibitDialog    *dialog,
                                       const gchar *const *paths)
{
  GfInhibitDialogPrivate *priv;
  gchar **old_inhibitors;

  priv = gf_inhibit_dialog_get_instance_private (dialog);

  old_inhibitors = priv->inhibitor_paths;
  priv->inhibitor_paths = g_strdupv ((gchar **) paths);
  g_strfreev (old_inhibitors);

  if (priv->list_store == NULL)
    return;

  gtk_list_store_clear (priv->list_store);

  if (paths == NULL || paths[0] == NULL)
    update_dialog_text (dialog);
  else
    populate_model (dialog);
}

static gchar *
inhibitor_get_app_id (GDBusProxy *proxy)
{
  GError *error;
  GVariant *res;
  gchar *app_id;

  error = NULL;
  res = g_dbus_proxy_call_sync (proxy, "GetAppId", NULL,
                                0, G_MAXINT, NULL, &error);

  if (error != NULL)
    {
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
  GError *error;
  GVariant *res;
  gchar *reason;

  error = NULL;
  res = g_dbus_proxy_call_sync (proxy, "GetReason", NULL,
                                0, G_MAXINT, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Failed to get Inhibitor app id: %s", error->message);
      g_error_free (error);

      return NULL;
    }

  g_variant_get (res, "(s)", &reason);
  g_variant_unref (res);

  return reason;
}

static gchar **
get_app_dirs (void)
{
  GPtrArray *dirs;
  const gchar * const *system_data_dirs;
  gchar *filename;
  gint i;

  dirs = g_ptr_array_new ();
  system_data_dirs = g_get_system_data_dirs ();

  filename = g_build_filename (g_get_user_data_dir (), "applications", NULL);
  g_ptr_array_add (dirs, filename);

  for (i = 0; system_data_dirs[i]; i++)
    {
      filename = g_build_filename (system_data_dirs[i], "applications", NULL);
      g_ptr_array_add (dirs, filename);
    }

  g_ptr_array_add (dirs, NULL);

  return (gchar **) g_ptr_array_free (dirs, FALSE);
}

static gchar **
get_autostart_dirs (void)
{
  GPtrArray *dirs;
  const gchar * const *system_config_dirs;
  const gchar * const *system_data_dirs;
  gchar *filename;
  gint i;

  dirs = g_ptr_array_new ();
  system_data_dirs = g_get_system_data_dirs ();
  system_config_dirs = g_get_system_config_dirs ();

  filename = g_build_filename (g_get_user_config_dir (), "autostart", NULL);
  g_ptr_array_add (dirs, filename);

  for (i = 0; system_data_dirs[i]; i++)
    {
      filename = g_build_filename (system_data_dirs[i], "gnome", "autostart", NULL);
      g_ptr_array_add (dirs, filename);
    }

  for (i = 0; system_config_dirs[i]; i++)
    {
      filename = g_build_filename (system_config_dirs[i], "autostart", NULL);
      g_ptr_array_add (dirs, filename);
    }

  g_ptr_array_add (dirs, NULL);

  return (gchar **) g_ptr_array_free (dirs, FALSE);
}

static gchar **
get_desktop_dirs (void)
{
  gchar **apps;
  gchar **autostart;
  gchar **result;
  gint size;
  gint i;

  apps = get_app_dirs ();
  autostart = get_autostart_dirs ();

  size = 0;
  for (i = 0; apps[i] != NULL; i++)
    size++;
  for (i = 0; autostart[i] != NULL; i++)
    size++;

  result = g_new (gchar *, size + 1);

  size = 0;
  for (i = 0; apps[i] != NULL; i++, size++)
    result[size] = apps[i];
  for (i = 0; autostart[i] != NULL; i++, size++)
    result[size] = autostart[i];
  result[size] = NULL;

  g_free (apps);
  g_free (autostart);

  return result;
}

static void
add_inhibitor (GfInhibitDialog *dialog,
               GDBusProxy      *inhibitor)
{
  GfInhibitDialogPrivate *priv;
  gchar *app_id;
  gchar *reason;
  gchar *filename;
  gchar *name;
  GdkPixbuf *pixbuf;

  priv = gf_inhibit_dialog_get_instance_private (dialog);
  app_id = inhibitor_get_app_id (inhibitor);
  reason = inhibitor_get_reason (inhibitor);
  filename = NULL;
  name = NULL;
  pixbuf = NULL;

  if (app_id == NULL)
    return;

  if (!IS_STRING_EMPTY (app_id))
    {
      if (!g_str_has_suffix (app_id, ".desktop"))
        filename = g_strdup_printf ("%s.desktop", app_id);
      else
        filename = g_strdup (app_id);
    }

  if (filename != NULL)
    {
      gchar **search_dirs;
      GDesktopAppInfo *app_info;
      GKeyFile *keyfile;

      search_dirs = get_desktop_dirs ();
      app_info = NULL;

      if (g_path_is_absolute (filename))
        {
          gchar *basename;

          app_info = g_desktop_app_info_new_from_filename (filename);

          if (app_info == NULL)
            {
              basename = g_path_get_basename (filename);
              g_free (filename);
              filename = basename;
            }
        }

      if (app_info == NULL)
        {
          keyfile = g_key_file_new ();

          if (g_key_file_load_from_dirs (keyfile, filename, (const gchar **) search_dirs, NULL, 0, NULL))
            app_info = g_desktop_app_info_new_from_keyfile (keyfile);

          g_key_file_free (keyfile);
        }

      if (app_info == NULL)
        {
          g_free (filename);
          filename = g_strdup_printf ("gnome-%s.desktop", app_id);
          keyfile = g_key_file_new ();

          if (g_key_file_load_from_dirs (keyfile, filename, (const gchar **) search_dirs, NULL, 0, NULL))
            app_info = g_desktop_app_info_new_from_keyfile (keyfile);

          g_key_file_free (keyfile);
        }

      g_strfreev (search_dirs);

      if (app_info != NULL)
        {
          GIcon *gicon;
          const gchar *tmp_name;

          tmp_name = g_app_info_get_name (G_APP_INFO (app_info));
          gicon = g_app_info_get_icon (G_APP_INFO (app_info));

          name = g_utf8_normalize (tmp_name, -1, G_NORMALIZE_ALL);

          if (pixbuf == NULL)
            {
              GtkIconInfo *info;

              info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
                                                     gicon, DEFAULT_ICON_SIZE, 0);
              pixbuf = gtk_icon_info_load_icon (info, NULL);
              g_object_unref (info);
            }
        }

      g_free (filename);
      g_clear_object (&app_info);
    }

  if (name == NULL)
    {
      if (!IS_STRING_EMPTY (app_id))
        name = g_strdup (app_id);
      else
        name = g_strdup (_("Unknown"));
    }

  g_free (app_id);

  if (pixbuf == NULL)
    pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                       "image-missing", DEFAULT_ICON_SIZE,
                                       0, NULL);

  gtk_list_store_insert_with_values (priv->list_store, NULL, 0,
                                     INHIBIT_IMAGE_COLUMN, pixbuf,
                                     INHIBIT_NAME_COLUMN, name,
                                     INHIBIT_REASON_COLUMN, reason,
                                     INHIBIT_ID_COLUMN, g_dbus_proxy_get_object_path (inhibitor),
                                     INHIBIT_PROXY_COLUMN, inhibitor,
                                     -1);

  g_clear_object (&pixbuf);
  g_free (name);
  g_free (reason);

  update_dialog_text (dialog);
}

static gboolean
model_is_empty (GtkTreeModel *model)
{
  return gtk_tree_model_iter_n_children (model, NULL) == 0;
}

static gchar *
get_user_name (void)
{
  gchar *name;

  name = g_locale_to_utf8 (g_get_real_name (), -1, NULL, NULL, NULL);

  if (IS_STRING_EMPTY (name) || g_strcmp0 (name, "Unknown") == 0)
    {
		  g_free (name);
		  name = g_locale_to_utf8 (g_get_user_name (), -1 , NULL, NULL, NULL);
    }

  if (!name)
    name = g_strdup (g_get_user_name ());

  return name;
}

static void
update_dialog_text (GfInhibitDialog *dialog)
{
  GfInhibitDialogPrivate *priv;
  gboolean inhibited;
  gint seconds;
  const gchar *title;
  gchar *description;

  priv = gf_inhibit_dialog_get_instance_private (dialog);
  inhibited = !model_is_empty (GTK_TREE_MODEL (priv->list_store));

  if (inhibited)
    gf_inhibit_dialog_stop_timer (dialog);
  else
    gf_inhibit_dialog_start_timer (dialog);

  if (priv->timeout <= 30)
    {
      seconds = priv->timeout;
    }
  else
    {
      seconds = (priv->timeout / 10) * 10;

      if (priv->timeout % 10)
        seconds += 10;
    }

  if (priv->action == GF_LOGOUT_ACTION_LOGOUT)
    {
      title = _("Log Out");

      if (inhibited)
        {
          description = g_strdup (_("Click Log Out to quit these applications and log out of the system."));
        }
      else
        {
          char *user_name;

          user_name = get_user_name ();

          description = g_strdup_printf (ngettext ("%s will be logged out automatically in %d second.",
                                                   "%s will be logged out automatically in %d seconds.",
                                                   seconds),
                                         user_name,
                                         seconds);

          g_free (user_name);
        }
    }
  else if (priv->action == GF_LOGOUT_ACTION_SHUTDOWN)
    {
      title = _("Power Off");

      if (inhibited)
        description = g_strdup (_("Click Power Off to quit these applications and power off the system."));
      else
        description = g_strdup_printf (ngettext ("The system will power off automatically in %d second.",
                                                 "The system will power off automatically in %d seconds.",
                                                 seconds),
                                       seconds);
    }
  else if (priv->action == GF_LOGOUT_ACTION_REBOOT)
    {
      title = _("Restart");

      if (inhibited)
        description = g_strdup (_("Click Restart to quit these applications and restart the system."));
      else
        description = g_strdup_printf (ngettext ("The system will restart automatically in %d second.",
                                                 "The system will restart automatically in %d seconds.",
                                                 seconds),
                                       seconds);
    }
  else if (priv->action == GF_LOGOUT_ACTION_HIBERNATE)
    {
      title = _("Hibernate");
      description = g_strdup_printf (ngettext ("The system will hibernate automatically in %d second.",
                                               "The system will hibernate automatically in %d seconds.",
                                               seconds),
                                     seconds);
    }
  else if (priv->action == GF_LOGOUT_ACTION_SUSPEND)
    {
      title = _("Suspend");
      description = g_strdup_printf (ngettext ("The system will suspend automatically in %d second.",
                                               "The system will suspend automatically in %d seconds.",
                                               seconds),
                                     seconds);
    }
  else if (priv->action == GF_LOGOUT_ACTION_HYBRID_SLEEP)
    {
      title = _("Hybrid Sleep");
      description = g_strdup_printf (ngettext ("The system will hybrid sleep automatically in %d second.",
                                               "The system will hybrid sleep automatically in %d seconds.",
                                               seconds),
                                     seconds);
    }
  else
    {
      g_assert_not_reached ();
    }

  gtk_window_set_title (GTK_WINDOW (dialog), title);
  gtk_label_set_text (GTK_LABEL (priv->description_label), description);
  g_free (description);

  if (inhibited)
    gtk_widget_show (priv->main_box);
  else
    gtk_widget_hide (priv->main_box);
}

static void
gf_inhibit_dialog_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GfInhibitDialog *dialog;

  dialog = GF_INHIBIT_DIALOG (object);

  switch (prop_id)
    {
      case PROP_ACTION:
        gf_inhibit_dialog_set_action (dialog, g_value_get_int (value));
        break;

      case PROP_TIMEOUT:
        gf_inhibit_dialog_set_timeout (dialog, g_value_get_int (value));
        break;

      case PROP_INHIBITOR_PATHS:
        gf_inhibit_dialog_set_inhibitor_paths (dialog, g_value_get_boxed (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gf_inhibit_dialog_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GfInhibitDialog *dialog;
  GfInhibitDialogPrivate *priv;

  dialog = GF_INHIBIT_DIALOG (object);
  priv = gf_inhibit_dialog_get_instance_private (dialog);

  switch (prop_id)
    {
      case PROP_ACTION:
        g_value_set_int (value, priv->action);
        break;

      case PROP_TIMEOUT:
        g_value_set_int (value, priv->action);
        break;

      case PROP_INHIBITOR_PATHS:
        g_value_set_boxed (value, priv->inhibitor_paths);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
name_cell_data_func (GtkTreeViewColumn *tree_column,
                     GtkCellRenderer   *cell,
                     GtkTreeModel      *model,
                     GtkTreeIter       *iter,
                     gpointer           user_data)
{
  gchar *name;
  gchar *reason;
  gchar *markup;

  name = NULL;
  reason = NULL;

  gtk_tree_model_get (model, iter,
                      INHIBIT_NAME_COLUMN, &name,
                      INHIBIT_REASON_COLUMN, &reason,
                      -1);

  markup = g_strdup_printf ("<b>%s</b>\n" "<span size=\"small\">%s</span>",
                            name ? name : "(null)", reason ? reason : "(null)");

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
  GfInhibitDialog *dialog;
  GError *error;
  GDBusProxy *proxy;

  dialog = GF_INHIBIT_DIALOG (user_data);

  error = NULL;
  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      g_warning ("Failed to create Inhibitor proxy: %s", error->message);
      g_error_free (error);

      return;
    }

  add_inhibitor (dialog, proxy);
  g_object_unref (proxy);
}

static void
populate_model (GfInhibitDialog *dialog)
{
  GfInhibitDialogPrivate *priv;
  gint i;

  priv = gf_inhibit_dialog_get_instance_private (dialog);

  if (priv->inhibitor_paths == NULL)
    return;

  for (i = 0; priv->inhibitor_paths[i]; i++)
    {
      g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION, 0, NULL,
                                "org.gnome.SessionManager",
                                priv->inhibitor_paths[i],
                                "org.gnome.SessionManager.Inhibitor",
                                NULL, on_inhibitor_created, dialog);
    }
}

static void
setup_dialog (GfInhibitDialog *dialog)
{
  GfInhibitDialogPrivate *priv;
  const gchar *button_text;
  GtkWidget *treeview;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  priv = gf_inhibit_dialog_get_instance_private (dialog);

  switch (priv->action)
    {
      case GF_LOGOUT_ACTION_LOGOUT:
        button_text = _("Log Out");
        break;

      case GF_LOGOUT_ACTION_SHUTDOWN:
        button_text = _("Power Off");
        break;

      case GF_LOGOUT_ACTION_REBOOT:
        button_text = _("Restart");
        break;

      case GF_LOGOUT_ACTION_HIBERNATE:
        button_text = _("Hibernate");
        break;

      case GF_LOGOUT_ACTION_SUSPEND:
        button_text = _("Suspend");
        break;

      case GF_LOGOUT_ACTION_HYBRID_SLEEP:
        button_text = _("Hybrid Sleep");
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  gtk_button_set_label (GTK_BUTTON (priv->accept_button), button_text);

  priv->list_store = gtk_list_store_new (NUMBER_OF_COLUMNS,
                                         GDK_TYPE_PIXBUF,
                                         G_TYPE_STRING,
                                         G_TYPE_STRING,
                                         G_TYPE_STRING,
                                         G_TYPE_OBJECT);

  treeview = priv->inhibitors_treeview;
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
                           GTK_TREE_MODEL (priv->list_store));

  /* IMAGE COLUMN */
  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  gtk_tree_view_column_set_attributes (column, renderer,
                                       "pixbuf", INHIBIT_IMAGE_COLUMN,
                                       NULL);

  g_object_set (renderer, "xalign", 1.0, NULL);

  /* NAME COLUMN */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
                                           name_cell_data_func,
                                           dialog, NULL);

  gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview),
                                    INHIBIT_REASON_COLUMN);

  populate_model (dialog);
}

static GObject *
gf_inhibit_dialog_constructor (GType                  type,
                               guint                  n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
  GObject *object;
  GfInhibitDialog *dialog;

  object = G_OBJECT_CLASS (gf_inhibit_dialog_parent_class)->constructor (type,
                                                                         n_construct_properties,
                                                                         construct_properties);
  dialog = GF_INHIBIT_DIALOG (object);

  setup_dialog (dialog);
  update_dialog_text (dialog);

  return object;
}

static void
gf_inhibit_dialog_dispose (GObject *object)
{
  GfInhibitDialog *dialog;
  GfInhibitDialogPrivate *priv;

  dialog = GF_INHIBIT_DIALOG (object);
  priv = gf_inhibit_dialog_get_instance_private (dialog);

  gf_inhibit_dialog_stop_timer (dialog);

  g_clear_object (&priv->list_store);

  G_OBJECT_CLASS (gf_inhibit_dialog_parent_class)->dispose (object);
}

static gboolean
gf_inhibit_dialog_timeout (gpointer user_data)
{
  GfInhibitDialog *dialog;
  GfInhibitDialogPrivate *priv;

  dialog = GF_INHIBIT_DIALOG (user_data);
  priv = gf_inhibit_dialog_get_instance_private (dialog);

  if (priv->timeout == 0)
    {
      gf_inhibit_dialog_response (dialog, GF_RESPONSE_ACCEPT);

      priv->timeout_id = 0;
      return G_SOURCE_REMOVE;
    }

  update_dialog_text (dialog);

  priv->timeout--;

  return G_SOURCE_CONTINUE;
}

static void
gf_inhibit_dialog_start_timer (GfInhibitDialog *dialog)
{
  GfInhibitDialogPrivate *priv;

  priv = gf_inhibit_dialog_get_instance_private (dialog);

  if (priv->timeout_id == 0)
    priv->timeout_id = g_timeout_add (1000, gf_inhibit_dialog_timeout, dialog);
}

static void
gf_inhibit_dialog_stop_timer (GfInhibitDialog *dialog)
{
  GfInhibitDialogPrivate *priv;

  priv = gf_inhibit_dialog_get_instance_private (dialog);

  if (priv->timeout_id != 0)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }
}

static void
gf_inhibit_dialog_class_init (GfInhibitDialogClass *dialog_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;
  const gchar *resource;

  object_class = G_OBJECT_CLASS (dialog_class);
  widget_class = GTK_WIDGET_CLASS (dialog_class);

  dialog_class->close = gf_inhibit_dialog_close;

  object_class->get_property = gf_inhibit_dialog_get_property;
  object_class->set_property = gf_inhibit_dialog_set_property;
  object_class->constructor = gf_inhibit_dialog_constructor;
  object_class->dispose = gf_inhibit_dialog_dispose;

  signals[SIGNAL_RESPONSE] =
    g_signal_new ("response",
                  G_OBJECT_CLASS_TYPE (dialog_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GfInhibitDialogClass, response),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE, 1, G_TYPE_INT);

  signals[SIGNAL_CLOSE] =
    g_signal_new ("close",
                  G_OBJECT_CLASS_TYPE (dialog_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GfInhibitDialogClass, close),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class, PROP_ACTION,
                                   g_param_spec_int ("action", "action", "action",
                                                     -1, G_MAXINT, -1,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_TIMEOUT,
                                   g_param_spec_int ("timeout", "timeout", "timeout",
                                                     -1, G_MAXINT, -1,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_INHIBITOR_PATHS,
                                   g_param_spec_boxed ("inhibitor-paths", "inhibitor-paths", "inhibitor-paths",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  binding_set = gtk_binding_set_by_class (dialog_class);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);

  resource = "/org/gnome/gnome-flashback/gf-inhibit-dialog.ui";
  gtk_widget_class_set_template_from_resource (widget_class, resource);

  gtk_widget_class_bind_template_child_private (widget_class, GfInhibitDialog, main_box);
  gtk_widget_class_bind_template_child_private (widget_class, GfInhibitDialog, inhibitors_treeview);
  gtk_widget_class_bind_template_child_private (widget_class, GfInhibitDialog, description_label);
  gtk_widget_class_bind_template_child_private (widget_class, GfInhibitDialog, lock_screen_button);
  gtk_widget_class_bind_template_child_private (widget_class, GfInhibitDialog, cancel_button);
  gtk_widget_class_bind_template_child_private (widget_class, GfInhibitDialog, accept_button);
}

static void
lock_screen_button_clicked (GtkButton       *button,
                            GfInhibitDialog *dialog)
{
  gf_screensaver_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        "org.gnome.ScreenSaver",
                                        "/org/gnome/ScreenSaver",
                                        NULL,
                                        screensaver_ready_cb,
                                        NULL);

  gf_inhibit_dialog_close (dialog);
}

static void
cancel_button_clicked (GtkButton       *button,
                       GfInhibitDialog *dialog)
{
  gf_inhibit_dialog_response (dialog, GF_RESPONSE_CANCEL);
}

static void
accept_button_clicked (GtkButton       *button,
                       GfInhibitDialog *dialog)
{
  gf_inhibit_dialog_response (dialog, GF_RESPONSE_ACCEPT);
}

static void
gf_inhibit_dialog_init (GfInhibitDialog *dialog)
{
  GfInhibitDialogPrivate *priv;
  GtkWindow *window;

  priv = gf_inhibit_dialog_get_instance_private (dialog);
  window = GTK_WINDOW (dialog);

  gtk_widget_init_template (GTK_WIDGET (dialog));

  g_signal_connect (priv->lock_screen_button, "clicked",
                    G_CALLBACK (lock_screen_button_clicked), dialog);
  g_signal_connect (priv->cancel_button, "clicked",
                    G_CALLBACK (cancel_button_clicked), dialog);
  g_signal_connect (priv->accept_button, "clicked",
                    G_CALLBACK (accept_button_clicked), dialog);

  gtk_window_set_icon_name (window, "system-log-out");
  gtk_window_set_keep_above (window, TRUE);
  gtk_window_set_skip_taskbar_hint (window, TRUE);
  gtk_window_set_skip_pager_hint (window, TRUE);
}

GtkWidget *
gf_inhibit_dialog_new (gint                action,
                       gint                seconds,
                       const gchar *const *inhibitor_paths)
{
  return g_object_new (GF_TYPE_INHIBIT_DIALOG,
                       "action", action,
                       "timeout", seconds,
                       "inhibitor-paths", inhibitor_paths,
                       NULL);
}

void
gf_inhibit_dialog_response (GfInhibitDialog *dialog,
                            gint             response_id)
{
  g_return_if_fail (GF_IS_INHIBIT_DIALOG (dialog));

  g_signal_emit (dialog, signals[SIGNAL_RESPONSE], 0, response_id);
}

void
gf_inhibit_dialog_close (GfInhibitDialog *dialog)
{
  g_return_if_fail (GF_IS_INHIBIT_DIALOG (dialog));

  gtk_window_close (GTK_WINDOW (dialog));
}

void
gf_inhibit_dialog_present (GfInhibitDialog *dialog,
                           guint32          timestamp)
{
  GtkWidget *widget;
  GdkWindow *window;
  guint32 server_time;

  g_return_if_fail (GF_IS_INHIBIT_DIALOG (dialog));

  if (timestamp != 0)
    {
      gtk_window_present_with_time (GTK_WINDOW (dialog), timestamp);

      return;
    }

  widget = GTK_WIDGET (dialog);

  gtk_widget_show (widget);

  window = gtk_widget_get_window (widget);
  server_time = GDK_CURRENT_TIME;

  if (window != NULL)
    server_time = gdk_x11_get_server_time (window);

  gtk_window_present_with_time (GTK_WINDOW (dialog), server_time);
}
