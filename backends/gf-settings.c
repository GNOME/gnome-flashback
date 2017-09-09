/*
 * Copyright (C) 2017 Alberts Muktupāvels
 * Copyright (C) 2017 Red Hat
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
 * Adapted from mutter:
 * - src/backends/meta-settings.c
 */

#include "config.h"

#include <gio/gio.h>

#include "gf-backend-private.h"
#include "gf-settings-private.h"

struct _GfSettings
{
  GObject    parent;

  GfBackend *backend;

  GSettings *interface;

  gint       global_scaling_factor;
};

enum
{
  PROP_0,

  PROP_BACKEND,

  LAST_PROP
};

static GParamSpec *settings_properties[LAST_PROP] = { NULL };

enum
{
  GLOBAL_SCALING_FACTOR_CHANGED,

  LAST_SIGNAL
};

static guint settings_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfSettings, gf_settings, G_TYPE_OBJECT)

static gboolean
update_global_scaling_factor (GfSettings *settings)
{
  gint scale;

  scale = (gint) g_settings_get_uint (settings->interface, "scaling-factor");

  if (settings->global_scaling_factor != scale)
    {
      settings->global_scaling_factor = scale;
      return TRUE;
    }

  return FALSE;
}

static void
interface_changed_cb (GSettings   *interface,
                      const gchar *key,
                      GfSettings  *settings)
{
  if (g_str_equal (key, "scaling-factor"))
    {
      if (update_global_scaling_factor (settings))
        g_signal_emit (settings, settings_signals[GLOBAL_SCALING_FACTOR_CHANGED], 0);
    }
}

static void
gf_settings_dispose (GObject *object)
{
  GfSettings *settings;

  settings = GF_SETTINGS (object);

  g_clear_object (&settings->interface);
  settings->backend = NULL;

  G_OBJECT_CLASS (gf_settings_parent_class)->dispose (object);
}

static void
gf_settings_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GfSettings *settings;

  settings = GF_SETTINGS (object);

  switch (property_id)
    {
      case PROP_BACKEND:
        g_value_set_object (value, settings->backend);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_settings_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GfSettings *settings;

  settings = GF_SETTINGS (object);

  switch (property_id)
    {
      case PROP_BACKEND:
        settings->backend = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_settings_install_properties (GObjectClass *object_class)
{
  settings_properties[PROP_BACKEND] =
    g_param_spec_object ("backend",
                         "GfBackend",
                         "GfBackend",
                         GF_TYPE_BACKEND,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     settings_properties);
}

static void
gf_settings_install_signals (GObjectClass *object_class)
{
  settings_signals[GLOBAL_SCALING_FACTOR_CHANGED] =
    g_signal_new ("global-scaling-factor-changed",
                  G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gf_settings_class_init (GfSettingsClass *settings_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (settings_class);

  object_class->dispose = gf_settings_dispose;
  object_class->get_property = gf_settings_get_property;
  object_class->set_property = gf_settings_set_property;

  gf_settings_install_properties (object_class);
  gf_settings_install_signals (object_class);
}

static void
gf_settings_init (GfSettings *settings)
{
  settings->interface = g_settings_new ("org.gnome.desktop.interface");

  g_signal_connect (settings->interface, "changed",
                    G_CALLBACK (interface_changed_cb), settings);

  update_global_scaling_factor (settings);
}

GfSettings *
gf_settings_new (GfBackend *backend)
{
  GfSettings *settings;

  settings = g_object_new (GF_TYPE_SETTINGS,
                           "backend", backend,
                           NULL);

  return settings;
}

gboolean
gf_settings_get_global_scaling_factor (GfSettings *settings,
                                       gint       *global_scaling_factor)
{
  if (settings->global_scaling_factor == 0)
    return FALSE;

  *global_scaling_factor = settings->global_scaling_factor;

  return TRUE;
}
