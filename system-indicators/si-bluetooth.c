/*
 * Copyright (C) 2019 Alberts Muktupāvels
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
#include "si-bluetooth.h"

#include <glib/gi18n-lib.h>

#include "dbus/gf-sd-rfkill-gen.h"
#include "si-desktop-menu-item.h"

typedef enum
{
  BLUETOOTH_TYPE_HEADSET = 1 << 5,
  BLUETOOTH_TYPE_HEADPHONES = 1 << 6,
  BLUETOOTH_TYPE_OTHER_AUDIO = 1 << 7,
  BLUETOOTH_TYPE_KEYBOARD = 1 << 8,
  BLUETOOTH_TYPE_MOUSE = 1 << 9,
  BLUETOOTH_TYPE_SPEAKERS = 1 << 20
} BluetoothType;

typedef struct _BluetoothClient BluetoothClient;
typedef struct _BluetoothDevice BluetoothDevice;

struct _SiBluetooth
{
  SiIndicator      parent;

  GtkWidget       *menu;

  BluetoothClient *client;
  GListModel      *devices;

  guint            bus_name_id;

  GCancellable    *cancellable;

  GfSdRfkillGen   *rfkill;
};

G_DEFINE_TYPE (SiBluetooth, si_bluetooth, SI_TYPE_INDICATOR)

extern BluetoothClient *
bluetooth_client_new (void);

extern GListStore *
bluetooth_client_get_devices (BluetoothClient *client);

extern void
bluetooth_client_connect_service (BluetoothClient     *client,
                                  const char          *path,
                                  gboolean             connect,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data);

extern gboolean
bluetooth_client_connect_service_finish (BluetoothClient *client,
                                         GAsyncResult    *res,
                                         GError         **error);

static gboolean
is_airplane_mode (SiBluetooth *self)
{
  gboolean airplane_mode;

  airplane_mode = FALSE;

  if (self->rfkill != NULL)
    airplane_mode = gf_sd_rfkill_gen_get_bluetooth_airplane_mode (self->rfkill);

  return airplane_mode;
}

static void
turn_on_cb (GtkMenuItem *item,
            SiBluetooth *self)
{
  if (self->rfkill == NULL)
    return;

  gf_sd_rfkill_gen_set_bluetooth_airplane_mode (self->rfkill, FALSE);
}

static void
turn_off_cb (GtkMenuItem *item,
             SiBluetooth *self)
{
  if (self->rfkill == NULL)
    return;

  gf_sd_rfkill_gen_set_bluetooth_airplane_mode (self->rfkill, TRUE);
}

static void
append_main_items (SiBluetooth *self)
{
  GtkWidget *item;

  if (!is_airplane_mode (self))
    {
      item = gtk_menu_item_new_with_label (_("Turn Off"));
      gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), item);
      gtk_widget_show (item);

      g_signal_connect (item, "activate", G_CALLBACK (turn_off_cb), self);

      item = si_desktop_menu_item_new (_("Send Files"),
                                       "bluetooth-sendto.desktop");

      gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), item);
      gtk_widget_show (item);
    }
  else
    {
      item = gtk_menu_item_new_with_label (_("Turn On"));
      gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), item);
      gtk_widget_show (item);

      g_signal_connect (item, "activate", G_CALLBACK (turn_on_cb), self);
    }
}

static void
connect_done_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GError *error;

  error = NULL;
  bluetooth_client_connect_service_finish ((BluetoothClient *) source_object,
                                           res,
                                           &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }
}

static void
disconnect_cb (GtkMenuItem *item,
               SiBluetooth *self)
{
  const char *path;

  path = g_object_get_data (G_OBJECT (item), "path");
  if (path == NULL)
    return;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();

  bluetooth_client_connect_service (self->client,
                                    path,
                                    FALSE,
                                    self->cancellable,
                                    connect_done_cb,
                                    self);
}

static void
connect_cb (GtkMenuItem *item,
            SiBluetooth *self)
{
  const char *path;

  path = g_object_get_data (G_OBJECT (item), "path");
  if (path == NULL)
    return;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();

  bluetooth_client_connect_service (self->client,
                                    path,
                                    TRUE,
                                    self->cancellable,
                                    connect_done_cb,
                                    self);
}

static void
append_devices (SiBluetooth *self)
{
  GtkWidget *separator;
  guint n_items;
  guint i;

  if (is_airplane_mode (self))
    return;

  separator = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), separator);
  gtk_widget_show (separator);

  n_items = g_list_model_get_n_items (self->devices);

  for (i = 0; i < n_items; i++)
    {
      BluetoothDevice *device;
      GDBusProxy *proxy;
      char *name;
      BluetoothType type;
      gboolean is_connected;
      GtkWidget *item;
      GtkWidget *menu;
      char *path;

      device = g_list_model_get_item (self->devices, i);

      g_object_get (device,
                    "proxy", &proxy,
                    "name", &name,
                    "type", &type,
                    "connected", &is_connected,
                    NULL);

      item = gtk_menu_item_new_with_label (name);
      g_free (name);

      gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), item);
      gtk_widget_show (item);

      menu = gtk_menu_new ();
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

      path = NULL;
      if (proxy != NULL)
        {
          path = g_strdup (g_dbus_proxy_get_object_path (proxy));
          g_object_unref (proxy);
        }

      if (is_connected)
        {
          item = gtk_menu_item_new_with_label (_("Disconnect"));
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);

          g_object_set_data_full (G_OBJECT (item), "path", path, g_free);

          g_signal_connect (item,
                            "activate",
                            G_CALLBACK (disconnect_cb),
                            self);
        }
      else
        {
          item = gtk_menu_item_new_with_label (_("Connect"));
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);

          g_object_set_data_full (G_OBJECT (item), "path", path, g_free);

          g_signal_connect (item,
                            "activate",
                            G_CALLBACK (connect_cb),
                            self);
        }

      switch (type)
        {
          case BLUETOOTH_TYPE_KEYBOARD:
            item = si_desktop_menu_item_new (_("Keyboard Settings"),
                                             "gnome-keyboard-panel.desktop");

            gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
            gtk_widget_show (item);
            break;

          case BLUETOOTH_TYPE_MOUSE:
            item = si_desktop_menu_item_new (_("Mouse & Touchpad Settings"),
                                             "gnome-mouse-panel.desktop");

            gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
            gtk_widget_show (item);
            break;

          case BLUETOOTH_TYPE_HEADSET:
          case BLUETOOTH_TYPE_HEADPHONES:
          case BLUETOOTH_TYPE_SPEAKERS:
          case BLUETOOTH_TYPE_OTHER_AUDIO:
            item = si_desktop_menu_item_new (_("Sound Settings"),
                                             "gnome-sound-panel.desktop");

            gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
            gtk_widget_show (item);
            break;

          default:
            break;
        }

      g_object_unref (device);
    }
}

static void
remove_item_cb (GtkWidget *widget,
                gpointer   data)
{
  gtk_widget_destroy (widget);
}

static void
update_indicator_menu (SiBluetooth *self)
{
  GtkWidget *separator;
  GtkWidget *item;

  gtk_container_foreach (GTK_CONTAINER (self->menu), remove_item_cb, NULL);

  append_main_items (self);
  append_devices (self);

  separator = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), separator);
  gtk_widget_show (separator);

  item = si_desktop_menu_item_new (_("Bluetooth Settings"),
                                   "gnome-bluetooth-panel.desktop");

  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), item);
  gtk_widget_show (item);
}

static void
update_indicator_icon (SiBluetooth *self)
{
  GpApplet *applet;
  gboolean symbolic;
  const char *icon_name;

  applet = si_indicator_get_applet (SI_INDICATOR (self));
  symbolic = gp_applet_get_prefer_symbolic_icons (applet);

  if (!is_airplane_mode (self))
    {
      icon_name = "bluetooth-active";
      if (symbolic)
        icon_name = "bluetooth-active-symbolic";
    }
  else
    {
      icon_name = "bluetooth-disabled";
      if (symbolic)
        icon_name = "bluetooth-disabled-symbolic";
    }

  si_indicator_set_icon_name (SI_INDICATOR (self), icon_name);
}

static void
get_n_devices (SiBluetooth *self,
               int         *n_devices,
               int         *n_connected_devices)
{
  guint n_items;
  guint i;

  *n_devices = 0;
  *n_connected_devices = 0;

  n_items = g_list_model_get_n_items (self->devices);

  for (i = 0; i < n_items; i++)
    {
      BluetoothDevice *device;
      gboolean is_connected;
      gboolean is_paired;
      gboolean is_trusted;

      device = g_list_model_get_item (self->devices, i);

      g_object_get (device,
                    "connected", &is_connected,
                    "paired", &is_paired,
                    "trusted", &is_trusted,
                    NULL);

      if (is_connected)
        (*n_connected_devices)++;

      if (is_paired || is_trusted)
        (*n_devices)++;

      g_object_unref (device);
    }
}

static void
update_indicator (SiBluetooth *self)
{
  GtkWidget *menu_item;
  int n_devices;
  int n_connected_devices;
  char *tooltip;

  menu_item = si_indicator_get_menu_item (SI_INDICATOR (self));

  get_n_devices (self, &n_devices, &n_connected_devices);

  if (n_devices == 0)
    {
      gtk_widget_hide (menu_item);
      return;
    }

  update_indicator_icon (self);
  update_indicator_menu (self);

  if (n_connected_devices > 0)
    {
      tooltip = g_strdup_printf (ngettext ("%d Connected Device",
                                           "%d Connected Devices",
                                           n_connected_devices),
                                 n_connected_devices);
    }
  else
    {
      tooltip = g_strdup (_("Not Connected"));
    }

  gtk_widget_set_tooltip_text (menu_item , tooltip);
  gtk_widget_show (menu_item);
  g_free (tooltip);
}

static void
items_changed_cb (GListModel  *model,
                  guint        position,
                  guint        removed,
                  guint        added,
                  SiBluetooth *self)
{
  update_indicator (self);
}

static void
prefer_symbolic_icons_cb (GObject     *object,
                          GParamSpec  *pspec,
                          SiBluetooth *self)
{
  update_indicator_icon (self);
}

static void
properties_changed_cb (GDBusProxy  *proxy,
                       GVariant    *changed_properties,
                       GStrv        invalidated_properties,
                       SiBluetooth *self)
{
  update_indicator (self);
}

static void
rfkill_proxy_ready_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GError *error;
  GfSdRfkillGen *rfkill;
  SiBluetooth *self;

  error = NULL;
  rfkill = gf_sd_rfkill_gen_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = SI_BLUETOOTH (user_data);
  self->rfkill = rfkill;

  g_signal_connect (self->rfkill,
                    "g-properties-changed",
                    G_CALLBACK (properties_changed_cb),
                    self);

  update_indicator (self);
}

static void
name_appeared_handler_cb (GDBusConnection *connection,
                          const char      *name,
                          const char      *name_owner,
                          gpointer         user_data)
{
  SiBluetooth *self;

  self = SI_BLUETOOTH (user_data);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();

  gf_sd_rfkill_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                      G_DBUS_PROXY_FLAGS_NONE,
                                      "org.gnome.SettingsDaemon.Rfkill",
                                      "/org/gnome/SettingsDaemon/Rfkill",
                                      self->cancellable,
                                      rfkill_proxy_ready_cb,
                                      self);
}

static void
name_vanished_handler_cb (GDBusConnection *connection,
                          const char      *name,
                          gpointer         user_data)
{
  SiBluetooth *self;

  self = SI_BLUETOOTH (user_data);

  g_clear_object (&self->rfkill);
  update_indicator (self);
}

static void
si_bluetooth_constructed (GObject *object)
{
  SiBluetooth *self;
  GtkWidget *menu_item;
  GpApplet *applet;

  self = SI_BLUETOOTH (object);

  G_OBJECT_CLASS (si_bluetooth_parent_class)->constructed (object);

  self->menu = gtk_menu_new ();

  menu_item = si_indicator_get_menu_item (SI_INDICATOR (self));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), self->menu);

  self->client = bluetooth_client_new ();
  self->devices = G_LIST_MODEL (bluetooth_client_get_devices (self->client));

  g_signal_connect (self->devices,
                    "items-changed",
                    G_CALLBACK (items_changed_cb),
                    self);

  applet = si_indicator_get_applet (SI_INDICATOR (self));

  g_signal_connect (applet,
                    "notify::prefer-symbolic-icons",
                    G_CALLBACK (prefer_symbolic_icons_cb),
                    self);

  update_indicator (self);
}

static void
si_bluetooth_dispose (GObject *object)
{
  SiBluetooth *self;

  self = SI_BLUETOOTH (object);

  if (self->bus_name_id)
    {
      g_bus_unwatch_name (self->bus_name_id);
      self->bus_name_id = 0;
    }

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->rfkill);

  g_clear_object (&self->client);
  g_clear_object (&self->devices);

  G_OBJECT_CLASS (si_bluetooth_parent_class)->dispose (object);
}

static void
si_bluetooth_class_init (SiBluetoothClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = si_bluetooth_constructed;
  object_class->dispose = si_bluetooth_dispose;
}

static void
si_bluetooth_init (SiBluetooth *self)
{
  self->bus_name_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                        "org.gnome.SettingsDaemon.Rfkill",
                                        G_BUS_NAME_WATCHER_FLAGS_NONE,
                                        name_appeared_handler_cb,
                                        name_vanished_handler_cb,
                                        self,
                                        NULL);
}

SiIndicator *
si_bluetooth_new (GpApplet *applet)
{
  return g_object_new (SI_TYPE_BLUETOOTH,
                       "applet", applet,
                       NULL);
}
