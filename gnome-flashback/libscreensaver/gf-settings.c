/*
 * Copyright (C) 2004-2006 William Jon McCann
 * Copyright (C) 2016 Alberts Muktupāvels
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     William Jon McCann <mccann@jhu.edu>
 */

#include "config.h"

#include <gio/gio.h>

#include "gf-settings.h"

struct _GfSettings
{
  GObject    parent;

  GSettings *lockdown;
  GSettings *screensaver;

  gboolean   initialized;

  gboolean   lock_disabled;
  gboolean   user_switch_disabled;

  gboolean   embedded_keyboard_enabled;
  gchar     *embedded_keyboard_command;
  gboolean   idle_activation_enabled;
  gboolean   lock_enabled;
  guint      lock_delay;
  gboolean   logout_enabled;
  guint      logout_delay;
  gchar     *logout_command;
  gboolean   status_message_enabled;
  gboolean   user_switch_enabled;
};

enum
{
  CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfSettings, gf_settings, G_TYPE_OBJECT)

static void
lockdown_changed_cb (GSettings   *gsettings,
                     const gchar *key,
                     GfSettings  *settings)
{
  if (key == NULL || g_strcmp0 (key, "disable-lock-screen") == 0)
    {
      gboolean disabled;

      disabled = g_settings_get_boolean (gsettings, "disable-lock-screen");

      settings->lock_disabled = disabled;
    }

  if (key == NULL || g_strcmp0 (key, "disable-user-switching") == 0)
    {
      gboolean disabled;

      disabled = g_settings_get_boolean (gsettings, "disable-user-switching");

      settings->user_switch_disabled = disabled;
    }

  if (settings->initialized)
    {
      g_signal_emit (settings, signals[CHANGED], 0);
    }
}

static void
screensaver_changed_cb (GSettings   *gsettings,
                        const gchar *key,
                        GfSettings  *settings)
{
  gboolean enabled;
  const gchar *command;
  guint delay;

  if (key == NULL || g_strcmp0 (key, "embedded-keyboard-enabled") == 0)
    {
      enabled = g_settings_get_boolean (gsettings, "embedded-keyboard-enabled");

      settings->embedded_keyboard_enabled = enabled;
    }

  if (key == NULL || g_strcmp0 (key, "embedded-keyboard-command") == 0)
    {
      command = g_settings_get_string (gsettings, "embedded-keyboard-command");

      g_free (settings->embedded_keyboard_command);
      settings->embedded_keyboard_command = g_strdup (command);
    }

  if (key == NULL || g_strcmp0 (key, "idle-activation-enabled") == 0)
    {
      enabled = g_settings_get_boolean (gsettings, "idle-activation-enabled");

      settings->idle_activation_enabled = enabled;
    }

  if (key == NULL || g_strcmp0 (key, "lock-enabled") == 0)
    {
      enabled = g_settings_get_boolean (gsettings, "lock-enabled");

      settings->lock_enabled = enabled;
    }

  if (key == NULL || g_strcmp0 (key, "lock-delay") == 0)
    {
      delay = g_settings_get_uint (gsettings, "lock-delay");

      /* prevent overflow when converting to milliseconds */
      if (delay > G_MAXUINT / 1000)
        {
          delay = G_MAXUINT / 1000;
        }

      settings->lock_delay = delay * 1000;
    }

  if (key == NULL || g_strcmp0 (key, "logout-enabled") == 0)
    {
      enabled = g_settings_get_boolean (gsettings, "logout-enabled");

      settings->logout_enabled = enabled;
    }

  if (key == NULL || g_strcmp0 (key, "logout-delay") == 0)
    {
      delay = g_settings_get_uint (gsettings, "logout-delay");

      /* prevent overflow when converting to milliseconds */
      if (delay > G_MAXUINT / 1000)
        {
          delay = G_MAXUINT / 1000;
        }

      settings->logout_delay = delay;
    }

  if (key == NULL || g_strcmp0 (key, "logout-command") == 0)
    {
      command = g_settings_get_string (gsettings, "logout-command");

      g_free (settings->logout_command);
      settings->logout_command = g_strdup (command);
    }

  if (key == NULL || g_strcmp0 (key, "status-message-enabled") == 0)
    {
      enabled = g_settings_get_boolean (gsettings, "status-message-enabled");

      settings->status_message_enabled = enabled;
    }

  if (key == NULL || g_strcmp0 (key, "user-switch-enabled") == 0)
    {
      enabled = g_settings_get_boolean (gsettings, "user-switch-enabled");

      settings->user_switch_enabled = enabled;
    }

  if (settings->initialized)
    {
      g_signal_emit (settings, signals[CHANGED], 0);
    }
}

static void
gf_settings_dispose (GObject *object)
{
  GfSettings *settings;

  settings = GF_SETTINGS (object);

  g_clear_object (&settings->lockdown);
  g_clear_object (&settings->screensaver);

  G_OBJECT_CLASS (gf_settings_parent_class)->dispose (object);
}

static void
gf_settings_finalize (GObject *object)
{
  GfSettings *settings;

  settings = GF_SETTINGS (object);

  g_free (settings->embedded_keyboard_command);
  g_free (settings->logout_command);

  G_OBJECT_CLASS (gf_settings_parent_class)->finalize (object);
}

static void
install_signals (GObjectClass *object_class)
{
  signals[CHANGED] =
    g_signal_new ("changed", GF_TYPE_SETTINGS, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gf_settings_class_init (GfSettingsClass *settings_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (settings_class);

  object_class->dispose = gf_settings_dispose;
  object_class->finalize = gf_settings_finalize;

  install_signals (object_class);
}

static void
gf_settings_init (GfSettings *settings)
{
  settings->lockdown = g_settings_new ("org.gnome.desktop.lockdown");

  g_signal_connect (settings->lockdown, "changed",
                    G_CALLBACK (lockdown_changed_cb), settings);

  settings->screensaver = g_settings_new ("org.gnome.desktop.screensaver");

  g_signal_connect (settings->screensaver, "changed",
                    G_CALLBACK (screensaver_changed_cb), settings);

  lockdown_changed_cb (settings->lockdown, NULL, settings);
  screensaver_changed_cb (settings->screensaver, NULL, settings);

  settings->initialized = TRUE;
}

GfSettings *
gf_settings_new (void)
{
  return g_object_new (GF_TYPE_SETTINGS, NULL);
}

gboolean
gf_settings_get_lock_disabled (GfSettings *settings)
{
  return settings->lock_disabled;
}

gboolean
gf_settings_get_user_switch_disabled (GfSettings *settings)
{
  return settings->user_switch_disabled;
}

gboolean
gf_settings_get_embedded_keyboard_enabled (GfSettings *settings)
{
  return settings->embedded_keyboard_enabled;
}

const gchar *
gf_settings_get_embedded_keyboard_command (GfSettings *settings)
{
  return settings->embedded_keyboard_command;
}

gboolean
gf_settings_get_idle_activation_enabled (GfSettings *settings)
{
  return settings->idle_activation_enabled;
}

gboolean
gf_settings_get_lock_enabled (GfSettings *settings)
{
  return settings->lock_enabled;
}

guint
gf_settings_get_lock_delay (GfSettings *settings)
{
  return settings->lock_delay;
}

gboolean
gf_settings_get_logout_enabled (GfSettings *settings)
{
  return settings->logout_enabled;
}

guint
gf_settings_get_logout_delay (GfSettings *settings)
{
  return settings->logout_delay;
}

const gchar *
gf_settings_get_logout_command (GfSettings *settings)
{
  return settings->logout_command;
}

gboolean
gf_settings_get_status_message_enabled (GfSettings *settings)
{
  return settings->status_message_enabled;
}

gboolean
gf_settings_get_user_switch_enabled (GfSettings *settings)
{
  return settings->user_switch_enabled;
}
