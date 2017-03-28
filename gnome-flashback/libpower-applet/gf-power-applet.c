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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libupower-glib/upower.h>
#include <math.h>

#include "gf-power-applet.h"
#include "gf-upower-device.h"

#define UPOWER_DBUS_NAME "org.freedesktop.UPower"
#define UPOWER_DEVICE_DBUS_PATH "/org/freedesktop/UPower/devices/DisplayDevice"

struct _GfPowerApplet
{
  GObject         parent;

  gint            bus_name_id;

  GtkStatusIcon  *status_icon;
  GfUPowerDevice *device;
};

G_DEFINE_TYPE (GfPowerApplet, gf_power_applet, G_TYPE_OBJECT)

static void
statistics_cb (GtkMenuItem *item,
               gpointer     user_data)
{
  GAppInfo *app_info;
  GError *error;

  error = NULL;
  app_info = g_app_info_create_from_commandline ("gnome-power-statistics",
                                                 "Power Statistics",
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
          g_warning ("Failed to start Power Statistics - %s", error->message);
          g_error_free (error);
        }
    }

  g_clear_object (&app_info);
}

static void
settings_cb (GtkMenuItem *item,
             gpointer     user_data)
{
  GAppInfo *app_info;
  GError *error;

  error = NULL;
  app_info = g_app_info_create_from_commandline ("gnome-control-center power",
                                                 "Power Settings",
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
          g_warning ("Failed to start Power Settings - %s", error->message);
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
  const gchar *title;
  gchar *tooltip_text;
  gchar *label;
  GtkWidget *menu;
  GtkWidget *item;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  title = gtk_status_icon_get_title (status_icon);
  tooltip_text = gtk_status_icon_get_tooltip_text (status_icon);
  label = g_strdup_printf ("%s: %s", title, tooltip_text);
  g_free (tooltip_text);

  menu = gtk_menu_new ();

  item = gtk_menu_item_new_with_label (label);
  g_free (label);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  g_signal_connect (item, "activate", G_CALLBACK (statistics_cb), NULL);

  item = gtk_menu_item_new_with_label (_("Power Settings"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  g_signal_connect (item, "activate", G_CALLBACK (settings_cb), NULL);

  gtk_widget_show_all (menu);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                  gtk_status_icon_position_menu, status_icon,
                  button, activate_time);

  G_GNUC_END_IGNORE_DEPRECATIONS
}

static gchar *
get_icon_name (GfPowerApplet *applet)
{
  gchar *icon_name;
  gchar *symbolic;

  icon_name = g_strdup (gf_upower_device_get_icon_name (applet->device));

  if (icon_name == NULL || icon_name[0] == '\0')
    {
      g_free (icon_name);
      return g_strdup ("battery");
    }

  symbolic = g_strrstr (icon_name, "-symbolic");

  if (symbolic != NULL)
    g_strlcpy (symbolic, "", sizeof (symbolic));

  return icon_name;
}

static gchar *
get_tooltip_text (GfPowerApplet *applet)
{
  UpDeviceState state;
  gint64 seconds;
  gdouble time;
  gdouble minutes;
  gdouble hours;
  gdouble percentage;

  state = gf_upower_device_get_state (applet->device);

  if (state == UP_DEVICE_STATE_FULLY_CHARGED)
    return g_strdup (_("Fully Charged"));
  else if (state == UP_DEVICE_STATE_EMPTY)
    return g_strdup (_("Empty"));
  else if (state == UP_DEVICE_STATE_CHARGING)
    seconds = gf_upower_device_get_time_to_full (applet->device);
  else if (state == UP_DEVICE_STATE_DISCHARGING)
    seconds = gf_upower_device_get_time_to_empty (applet->device);
  else
    return g_strdup (_("Estimating..."));

  time = round (seconds / 60);

  if (time == 0)
    return g_strdup (_("Estimating..."));

  minutes = fmod (time, 60);
  hours = floor (time / 60);
  percentage = gf_upower_device_get_percentage (applet->device);

  if (state == UP_DEVICE_STATE_DISCHARGING)
    {
      /* Translators: this is <hours>:<minutes> Remaining (<percentage>) */
      return g_strdup_printf (_("%.0f:%02.0f Remaining (%.0f%%)"),
                              hours, minutes, percentage);
    }

  if (state == UP_DEVICE_STATE_CHARGING)
    {
      /* Translators: this is <hours>:<minutes> Until Full (<percentage>) */
      return g_strdup_printf (_("%.0f:%02.0f Until Full (%.0f%%)"),
                              hours, minutes, percentage);
    }

  return NULL;
}

static const gchar *
get_title (GfPowerApplet *applet)
{
  UpDeviceKind type;

  type = gf_upower_device_get_type_ (applet->device);

  if (type == UP_DEVICE_KIND_UPS)
    return _("UPS");

  return _("Battery");
}

static void
gf_power_applet_sync (GfPowerApplet *applet)
{
  gchar *icon_name;
  gchar *tooltip_text;
  const gchar *title;
  gboolean is_present;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  if (applet->status_icon == NULL)
    {
      applet->status_icon = gtk_status_icon_new ();

      g_signal_connect (applet->status_icon, "popup-menu",
                        G_CALLBACK (popup_menu_cb), applet);
    }

  icon_name = get_icon_name (applet);
  gtk_status_icon_set_from_icon_name (applet->status_icon, icon_name);
  g_free (icon_name);

  tooltip_text = get_tooltip_text (applet);
  gtk_status_icon_set_tooltip_text (applet->status_icon, tooltip_text);
  g_free (tooltip_text);

  title = get_title (applet);
  gtk_status_icon_set_title (applet->status_icon, title);

  is_present = gf_upower_device_get_is_present (applet->device);
  gtk_status_icon_set_visible (applet->status_icon, is_present);

  G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
properties_changed_cb (GDBusProxy *proxy,
                       GVariant   *changed_properties,
                       GStrv       invalidated_properties,
                       gpointer    user_data)
{
  GfPowerApplet *applet;

  applet = GF_POWER_APPLET (user_data);

  gf_power_applet_sync (applet);
}

static void
device_proxy_ready_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GfPowerApplet *applet;
  GError *error;

  applet = GF_POWER_APPLET (user_data);

  error = NULL;
  applet->device = gf_upower_device_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      g_warning ("Failed to get UPower device proxy - %s", error->message);
      g_error_free (error);

      return;
    }

  g_signal_connect (applet->device, "g-properties-changed",
                    G_CALLBACK (properties_changed_cb), applet);

  gf_power_applet_sync (applet);
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  gf_upower_device_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                      G_DBUS_PROXY_FLAGS_NONE,
                                      UPOWER_DBUS_NAME,
                                      UPOWER_DEVICE_DBUS_PATH,
                                      NULL,
                                      device_proxy_ready_cb,
                                      user_data);
}

static void
name_vanished_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  GfPowerApplet *applet;

  applet = GF_POWER_APPLET (user_data);

  g_clear_object (&applet->status_icon);
  g_clear_object (&applet->device);
}

static void
gf_power_applet_dispose (GObject *object)
{
  GfPowerApplet *applet;

  applet = GF_POWER_APPLET (object);

  if (applet->bus_name_id)
    {
      g_bus_unwatch_name (applet->bus_name_id);
      applet->bus_name_id = 0;
    }

  g_clear_object (&applet->status_icon);
  g_clear_object (&applet->device);

  G_OBJECT_CLASS (gf_power_applet_parent_class)->dispose (object);
}

static void
gf_power_applet_class_init (GfPowerAppletClass *applet_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (applet_class);

  object_class->dispose = gf_power_applet_dispose;
}

static void
gf_power_applet_init (GfPowerApplet *applet)
{
  applet->bus_name_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                          UPOWER_DBUS_NAME,
                                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                                          name_appeared_handler,
                                          name_vanished_handler,
                                          applet, NULL);
}

GfPowerApplet *
gf_power_applet_new (void)
{
  return g_object_new (GF_TYPE_POWER_APPLET, NULL);
}
