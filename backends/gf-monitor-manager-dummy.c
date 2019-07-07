/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2017-2019 Alberts Muktupāvels
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
 * - src/backends/meta-monitor-manager-dummy.c
 */

#include "config.h"
#include "gf-monitor-manager-dummy-private.h"

struct _GfMonitorManagerDummy
{
  GfMonitorManager parent;
};

G_DEFINE_TYPE (GfMonitorManagerDummy, gf_monitor_manager_dummy, GF_TYPE_MONITOR_MANAGER)

static void
gf_monitor_manager_dummy_ensure_initial_config (GfMonitorManager *manager)
{
}

static gboolean
gf_monitor_manager_dummy_apply_monitors_config (GfMonitorManager        *manager,
                                                GfMonitorsConfig        *config,
                                                GfMonitorsConfigMethod   method,
                                                GError                 **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Not implemented");
  return FALSE;
}

static gboolean
gf_monitor_manager_dummy_is_transform_handled (GfMonitorManager   *manager,
                                               GfCrtc             *crtc,
                                               GfMonitorTransform  transform)
{
  return FALSE;
}

static gfloat
gf_monitor_manager_dummy_calculate_monitor_mode_scale (GfMonitorManager *manager,
                                                       GfMonitor        *monitor,
                                                       GfMonitorMode    *monitor_mode)
{
  return 1.0;
}

static gfloat *
gf_monitor_manager_dummy_calculate_supported_scales (GfMonitorManager           *manager,
                                                     GfLogicalMonitorLayoutMode  layout_mode,
                                                     GfMonitor                  *monitor,
                                                     GfMonitorMode              *monitor_mode,
                                                     gint                       *n_supported_scales)
{
  *n_supported_scales = 0;
  return NULL;
}

static GfMonitorManagerCapability
gf_monitor_manager_dummy_get_capabilities (GfMonitorManager *manager)
{

  return GF_MONITOR_MANAGER_CAPABILITY_NONE;
}

static gboolean
gf_monitor_manager_dummy_get_max_screen_size (GfMonitorManager *manager,
                                              gint             *max_width,
                                              gint             *max_height)
{
  return FALSE;
}

static GfLogicalMonitorLayoutMode
gf_monitor_manager_dummy_get_default_layout_mode (GfMonitorManager *manager)
{
  return GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;
}

static void
gf_monitor_manager_dummy_class_init (GfMonitorManagerDummyClass *dummy_class)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_CLASS (dummy_class);

  manager_class->ensure_initial_config = gf_monitor_manager_dummy_ensure_initial_config;
  manager_class->apply_monitors_config = gf_monitor_manager_dummy_apply_monitors_config;
  manager_class->is_transform_handled = gf_monitor_manager_dummy_is_transform_handled;
  manager_class->calculate_monitor_mode_scale = gf_monitor_manager_dummy_calculate_monitor_mode_scale;
  manager_class->calculate_supported_scales = gf_monitor_manager_dummy_calculate_supported_scales;
  manager_class->get_capabilities = gf_monitor_manager_dummy_get_capabilities;
  manager_class->get_max_screen_size = gf_monitor_manager_dummy_get_max_screen_size;
  manager_class->get_default_layout_mode = gf_monitor_manager_dummy_get_default_layout_mode;
}

static void
gf_monitor_manager_dummy_init (GfMonitorManagerDummy *dummy)
{
}
