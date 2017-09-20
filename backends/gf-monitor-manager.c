/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2017 Alberts Muktupāvels
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
 * - src/backends/meta-monitor-manager.c
 */

#include "config.h"
#include "gf-monitor-manager-private.h"
#include "gf-monitor-spec-private.h"
#include "gf-monitor-private.h"

typedef struct
{
  GfBackend *backend;

  guint      bus_name_id;
} GfMonitorManagerPrivate;

enum
{
  PROP_0,

  PROP_BACKEND,

  LAST_PROP
};

static GParamSpec *manager_properties[LAST_PROP] = { NULL };

enum
{
  CONFIRM_DISPLAY_CHANGE,

  LAST_SIGNAL
};

static guint manager_signals[LAST_SIGNAL] = { 0 };

static void gf_monitor_manager_display_config_init (GfDBusDisplayConfigIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GfMonitorManager, gf_monitor_manager, GF_DBUS_TYPE_DISPLAY_CONFIG_SKELETON,
                                  G_ADD_PRIVATE (GfMonitorManager)
                                  G_IMPLEMENT_INTERFACE (GF_DBUS_TYPE_DISPLAY_CONFIG, gf_monitor_manager_display_config_init))

static gboolean
gf_monitor_manager_handle_get_resources (GfDBusDisplayConfig   *skeleton,
                                         GDBusMethodInvocation *invocation)
{
  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "Not implemented");

  return TRUE;
}

static gboolean
gf_monitor_manager_handle_change_backlight (GfDBusDisplayConfig   *skeleton,
                                            GDBusMethodInvocation *invocation,
                                            guint                  serial,
                                            guint                  output_index,
                                            gint                   value)
{
  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "Not implemented");

  return TRUE;
}

static gboolean
gf_monitor_manager_handle_get_crtc_gamma (GfDBusDisplayConfig   *skeleton,
                                          GDBusMethodInvocation *invocation,
                                          guint                  serial,
                                          guint                  crtc_id)
{
  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "Not implemented");

  return TRUE;
}

static gboolean
gf_monitor_manager_handle_set_crtc_gamma (GfDBusDisplayConfig   *skeleton,
                                          GDBusMethodInvocation *invocation,
                                          guint                  serial,
                                          guint                  crtc_id,
                                          GVariant              *red_v,
                                          GVariant              *green_v,
                                          GVariant              *blue_v)
{
  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "Not implemented");

  return TRUE;
}

static gboolean
gf_monitor_manager_handle_get_current_state (GfDBusDisplayConfig   *skeleton,
                                             GDBusMethodInvocation *invocation)
{
  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "Not implemented");

  return TRUE;
}

static gboolean
gf_monitor_manager_handle_apply_monitors_config (GfDBusDisplayConfig   *skeleton,
                                                 GDBusMethodInvocation *invocation,
                                                 guint                  serial,
                                                 guint                  method,
                                                 GVariant              *logical_monitor_configs_variant,
                                                 GVariant              *properties_variant)
{
  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "Not implemented");

  return TRUE;
}

static void
gf_monitor_manager_display_config_init (GfDBusDisplayConfigIface *iface)
{
  iface->handle_get_resources = gf_monitor_manager_handle_get_resources;
  iface->handle_change_backlight = gf_monitor_manager_handle_change_backlight;
  iface->handle_get_crtc_gamma = gf_monitor_manager_handle_get_crtc_gamma;
  iface->handle_set_crtc_gamma = gf_monitor_manager_handle_set_crtc_gamma;
  iface->handle_get_current_state = gf_monitor_manager_handle_get_current_state;
  iface->handle_apply_monitors_config = gf_monitor_manager_handle_apply_monitors_config;
}

static void
bus_acquired_cb (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  GfMonitorManager *manager;
  GDBusInterfaceSkeleton *skeleton;

  manager = GF_MONITOR_MANAGER (user_data);
  skeleton = G_DBUS_INTERFACE_SKELETON (manager);

  g_dbus_interface_skeleton_export (skeleton, connection,
                                    "/org/gnome/Mutter/DisplayConfig",
                                    NULL);
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
name_lost_cb (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
}

static void
gf_monitor_manager_constructed (GObject *object)
{
  GfMonitorManager *manager;
  GfMonitorManagerPrivate *priv;

  manager = GF_MONITOR_MANAGER (object);
  priv = gf_monitor_manager_get_instance_private (manager);

  G_OBJECT_CLASS (gf_monitor_manager_parent_class)->constructed (object);

  priv->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                      "org.gnome.Mutter.DisplayConfig",
                                      G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                      G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                      bus_acquired_cb,
                                      name_acquired_cb,
                                      name_lost_cb,
                                      g_object_ref (manager),
                                      g_object_unref);
}

static void
gf_monitor_manager_dispose (GObject *object)
{
  GfMonitorManager *manager;
  GfMonitorManagerPrivate *priv;

  manager = GF_MONITOR_MANAGER (object);
  priv = gf_monitor_manager_get_instance_private (manager);

  if (priv->bus_name_id != 0)
    {
      g_bus_unown_name (priv->bus_name_id);
      priv->bus_name_id = 0;
    }

  priv->backend = NULL;

  G_OBJECT_CLASS (gf_monitor_manager_parent_class)->dispose (object);
}

static void
gf_monitor_manager_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GfMonitorManager *manager;
  GfMonitorManagerPrivate *priv;

  manager = GF_MONITOR_MANAGER (object);
  priv = gf_monitor_manager_get_instance_private (manager);

  switch (property_id)
    {
      case PROP_BACKEND:
        g_value_set_object (value, priv->backend);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_monitor_manager_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GfMonitorManager *manager;
  GfMonitorManagerPrivate *priv;

  manager = GF_MONITOR_MANAGER (object);
  priv = gf_monitor_manager_get_instance_private (manager);

  switch (property_id)
    {
      case PROP_BACKEND:
        priv->backend = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_monitor_manager_install_properties (GObjectClass *object_class)
{
  manager_properties[PROP_BACKEND] =
    g_param_spec_object ("backend",
                         "GfBackend",
                         "GfBackend",
                         GF_TYPE_BACKEND,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     manager_properties);
}

static void
gf_monitor_manager_install_signals (GObjectClass *object_class)
{
  manager_signals[CONFIRM_DISPLAY_CHANGE] =
    g_signal_new ("confirm-display-change",
                  G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gf_monitor_manager_class_init (GfMonitorManagerClass *manager_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (manager_class);

  object_class->constructed = gf_monitor_manager_constructed;
  object_class->dispose = gf_monitor_manager_dispose;
  object_class->get_property = gf_monitor_manager_get_property;
  object_class->set_property = gf_monitor_manager_set_property;

  gf_monitor_manager_install_properties (object_class);
  gf_monitor_manager_install_signals (object_class);
}

static void
gf_monitor_manager_init (GfMonitorManager *manager)
{
}

GfBackend *
gf_monitor_manager_get_backend (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;

  priv = gf_monitor_manager_get_instance_private (manager);

  return priv->backend;
}

GfMonitor *
gf_monitor_manager_get_monitor_from_spec (GfMonitorManager *manager,
                                          GfMonitorSpec    *monitor_spec)
{
  GList *l;

  for (l = manager->monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;

      if (gf_monitor_spec_equals (gf_monitor_get_spec (monitor), monitor_spec))
        return monitor;
    }

  return NULL;
}

void
gf_monitor_manager_tiled_monitor_added (GfMonitorManager *manager,
                                        GfMonitor        *monitor)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  if (manager_class->tiled_monitor_added)
    manager_class->tiled_monitor_added (manager, monitor);
}

void
gf_monitor_manager_tiled_monitor_removed (GfMonitorManager *manager,
                                          GfMonitor        *monitor)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  if (manager_class->tiled_monitor_removed)
    manager_class->tiled_monitor_removed (manager, monitor);
}

GfMonitorManagerCapability
gf_monitor_manager_get_capabilities (GfMonitorManager *manager)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->get_capabilities (manager);
}
