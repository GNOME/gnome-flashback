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
#include "si-power.h"

#include <glib/gi18n-lib.h>
#include <libupower-glib/upower.h>
#include <math.h>

#include "dbus/gf-upower-device-gen.h"
#include "si-desktop-menu-item.h"

#define SHOW_BATTERY_PERCENTAGE_KEY "show-battery-percentage"

struct _SiPower
{
  SiIndicator        parent;

  GSettings         *settings;
  gboolean           show_battery_percentage;

  GtkWidget         *menu;

  guint              bus_name_id;

  GCancellable      *cancellable;

  GfUPowerDeviceGen *device;
};

G_DEFINE_TYPE (SiPower, si_power, SI_TYPE_INDICATOR)

static void
remove_item_cb (GtkWidget *widget,
                gpointer   data)
{
  gtk_widget_destroy (widget);
}

static const char *
get_type_text (SiPower *self)
{
  UpDeviceKind type;

  type = gf_upower_device_gen_get_type_ (self->device);

  if (type == UP_DEVICE_KIND_UPS)
    return _("UPS");

  return _("Battery");
}

static char *
get_state_text (SiPower *self)
{
  UpDeviceState state;
  int64_t seconds;
  double time;
  double minutes;
  double hours;
  double percentage;

  state = gf_upower_device_gen_get_state (self->device);

  if (state == UP_DEVICE_STATE_FULLY_CHARGED)
    return g_strdup (_("Fully Charged"));
  else if (state == UP_DEVICE_STATE_EMPTY)
    return g_strdup (_("Empty"));
  else if (state == UP_DEVICE_STATE_CHARGING)
    seconds = gf_upower_device_gen_get_time_to_full (self->device);
  else if (state == UP_DEVICE_STATE_DISCHARGING)
    seconds = gf_upower_device_gen_get_time_to_empty (self->device);
  else if (state == UP_DEVICE_STATE_PENDING_CHARGE)
    return g_strdup (_("Not Charging"));
  else
    return g_strdup (_("Estimating..."));

  time = round (seconds / 60.0);

  if (time == 0)
    return g_strdup (_("Estimating..."));

  minutes = fmod (time, 60);
  hours = floor (time / 60);
  percentage = gf_upower_device_gen_get_percentage (self->device);

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

static void
update_indicator_menu (SiPower *self)
{
  const char *type_text;
  char *state_text;
  char *label;
  GtkWidget *separator;
  GtkWidget *item;

  gtk_container_foreach (GTK_CONTAINER (self->menu), remove_item_cb, NULL);

  type_text = get_type_text (self);
  state_text = get_state_text (self);

  label = g_strdup_printf ("%s: %s", type_text, state_text);
  g_free (state_text);

  item = si_desktop_menu_item_new (label, "org.gnome.PowerStats.desktop");
  g_free (label);

  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), item);
  gtk_widget_show (item);

  separator = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), separator);
  gtk_widget_show (separator);

  item = si_desktop_menu_item_new (_("Power Settings"),
                                   "gnome-power-panel.desktop");

  gtk_menu_shell_append (GTK_MENU_SHELL (self->menu), item);
  gtk_widget_show (item);
}

static void
update_indicator_label (SiPower *self)
{
  GtkWidget *menu_item;
  double percentage;
  char *label;

  menu_item = si_indicator_get_menu_item (SI_INDICATOR (self));

  if (self->device == NULL || !self->show_battery_percentage)
    {
      gtk_menu_item_set_label (GTK_MENU_ITEM (menu_item), NULL);
      return;
    }

  percentage = gf_upower_device_gen_get_percentage (self->device);
  label = g_strdup_printf ("%.0f%%", percentage);

  gtk_menu_item_set_label (GTK_MENU_ITEM (menu_item), label);
  g_free (label);
}

static GIcon *
get_themed_icon (SiPower  *self,
                 gboolean  symbolic)
{
  const char *device_icon_name;
  GIcon *icon;

  device_icon_name = gf_upower_device_gen_get_icon_name (self->device);

  if (device_icon_name != NULL && device_icon_name[0] != '\0')
    {
      if (!symbolic && g_str_has_suffix (device_icon_name, "-symbolic"))
        {
          char *icon_name;
          char *tmp;

          icon_name = g_strdup (device_icon_name);
          tmp = g_strrstr (icon_name, "-symbolic");

          if (tmp != NULL)
            *tmp = '\0';

          icon = g_themed_icon_new (icon_name);
          g_free (icon_name);
        }
      else
        {
          icon = g_themed_icon_new (device_icon_name);
        }
    }
  else
    {
      if (symbolic)
        icon = g_themed_icon_new ("battery-symbolic");
      else
        icon = g_themed_icon_new ("battery");
    }

  return icon;
}

static void
update_indicator_icon (SiPower *self)
{
  GpApplet *applet;
  gboolean symbolic;
  GIcon *icon;

  if (self->device == NULL)
    return;

  applet = si_indicator_get_applet (SI_INDICATOR (self));
  symbolic = gp_applet_get_prefer_symbolic_icons (applet);

  icon = get_themed_icon (self, symbolic);

  if (symbolic)
    {
      UpDeviceState state;
      double percentage;
      int level;
      char *icon_name;

      state = gf_upower_device_gen_get_state (self->device);
      percentage = gf_upower_device_gen_get_percentage (self->device);
      level = floor (percentage / 10);

      if (level == 100 || state == UP_DEVICE_STATE_FULLY_CHARGED)
        {
          icon_name = g_strdup ("battery-level-100-charged-symbolic");
        }
      else
        {
          gboolean charging;

          charging = state == UP_DEVICE_STATE_CHARGING;
          icon_name = g_strdup_printf ("battery-level-%d%s-symbolic",
                                       level,
                                       charging ? "-charging" : "");
        }

      g_themed_icon_prepend_name (G_THEMED_ICON (icon), icon_name);
      g_free (icon_name);
    }

  si_indicator_set_icon (SI_INDICATOR (self), icon);
  g_object_unref (icon);
}

static void
update_indicator (SiPower *self)
{
  GtkWidget *menu_item;
  char *tooltip;

  menu_item = si_indicator_get_menu_item (SI_INDICATOR (self));

  if (self->device == NULL ||
      !gf_upower_device_gen_get_is_present (self->device))
    {
      gtk_widget_hide (menu_item);
      return;
    }

  update_indicator_icon (self);
  update_indicator_label (self);
  update_indicator_menu (self);

  tooltip = get_state_text (self);
  gtk_widget_set_tooltip_text (menu_item , tooltip);
  g_free (tooltip);

  gtk_widget_show (menu_item);
}

static void
prefer_symbolic_icons_cb (GObject    *object,
                          GParamSpec *pspec,
                          SiPower    *self)
{
  update_indicator_icon (self);
}

static void
properties_changed_cb (GDBusProxy *proxy,
                       GVariant   *changed_properties,
                       GStrv       invalidated_properties,
                       SiPower    *self)
{
  update_indicator (self);
}

static void
device_proxy_ready_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GError *error;
  GfUPowerDeviceGen *device;
  SiPower *self;

  error = NULL;
  device = gf_upower_device_gen_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = SI_POWER (user_data);
  self->device = device;

  g_signal_connect (self->device,
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
  SiPower *self;

  self = SI_POWER (user_data);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();

  gf_upower_device_gen_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          "org.freedesktop.UPower",
                                          "/org/freedesktop/UPower/devices/DisplayDevice",
                                          self->cancellable,
                                          device_proxy_ready_cb,
                                          self);
}

static void
name_vanished_handler_cb (GDBusConnection *connection,
                          const char      *name,
                          gpointer         user_data)
{
  SiPower *self;

  self = SI_POWER (user_data);

  g_clear_object (&self->device);
  update_indicator (self);
}

static void
show_battery_percentage_changed_cb (GSettings  *settings,
                                    const char *key,
                                    SiPower    *self)
{
  gboolean show_battery_percentage;

  show_battery_percentage = g_settings_get_boolean (settings,
                                                    SHOW_BATTERY_PERCENTAGE_KEY);

  if (self->show_battery_percentage == show_battery_percentage)
    return;

  self->show_battery_percentage = show_battery_percentage;
  update_indicator_label (self);
}

static void
si_power_constructed (GObject *object)
{
  SiPower *self;
  GtkWidget *menu_item;
  GpApplet *applet;

  self = SI_POWER (object);

  G_OBJECT_CLASS (si_power_parent_class)->constructed (object);

  self->menu = gtk_menu_new ();

  menu_item = si_indicator_get_menu_item (SI_INDICATOR (self));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), self->menu);

  applet = si_indicator_get_applet (SI_INDICATOR (self));

  g_signal_connect (applet,
                    "notify::prefer-symbolic-icons",
                    G_CALLBACK (prefer_symbolic_icons_cb),
                    self);

  update_indicator (self);
}

static void
si_power_dispose (GObject *object)
{
  SiPower *self;

  self = SI_POWER (object);

  if (self->bus_name_id)
    {
      g_bus_unwatch_name (self->bus_name_id);
      self->bus_name_id = 0;
    }

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->settings);
  g_clear_object (&self->device);

  G_OBJECT_CLASS (si_power_parent_class)->dispose (object);
}

static void
si_power_class_init (SiPowerClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = si_power_constructed;
  object_class->dispose = si_power_dispose;
}

static void
si_power_init (SiPower *self)
{
  self->settings = g_settings_new ("org.gnome.desktop.interface");

  g_signal_connect (self->settings,
                    "changed::" SHOW_BATTERY_PERCENTAGE_KEY,
                    G_CALLBACK (show_battery_percentage_changed_cb),
                    self);

  self->show_battery_percentage = g_settings_get_boolean (self->settings,
                                                          SHOW_BATTERY_PERCENTAGE_KEY);

  self->bus_name_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                        "org.freedesktop.UPower",
                                        G_BUS_NAME_WATCHER_FLAGS_NONE,
                                        name_appeared_handler_cb,
                                        name_vanished_handler_cb,
                                        self,
                                        NULL);
}

SiIndicator *
si_power_new (GpApplet *applet)
{
  return g_object_new (SI_TYPE_POWER,
                       "applet", applet,
                       NULL);
}
