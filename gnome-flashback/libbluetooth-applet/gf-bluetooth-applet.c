/*
 * Copyright (C) 2015 Alberts Muktupāvels
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

#include <bluetooth-client.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gf-bluetooth-applet.h"
#include "gf-sd-rfkill.h"

#define GSM_DBUS_NAME "org.gnome.SettingsDaemon.Rfkill"
#define GSM_DBUS_PATH "/org/gnome/SettingsDaemon/Rfkill"

struct _GfBluetoothApplet
{
  GObject          parent;

  gint             bus_name_id;

  GtkStatusIcon   *status_icon;
  GfSdRfkill      *rfkill;
  BluetoothClient *client;
  GtkTreeModel    *model;
};

G_DEFINE_TYPE (GfBluetoothApplet, gf_bluetooth_applet, G_TYPE_OBJECT)

static void
turn_off_cb (GtkMenuItem *item,
             gpointer     user_data)
{
  GfBluetoothApplet *applet;

  applet = GF_BLUETOOTH_APPLET (user_data);

  if (applet->rfkill == NULL)
    return;

  gf_sd_rfkill_set_bluetooth_airplane_mode (applet->rfkill, TRUE);
}

static void
send_files_cb (GtkMenuItem *item,
               gpointer     user_data)
{
  GAppInfo *app_info;
  GError *error;

  error = NULL;
  app_info = g_app_info_create_from_commandline ("bluetooth-sendto",
                                                 "Bluetooth Transfer",
                                                 G_APP_INFO_CREATE_NONE,
                                                 &error);

  if (error != NULL)
    {
      g_warning ("Failed to crete GAppInfo from commandline - %s",
                 error->message);
      g_error_free (error);
    }
  else
    {
      g_app_info_launch (app_info, NULL, NULL, &error);

      if (error != NULL)
        {
          g_warning ("Failed to start Bluetooth Transfer - %s", error->message);
          g_error_free (error);
        }
    }

  g_clear_object (&app_info);
}

static void
turn_on_cb (GtkMenuItem *item,
            gpointer     user_data)
{
  GfBluetoothApplet *applet;

  applet = GF_BLUETOOTH_APPLET (user_data);

  if (applet->rfkill == NULL)
    return;

  gf_sd_rfkill_set_bluetooth_airplane_mode (applet->rfkill, FALSE);
}

static void
settings_cb (GtkMenuItem *item,
             gpointer     user_data)
{
  GAppInfo *app_info;
  GError *error;

  error = NULL;
  app_info = g_app_info_create_from_commandline ("gnome-control-center bluetooth",
                                                 "Bluetooth Settings",
                                                 G_APP_INFO_CREATE_NONE,
                                                 &error);

  if (error != NULL)
    {
      g_warning ("Failed to crete GAppInfo from commandline - %s",
                 error->message);
      g_error_free (error);
    }
  else
    {
      g_app_info_launch (app_info, NULL, NULL, &error);

      if (error != NULL)
        {
          g_warning ("Failed to start Bluetooth Settings - %s", error->message);
          g_error_free (error);
        }
    }

  g_clear_object (&app_info);
}

static void
popup_menu_cb (GtkStatusIcon *status_icon,
               guint          button,
               guint          activate_time,
               gpointer       user_data)
{
  GfBluetoothApplet *applet;
  GtkWidget *menu;
  gboolean airplane_mode;
  GtkWidget *item;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  applet = GF_BLUETOOTH_APPLET (user_data);
  menu = gtk_menu_new ();

  airplane_mode = FALSE;
  if (applet->rfkill != NULL)
    airplane_mode = gf_sd_rfkill_get_bluetooth_airplane_mode (applet->rfkill);

  if (!airplane_mode)
    {
      item = gtk_menu_item_new_with_label (_("Turn Off"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      g_signal_connect (item, "activate", G_CALLBACK (turn_off_cb), applet);

      item = gtk_menu_item_new_with_label (_("Send Files"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      g_signal_connect (item, "activate", G_CALLBACK (send_files_cb), NULL);
    }
  else
    {
      item = gtk_menu_item_new_with_label (_("Turn On"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      g_signal_connect (item, "activate", G_CALLBACK (turn_on_cb), applet);
    }

  item = gtk_menu_item_new_with_label (_("Bluetooth Settings"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  g_signal_connect (item, "activate", G_CALLBACK (settings_cb), NULL);

  gtk_widget_show_all (menu);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                  gtk_status_icon_position_menu, status_icon,
                  button, activate_time);

  G_GNUC_END_IGNORE_DEPRECATIONS
}

static GtkTreeIter *
get_default_adapter (GfBluetoothApplet *applet)
{
  gboolean valid;
  GtkTreeIter iter;

  valid = gtk_tree_model_get_iter_first (applet->model, &iter);

  while (valid)
    {
      gboolean is_default;

      gtk_tree_model_get (applet->model, &iter,
                          BLUETOOTH_COLUMN_DEFAULT, &is_default,
                          -1);

      if (is_default)
        return gtk_tree_iter_copy (&iter);

      valid = gtk_tree_model_iter_next (applet->model , &iter);
    }

  return NULL;
}

static gint
get_n_connected_devices (GfBluetoothApplet *applet)
{
  GtkTreeIter *adapter;
  guint devices;
  gboolean valid;
  GtkTreeIter iter;

  adapter = get_default_adapter (applet);

  if (adapter == NULL)
    return -1;

  devices = 0;
  valid = gtk_tree_model_iter_children (applet->model, &iter, adapter);

  while (valid)
    {
      gboolean is_connected;

      gtk_tree_model_get (applet->model, &iter,
                          BLUETOOTH_COLUMN_CONNECTED, &is_connected,
                          -1);

      if (is_connected)
        devices++;

      valid = gtk_tree_model_iter_next (applet->model , &iter);
    }

  gtk_tree_iter_free (adapter);

  return devices;
}

static void
gf_bluetooth_applet_sync (GfBluetoothApplet *applet)
{
  gint devices;
  gboolean airplane_mode;
  const gchar *title;
  const gchar *icon_name;
  gchar *tooltip_text;

  devices = get_n_connected_devices (applet);

  if (devices == -1)
    {
      g_clear_object (&applet->status_icon);

      return;
    }

  airplane_mode = FALSE;
  if (applet->rfkill != NULL)
    airplane_mode = gf_sd_rfkill_get_bluetooth_airplane_mode (applet->rfkill);

  if (!airplane_mode)
    {
      title = _("Bluetooth active");
      icon_name = "bluetooth-active";
    }
  else
    {
      title = _("Bluetooth disabled");
      icon_name = "bluetooth-disabled";
    }

  if (devices > 0)
    {
      tooltip_text = g_strdup_printf (ngettext ("%d Connected Device",
                                                "%d Connected Devices",
                                                devices), devices);
    }
  else
    {
      tooltip_text = g_strdup (_("Not Connected"));
    }

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  if (applet->status_icon == NULL)
    {
      applet->status_icon = gtk_status_icon_new ();

      g_signal_connect (applet->status_icon, "popup-menu",
                        G_CALLBACK (popup_menu_cb), applet);
    }

  gtk_status_icon_set_title (applet->status_icon, title);
  gtk_status_icon_set_from_icon_name (applet->status_icon, icon_name);
  gtk_status_icon_set_tooltip_text (applet->status_icon, tooltip_text);
  gtk_status_icon_set_visible (applet->status_icon, TRUE);

  G_GNUC_END_IGNORE_DEPRECATIONS

  g_free (tooltip_text);
}

static void
properties_changed_cb (GDBusProxy *proxy,
                       GVariant   *changed_properties,
                       GStrv       invalidated_properties,
                       gpointer    user_data)
{
  GfBluetoothApplet *applet;

  applet = GF_BLUETOOTH_APPLET (user_data);

  gf_bluetooth_applet_sync (applet);
}

static void
rfkill_proxy_ready_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GfBluetoothApplet *applet;
  GError *error;

  applet = GF_BLUETOOTH_APPLET (user_data);

  error = NULL;
  applet->rfkill = gf_sd_rfkill_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      g_warning ("Failed to get Rfkill proxy - %s", error->message);
      g_error_free (error);

      return;
    }

  g_signal_connect (applet->rfkill, "g-properties-changed",
                    G_CALLBACK (properties_changed_cb), applet);

  gf_bluetooth_applet_sync (applet);
}

static void
row_changed_cb (GtkTreeModel *tree_model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      user_data)
{
  GfBluetoothApplet *applet;

  applet = GF_BLUETOOTH_APPLET (user_data);

  gf_bluetooth_applet_sync (applet);
}

static void
row_deleted_cb (GtkTreeModel *tree_model,
                GtkTreePath  *path,
                gpointer      user_data)
{
  GfBluetoothApplet *applet;

  applet = GF_BLUETOOTH_APPLET (user_data);

  gf_bluetooth_applet_sync (applet);
}

static void
row_inserted_cb (GtkTreeModel *tree_model,
                 GtkTreePath  *path,
                 GtkTreeIter  *iter,
                 gpointer      user_data)
{
  GfBluetoothApplet *applet;

  applet = GF_BLUETOOTH_APPLET (user_data);

  gf_bluetooth_applet_sync (applet);
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  gf_sd_rfkill_proxy_new_for_bus (G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE,
                                  GSM_DBUS_NAME, GSM_DBUS_PATH, NULL,
                                  rfkill_proxy_ready_cb, user_data);
}

static void
name_vanished_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  GfBluetoothApplet *applet;

  applet = GF_BLUETOOTH_APPLET (user_data);

  g_clear_object (&applet->rfkill);

  gf_bluetooth_applet_sync (applet);
}

static void
gf_bluetooth_applet_dispose (GObject *object)
{
  GfBluetoothApplet *applet;

  applet = GF_BLUETOOTH_APPLET (object);

  if (applet->bus_name_id)
    {
      g_bus_unwatch_name (applet->bus_name_id);
      applet->bus_name_id = 0;
    }

  g_clear_object (&applet->status_icon);
  g_clear_object (&applet->rfkill);
  g_clear_object (&applet->client);
  g_clear_object (&applet->model);

  G_OBJECT_CLASS (gf_bluetooth_applet_parent_class)->dispose (object);
}

static void
gf_bluetooth_applet_class_init (GfBluetoothAppletClass *applet_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (applet_class);

  object_class->dispose = gf_bluetooth_applet_dispose;
}

static void
gf_bluetooth_applet_init (GfBluetoothApplet *applet)
{
  applet->bus_name_id = g_bus_watch_name (G_BUS_TYPE_SESSION, GSM_DBUS_NAME,
                                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                                          name_appeared_handler,
                                          name_vanished_handler,
                                          applet, NULL);

  applet->client = bluetooth_client_new ();
  applet->model = bluetooth_client_get_model (applet->client);

  g_signal_connect (applet->model, "row-changed",
                    G_CALLBACK (row_changed_cb), applet);
  g_signal_connect (applet->model, "row-deleted",
                    G_CALLBACK (row_deleted_cb), applet);
  g_signal_connect (applet->model, "row-inserted",
                    G_CALLBACK (row_inserted_cb), applet);

  gf_bluetooth_applet_sync (applet);
}

GfBluetoothApplet *
gf_bluetooth_applet_new (void)
{
  return g_object_new (GF_TYPE_BLUETOOTH_APPLET, NULL);
}
