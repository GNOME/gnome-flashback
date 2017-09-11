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

typedef struct
{
  GfBackend *backend;
} GfMonitorManagerPrivate;

enum
{
  PROP_0,

  PROP_BACKEND,

  LAST_PROP
};

static GParamSpec *manager_properties[LAST_PROP] = { NULL };

static void gf_monitor_manager_display_config_init (GfDBusDisplayConfigIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GfMonitorManager, gf_monitor_manager, GF_DBUS_TYPE_DISPLAY_CONFIG_SKELETON,
                                  G_ADD_PRIVATE (GfMonitorManager)
                                  G_IMPLEMENT_INTERFACE (GF_DBUS_TYPE_DISPLAY_CONFIG, gf_monitor_manager_display_config_init))

static void
gf_monitor_manager_display_config_init (GfDBusDisplayConfigIface *iface)
{
}

static void
gf_monitor_manager_dispose (GObject *object)
{
  GfMonitorManager *manager;
  GfMonitorManagerPrivate *priv;

  manager = GF_MONITOR_MANAGER (object);
  priv = gf_monitor_manager_get_instance_private (manager);

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
gf_monitor_manager_class_init (GfMonitorManagerClass *manager_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (manager_class);

  object_class->dispose = gf_monitor_manager_dispose;
  object_class->get_property = gf_monitor_manager_get_property;
  object_class->set_property = gf_monitor_manager_set_property;

  gf_monitor_manager_install_properties (object_class);
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
