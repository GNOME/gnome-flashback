/*
 * Copyright (C) 2016 Red Hat
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
 * - src/backends/meta-monitor.h
 */

#ifndef GF_MONITOR_PRIVATE_H
#define GF_MONITOR_PRIVATE_H

#include "gf-monitor-manager-enums-private.h"
#include "gf-monitor-manager-types-private.h"
#include "gf-monitor-manager.h"
#include "gf-rectangle.h"

G_BEGIN_DECLS

#define HANDLED_CRTC_MODE_FLAGS (GF_CRTC_MODE_FLAG_INTERLACE)

typedef enum
{
  GF_MONITOR_SCALES_CONSTRAINT_NONE = 0,
  GF_MONITOR_SCALES_CONSTRAINT_NO_FRAC = (1 << 0)
} GfMonitorScalesConstraint;

typedef struct
{
  gint           width;
  gint           height;
  gfloat         refresh_rate;
  GfCrtcModeFlag flags;
} GfMonitorModeSpec;

typedef struct
{
  GfOutput   *output;
  GfCrtcMode *crtc_mode;
} GfMonitorCrtcMode;

struct _GfMonitorMode
{
  gchar             *id;
  GfMonitorModeSpec  spec;
  GfMonitorCrtcMode *crtc_modes;
};

typedef gboolean (* GfMonitorModeFunc) (GfMonitor          *monitor,
                                        GfMonitorMode      *mode,
                                        GfMonitorCrtcMode  *monitor_crtc_mode,
                                        gpointer            user_data,
                                        GError            **error);

#define GF_TYPE_MONITOR (gf_monitor_get_type ())
G_DECLARE_DERIVABLE_TYPE (GfMonitor, gf_monitor, GF, MONITOR, GObject)

struct _GfMonitorClass
{
  GObjectClass parent_class;

  GfOutput  * (* get_main_output)        (GfMonitor          *monitor);

  void        (* derive_layout)          (GfMonitor          *monitor,
                                          GfRectangle        *layout);

  void        (* calculate_crtc_pos)     (GfMonitor          *monitor,
                                          GfMonitorMode      *monitor_mode,
                                          GfOutput           *output,
                                          GfMonitorTransform  crtc_transform,
                                          gint               *out_x,
                                          gint               *out_y);

  gboolean    (* get_suggested_position) (GfMonitor          *monitor,
                                          gint               *width,
                                          gint               *height);
};

GfMonitorManager  *gf_monitor_get_monitor_manager        (GfMonitor                  *monitor);

gboolean           gf_monitor_is_mode_assigned           (GfMonitor                  *monitor,
                                                          GfMonitorMode              *mode);

void               gf_monitor_append_output              (GfMonitor                  *monitor,
                                                          GfOutput                   *output);

void               gf_monitor_set_winsys_id              (GfMonitor                  *monitor,
                                                          glong                       winsys_id);

void               gf_monitor_set_preferred_mode         (GfMonitor                  *monitor,
                                                          GfMonitorMode              *mode);

void               gf_monitor_generate_spec              (GfMonitor                  *monitor);

gboolean           gf_monitor_add_mode                   (GfMonitor                  *monitor,
                                                          GfMonitorMode              *monitor_mode,
                                                          gboolean                    replace);

void               gf_monitor_mode_free                  (GfMonitorMode              *monitor_mode);

gchar             *gf_monitor_mode_spec_generate_id      (GfMonitorModeSpec          *spec);

GfMonitorSpec     *gf_monitor_get_spec                   (GfMonitor                  *monitor);

gboolean           gf_monitor_is_active                  (GfMonitor                  *monitor);

GfOutput          *gf_monitor_get_main_output            (GfMonitor                  *monitor);

gboolean           gf_monitor_is_primary                 (GfMonitor                  *monitor);

gboolean           gf_monitor_supports_underscanning     (GfMonitor                  *monitor);

gboolean           gf_monitor_is_underscanning           (GfMonitor                  *monitor);

gboolean           gf_monitor_is_laptop_panel            (GfMonitor                  *monitor);

gboolean           gf_monitor_is_same_as                 (GfMonitor                  *monitor,
                                                          GfMonitor                  *other_monitor);

GList             *gf_monitor_get_outputs                (GfMonitor                  *monitor);

void               gf_monitor_get_current_resolution     (GfMonitor                  *monitor,
                                                          int                        *width,
                                                          int                        *height);

void               gf_monitor_derive_layout              (GfMonitor                  *monitor,
                                                          GfRectangle                *layout);

void               gf_monitor_get_physical_dimensions    (GfMonitor                  *monitor,
                                                          gint                       *width_mm,
                                                          gint                       *height_mm);

const gchar       *gf_monitor_get_connector              (GfMonitor                  *monitor);

const gchar       *gf_monitor_get_vendor                 (GfMonitor                  *monitor);

const gchar       *gf_monitor_get_product                (GfMonitor                  *monitor);

const gchar       *gf_monitor_get_serial                 (GfMonitor                  *monitor);

GfConnectorType    gf_monitor_get_connector_type         (GfMonitor                  *monitor);

gboolean           gf_monitor_get_suggested_position     (GfMonitor                  *monitor,
                                                          gint                       *x,
                                                          gint                       *y);

GfLogicalMonitor  *gf_monitor_get_logical_monitor        (GfMonitor                  *monitor);

GfMonitorMode     *gf_monitor_get_mode_from_id           (GfMonitor                  *monitor,
                                                          const gchar                *monitor_mode_id);

GfMonitorMode     *gf_monitor_get_mode_from_spec         (GfMonitor                  *monitor,
                                                          GfMonitorModeSpec          *monitor_mode_spec);

GfMonitorMode     *gf_monitor_get_preferred_mode         (GfMonitor                  *monitor);

GfMonitorMode     *gf_monitor_get_current_mode           (GfMonitor                  *monitor);

void               gf_monitor_derive_current_mode        (GfMonitor                  *monitor);

void               gf_monitor_set_current_mode           (GfMonitor                  *monitor,
                                                          GfMonitorMode              *mode);

GList             *gf_monitor_get_modes                  (GfMonitor                  *monitor);

void               gf_monitor_calculate_crtc_pos         (GfMonitor                  *monitor,
                                                          GfMonitorMode              *monitor_mode,
                                                          GfOutput                   *output,
                                                          GfMonitorTransform          crtc_transform,
                                                          gint                       *out_x,
                                                          gint                       *out_y);

gfloat             gf_monitor_calculate_mode_scale       (GfMonitor                  *monitor,
                                                          GfMonitorMode              *monitor_mode);

gfloat            *gf_monitor_calculate_supported_scales (GfMonitor                  *monitor,
                                                          GfMonitorMode              *monitor_mode,
                                                          GfMonitorScalesConstraint   constraints,
                                                          gint                       *n_supported_scales);

const gchar       *gf_monitor_mode_get_id                (GfMonitorMode              *monitor_mode);

GfMonitorModeSpec *gf_monitor_mode_get_spec              (GfMonitorMode              *monitor_mode);

void               gf_monitor_mode_get_resolution        (GfMonitorMode              *monitor_mode,
                                                          gint                       *width,
                                                          gint                       *height);

gfloat             gf_monitor_mode_get_refresh_rate      (GfMonitorMode              *monitor_mode);

GfCrtcModeFlag     gf_monitor_mode_get_flags             (GfMonitorMode              *monitor_mode);

gboolean           gf_monitor_mode_foreach_crtc          (GfMonitor                  *monitor,
                                                          GfMonitorMode              *mode,
                                                          GfMonitorModeFunc           func,
                                                          gpointer                    user_data,
                                                          GError                    **error);

gboolean           gf_monitor_mode_foreach_output        (GfMonitor                  *monitor,
                                                          GfMonitorMode              *mode,
                                                          GfMonitorModeFunc           func,
                                                          gpointer                    user_data,
                                                          GError                    **error);

gboolean           gf_verify_monitor_mode_spec           (GfMonitorModeSpec          *mode_spec,
                                                          GError                    **error);

G_END_DECLS

#endif
