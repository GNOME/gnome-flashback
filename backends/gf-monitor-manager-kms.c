/*
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
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     Giovanni Campagna <gcampagn@redhat.com>
 *
 * Adapted from mutter:
 * - src/backends/native/meta-monitor-manager-kms.c
 */

#include "config.h"
#include "gf-monitor-manager-kms-private.h"

#include <gio/gio.h>

struct _GfMonitorManagerKms
{
  GfMonitorManager parent;
};

static void
initable_iface_init (GInitableIface *initable_iface);

G_DEFINE_TYPE_WITH_CODE (GfMonitorManagerKms, gf_monitor_manager_kms,
                         GF_TYPE_MONITOR_MANAGER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init))

static gboolean
gf_monitor_manager_kms_initable_init (GInitable     *initable,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  return TRUE;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = gf_monitor_manager_kms_initable_init;
}

static GBytes *
gf_monitor_manager_kms_read_edid (GfMonitorManager *manager,
                                  GfOutput         *output)
{
  return NULL;
}

static void
gf_monitor_manager_kms_ensure_initial_config (GfMonitorManager *manager)
{
}

static gboolean
gf_monitor_manager_kms_apply_monitors_config (GfMonitorManager        *manager,
                                              GfMonitorsConfig        *config,
                                              GfMonitorsConfigMethod   method,
                                              GError                 **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Not implemented");
  return FALSE;
}

static void
gf_monitor_manager_kms_set_power_save_mode (GfMonitorManager *manager,
                                            GfPowerSave       mode)
{
}

static void
gf_monitor_manager_kms_get_crtc_gamma (GfMonitorManager  *manager,
                                       GfCrtc            *crtc,
                                       gsize             *size,
                                       gushort          **red,
                                       gushort          **green,
                                       gushort          **blue)
{
}

static void
gf_monitor_manager_kms_set_crtc_gamma (GfMonitorManager *manager,
                                       GfCrtc           *crtc,
                                       gsize             size,
                                       gushort          *red,
                                       gushort          *green,
                                       gushort          *blue)
{
}

static gboolean
gf_monitor_manager_kms_is_transform_handled (GfMonitorManager   *manager,
                                             GfCrtc             *crtc,
                                             GfMonitorTransform  transform)
{
  return FALSE;
}

static gfloat
gf_monitor_manager_kms_calculate_monitor_mode_scale (GfMonitorManager           *manager,
                                                     GfLogicalMonitorLayoutMode  layout_mode,
                                                     GfMonitor                  *monitor,
                                                     GfMonitorMode              *monitor_mode)
{
  return 1.0;
}

static gfloat *
gf_monitor_manager_kms_calculate_supported_scales (GfMonitorManager           *manager,
                                                   GfLogicalMonitorLayoutMode  layout_mode,
                                                   GfMonitor                  *monitor,
                                                   GfMonitorMode              *monitor_mode,
                                                   gint                       *n_supported_scales)
{
  *n_supported_scales = 0;
  return NULL;
}

static GfMonitorManagerCapability
gf_monitor_manager_kms_get_capabilities (GfMonitorManager *manager)
{
  return GF_MONITOR_MANAGER_CAPABILITY_NONE;
}

static gboolean
gf_monitor_manager_kms_get_max_screen_size (GfMonitorManager *manager,
                                            gint             *max_width,
                                            gint             *max_height)
{
  return FALSE;
}

static GfLogicalMonitorLayoutMode
gf_monitor_manager_kms_get_default_layout_mode (GfMonitorManager *manager)
{
  return GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;
}

static void
gf_monitor_manager_kms_class_init (GfMonitorManagerKmsClass *kms_class)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_CLASS (kms_class);

  manager_class->read_edid = gf_monitor_manager_kms_read_edid;
  manager_class->ensure_initial_config = gf_monitor_manager_kms_ensure_initial_config;
  manager_class->apply_monitors_config = gf_monitor_manager_kms_apply_monitors_config;
  manager_class->set_power_save_mode = gf_monitor_manager_kms_set_power_save_mode;
  manager_class->get_crtc_gamma = gf_monitor_manager_kms_get_crtc_gamma;
  manager_class->set_crtc_gamma = gf_monitor_manager_kms_set_crtc_gamma;
  manager_class->is_transform_handled = gf_monitor_manager_kms_is_transform_handled;
  manager_class->calculate_monitor_mode_scale = gf_monitor_manager_kms_calculate_monitor_mode_scale;
  manager_class->calculate_supported_scales = gf_monitor_manager_kms_calculate_supported_scales;
  manager_class->get_capabilities = gf_monitor_manager_kms_get_capabilities;
  manager_class->get_max_screen_size = gf_monitor_manager_kms_get_max_screen_size;
  manager_class->get_default_layout_mode = gf_monitor_manager_kms_get_default_layout_mode;
}

static void
gf_monitor_manager_kms_init (GfMonitorManagerKms *kms)
{
}
