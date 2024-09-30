/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2017-2019 Alberts Muktupāvels
 * Copyright (C) 2020 NVIDIA CORPORATION
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
 * - src/backends/meta-monitor-manager-private.h
 */

#ifndef GF_MONITOR_MANAGER_PRIVATE_H
#define GF_MONITOR_MANAGER_PRIVATE_H

#include <libgnome-desktop/gnome-pnp-ids.h>

#include "gf-backend-private.h"
#include "gf-display-config-shared.h"
#include "gf-monitor-manager-enums-private.h"
#include "gf-monitor-manager-types-private.h"
#include "gf-monitor-manager.h"
#include "gf-monitor-transform.h"

G_BEGIN_DECLS

#define GF_TYPE_MONITOR_MANAGER         (gf_monitor_manager_get_type ())
#define GF_MONITOR_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GF_TYPE_MONITOR_MANAGER, GfMonitorManager))
#define GF_MONITOR_MANAGER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    GF_TYPE_MONITOR_MANAGER, GfMonitorManagerClass))
#define GF_IS_MONITOR_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GF_TYPE_MONITOR_MANAGER))
#define GF_IS_MONITOR_MANAGER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    GF_TYPE_MONITOR_MANAGER))
#define GF_MONITOR_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  GF_TYPE_MONITOR_MANAGER, GfMonitorManagerClass))

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GfMonitorManager, g_object_unref)

struct _GfMonitorManager
{
  GObject                      parent;

  GfDBusDisplayConfig         *display_config;

  gboolean                     in_init;

  guint                        serial;

  GfLogicalMonitorLayoutMode   layout_mode;

  gint                         screen_width;
  gint                         screen_height;

  GList                       *monitors;

  GList                       *logical_monitors;
  GfLogicalMonitor            *primary_logical_monitor;

  gboolean                     panel_orientation_managed;

  GfMonitorConfigManager      *config_manager;

  GnomePnpIds                 *pnp_ids;

  GfMonitorSwitchConfigType    current_switch_config;
};

typedef struct
{
  GObjectClass parent_class;

  GBytes                     * (* read_edid)                    (GfMonitorManager            *manager,
                                                                 GfOutput                    *output);

  void                         (* read_current_state)           (GfMonitorManager            *manager);

  void                         (* ensure_initial_config)        (GfMonitorManager            *manager);

  gboolean                     (* apply_monitors_config)        (GfMonitorManager            *manager,
                                                                 GfMonitorsConfig            *config,
                                                                 GfMonitorsConfigMethod       method,
                                                                 GError                     **error);

  void                         (* set_power_save_mode)          (GfMonitorManager            *manager,
                                                                 GfPowerSave                  mode);

  void                         (* get_crtc_gamma)               (GfMonitorManager            *manager,
                                                                 GfCrtc                      *crtc,
                                                                 gsize                       *size,
                                                                 gushort                    **red,
                                                                 gushort                    **green,
                                                                 gushort                    **blue);

  void                         (* set_crtc_gamma)               (GfMonitorManager            *manager,
                                                                 GfCrtc                      *crtc,
                                                                 gsize                        size,
                                                                 gushort                     *red,
                                                                 gushort                     *green,
                                                                 gushort                     *blue);

  void                         (* tiled_monitor_added)          (GfMonitorManager            *manager,
                                                                 GfMonitor                   *monitor);

  void                         (* tiled_monitor_removed)        (GfMonitorManager            *manager,
                                                                 GfMonitor                   *monitor);

  gboolean                     (* is_transform_handled)         (GfMonitorManager            *manager,
                                                                 GfCrtc                      *crtc,
                                                                 GfMonitorTransform           transform);

  gfloat                       (* calculate_monitor_mode_scale) (GfMonitorManager            *manager,
                                                                 GfLogicalMonitorLayoutMode   layout_mode,
                                                                 GfMonitor                   *monitor,
                                                                 GfMonitorMode               *monitor_mode);

  gfloat                     * (* calculate_supported_scales)   (GfMonitorManager            *manager,
                                                                 GfLogicalMonitorLayoutMode   layout_mode,
                                                                 GfMonitor                   *monitor,
                                                                 GfMonitorMode               *monitor_mode,
                                                                 gint                         *n_supported_scales);

  GfMonitorManagerCapability   (* get_capabilities)             (GfMonitorManager            *manager);

  gboolean                     (* get_max_screen_size)          (GfMonitorManager            *manager,
                                                                 gint                        *max_width,
                                                                 gint                        *max_height);

  GfLogicalMonitorLayoutMode   (* get_default_layout_mode)      (GfMonitorManager            *manager);

  void                         (* set_output_ctm)               (GfOutput                    *output,
                                                                 const GfOutputCtm           *ctm);
} GfMonitorManagerClass;

GType                       gf_monitor_manager_get_type                     (void);

GfBackend                  *gf_monitor_manager_get_backend                  (GfMonitorManager            *manager);

void                        gf_monitor_manager_setup                        (GfMonitorManager            *manager);

void                        gf_monitor_manager_rebuild_derived              (GfMonitorManager            *manager,
                                                                             GfMonitorsConfig            *config);

GList                      *gf_monitor_manager_get_logical_monitors         (GfMonitorManager            *manager);

GfMonitor                  *gf_monitor_manager_get_primary_monitor          (GfMonitorManager            *manager);

GfMonitor                  *gf_monitor_manager_get_laptop_panel             (GfMonitorManager            *manager);

GfMonitor                  *gf_monitor_manager_get_monitor_from_spec        (GfMonitorManager            *manager,
                                                                             GfMonitorSpec               *monitor_spec);

GList                      *gf_monitor_manager_get_monitors                 (GfMonitorManager            *manager);

GfLogicalMonitor           *gf_monitor_manager_get_primary_logical_monitor  (GfMonitorManager            *manager);

gboolean                    gf_monitor_manager_has_hotplug_mode_update      (GfMonitorManager            *manager);
void                        gf_monitor_manager_read_current_state           (GfMonitorManager            *manager);

void                        gf_monitor_manager_reconfigure                  (GfMonitorManager            *self);

gboolean                    gf_monitor_manager_get_monitor_matrix           (GfMonitorManager            *manager,
                                                                             GfMonitor                   *monitor,
                                                                             GfLogicalMonitor            *logical_monitor,
                                                                             gfloat                       matrix[6]);

void                        gf_monitor_manager_tiled_monitor_added          (GfMonitorManager            *manager,
                                                                             GfMonitor                   *monitor);

void                        gf_monitor_manager_tiled_monitor_removed        (GfMonitorManager            *manager,
                                                                             GfMonitor                   *monitor);

gboolean                    gf_monitor_manager_is_transform_handled         (GfMonitorManager            *manager,
                                                                             GfCrtc                      *crtc,
                                                                             GfMonitorTransform           transform);

GfMonitorsConfig           *gf_monitor_manager_ensure_configured            (GfMonitorManager            *manager);

void                        gf_monitor_manager_update_logical_state_derived (GfMonitorManager            *manager,
                                                                             GfMonitorsConfig            *config);

gfloat                      gf_monitor_manager_calculate_monitor_mode_scale (GfMonitorManager            *manager,
                                                                             GfLogicalMonitorLayoutMode   layout_mode,
                                                                             GfMonitor                   *monitor,
                                                                             GfMonitorMode               *monitor_mode);

gfloat                     *gf_monitor_manager_calculate_supported_scales   (GfMonitorManager            *manager,
                                                                             GfLogicalMonitorLayoutMode   layout_mode,
                                                                             GfMonitor                   *monitor,
                                                                             GfMonitorMode               *monitor_mode,
                                                                             gint                        *n_supported_scales);

gboolean                    gf_monitor_manager_is_scale_supported           (GfMonitorManager            *manager,
                                                                             GfLogicalMonitorLayoutMode   layout_mode,
                                                                             GfMonitor                   *monitor,
                                                                             GfMonitorMode               *monitor_mode,
                                                                             gfloat                       scale);

GfMonitorManagerCapability  gf_monitor_manager_get_capabilities             (GfMonitorManager            *manager);

gboolean                    gf_monitor_manager_get_max_screen_size          (GfMonitorManager            *manager,
                                                                             gint                        *max_width,
                                                                             gint                        *max_height);

GfLogicalMonitorLayoutMode  gf_monitor_manager_get_default_layout_mode      (GfMonitorManager            *manager);

GfMonitorConfigManager     *gf_monitor_manager_get_config_manager           (GfMonitorManager            *manager);

char                       *gf_monitor_manager_get_vendor_name              (GfMonitorManager            *manager,
                                                                             const char                  *vendor);

GfPowerSave                 gf_monitor_manager_get_power_save_mode          (GfMonitorManager            *manager);

void                        gf_monitor_manager_power_save_mode_changed      (GfMonitorManager            *manager,
                                                                             GfPowerSave                  mode);

gboolean                    gf_monitor_manager_is_monitor_visible           (GfMonitorManager            *manager,
                                                                             GfMonitor                   *monitor);

G_END_DECLS

#endif
