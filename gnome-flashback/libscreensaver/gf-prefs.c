/*
 * Copyright (C) 2004-2006 William Jon McCann
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     William Jon McCann <mccann@jhu.edu>
 */

#include "config.h"
#include "gf-prefs.h"

#include <gio/gio.h>

struct _GfPrefs
{
  GObject    parent;

  GSettings *lockdown;
  GSettings *screensaver;

  gboolean   lock_disabled;
  gboolean   user_switch_disabled;

  gboolean   lock_enabled;
  guint      lock_delay;
  gboolean   user_switch_enabled;
};

enum
{
  CHANGED,

  LAST_SIGNAL
};

static guint prefs_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfPrefs, gf_prefs, G_TYPE_OBJECT)

static void
lockdown_changed_cb (GSettings   *settings,
                     const gchar *key,
                     GfPrefs     *self)
{
  if (key == NULL || g_strcmp0 (key, "disable-lock-screen") == 0)
    {
      gboolean disabled;

      disabled = g_settings_get_boolean (settings, "disable-lock-screen");

      self->lock_disabled = disabled;
    }

  if (key == NULL || g_strcmp0 (key, "disable-user-switching") == 0)
    {
      gboolean disabled;

      disabled = g_settings_get_boolean (settings, "disable-user-switching");

      self->user_switch_disabled = disabled;
    }

  g_signal_emit (self, prefs_signals[CHANGED], 0);
}

static void
screensaver_changed_cb (GSettings   *settings,
                        const gchar *key,
                        GfPrefs     *self)
{
  gboolean enabled;

  if (key == NULL || g_strcmp0 (key, "lock-enabled") == 0)
    {
      enabled = g_settings_get_boolean (settings, "lock-enabled");

      self->lock_enabled = enabled;
    }

  if (key == NULL || g_strcmp0 (key, "lock-delay") == 0)
    {
      guint delay;

      delay = g_settings_get_uint (settings, "lock-delay");

      /* prevent overflow when converting to milliseconds */
      if (delay > G_MAXUINT / 1000)
        delay = G_MAXUINT / 1000;

      self->lock_delay = delay * 1000;
    }

  if (key == NULL || g_strcmp0 (key, "user-switch-enabled") == 0)
    {
      enabled = g_settings_get_boolean (settings, "user-switch-enabled");

      self->user_switch_enabled = enabled;
    }

  g_signal_emit (self, prefs_signals[CHANGED], 0);
}

static void
gf_prefs_dispose (GObject *object)
{
  GfPrefs *self;

  self = GF_PREFS (object);

  g_clear_object (&self->lockdown);
  g_clear_object (&self->screensaver);

  G_OBJECT_CLASS (gf_prefs_parent_class)->dispose (object);
}

static void
install_signals (void)
{
  prefs_signals[CHANGED] =
    g_signal_new ("changed", GF_TYPE_PREFS, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gf_prefs_class_init (GfPrefsClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_prefs_dispose;

  install_signals ();
}

static void
gf_prefs_init (GfPrefs *self)
{
  self->lockdown = g_settings_new ("org.gnome.desktop.lockdown");
  self->screensaver = g_settings_new ("org.gnome.desktop.screensaver");

  g_signal_connect (self->lockdown, "changed",
                    G_CALLBACK (lockdown_changed_cb), self);

  g_signal_connect (self->screensaver, "changed",
                    G_CALLBACK (screensaver_changed_cb), self);

  lockdown_changed_cb (self->lockdown, NULL, self);
  screensaver_changed_cb (self->screensaver, NULL, self);
}

GfPrefs *
gf_prefs_new (void)
{
  return g_object_new (GF_TYPE_PREFS, NULL);
}

gboolean
gf_prefs_get_lock_disabled (GfPrefs *self)
{
  return self->lock_disabled;
}

gboolean
gf_prefs_get_user_switch_disabled (GfPrefs *self)
{
  return self->user_switch_disabled;
}

gboolean
gf_prefs_get_lock_enabled (GfPrefs *self)
{
  return self->lock_enabled;
}

guint
gf_prefs_get_lock_delay (GfPrefs *self)
{
  return self->lock_delay;
}

gboolean
gf_prefs_get_user_switch_enabled (GfPrefs *self)
{
  return self->user_switch_enabled;
}
