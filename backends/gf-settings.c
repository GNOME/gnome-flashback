/*
 * Copyright (C) 2017-2019 Alberts Muktupāvels
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
#include "gf-settings-private.h"

#include <gio/gio.h>

#include "gf-backend-private.h"
#include "gf-logical-monitor-private.h"
#include "gf-monitor-manager-private.h"

struct _GfSettings
{
  GObject    parent;

  GfBackend *backend;

  GSettings *interface;

  gint       ui_scaling_factor;
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
  UI_SCALING_FACTOR_CHANGED,
  GLOBAL_SCALING_FACTOR_CHANGED,

  LAST_SIGNAL
};

static guint settings_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfSettings, gf_settings, G_TYPE_OBJECT)

static gint
calculate_ui_scaling_factor (GfSettings *settings)
{
  GfMonitorManager *monitor_manager;
  GfLogicalMonitor *primary_logical_monitor;

  monitor_manager = gf_backend_get_monitor_manager (settings->backend);

  if (!monitor_manager)
    return 1;

  primary_logical_monitor = gf_monitor_manager_get_primary_logical_monitor (monitor_manager);

  if (!primary_logical_monitor)
    return 1;

  return (gint) gf_logical_monitor_get_scale (primary_logical_monitor);
}

static gboolean
update_ui_scaling_factor (GfSettings *settings)
{
  gint ui_scaling_factor;

  ui_scaling_factor = calculate_ui_scaling_factor (settings);

  if (settings->ui_scaling_factor != ui_scaling_factor)
    {
      settings->ui_scaling_factor = ui_scaling_factor;
      return TRUE;
    }

  return FALSE;
}

static void
monitors_changed_cb (GfMonitorManager *monitor_manager,
                     GfSettings       *settings)
{
  if (update_ui_scaling_factor (settings))
    g_signal_emit (settings, settings_signals[UI_SCALING_FACTOR_CHANGED], 0);
}

static void
global_scaling_factor_changed_cb (GfSettings *settings,
                                  gpointer    user_data)
{
  if (update_ui_scaling_factor (settings))
    g_signal_emit (settings, settings_signals[UI_SCALING_FACTOR_CHANGED], 0);
}

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
  settings_signals[UI_SCALING_FACTOR_CHANGED] =
    g_signal_new ("ui-scaling-factor-changed",
                  G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

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

  /* Chain up inter-dependent settings. */
  g_signal_connect (settings, "global-scaling-factor-changed",
                    G_CALLBACK (global_scaling_factor_changed_cb), NULL);

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

void
gf_settings_post_init (GfSettings *settings)
{
  GfMonitorManager *monitor_manager;

  monitor_manager = gf_backend_get_monitor_manager (settings->backend);
  g_signal_connect_object (monitor_manager, "monitors-changed",
                           G_CALLBACK (monitors_changed_cb),
                           settings, G_CONNECT_AFTER);

  update_ui_scaling_factor (settings);
}

int
gf_settings_get_ui_scaling_factor (GfSettings *settings)
{
  g_assert (settings->ui_scaling_factor != 0);

  return settings->ui_scaling_factor;
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
