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
 * - src/backends/meta-logical-monitor.h
 */

#ifndef GF_LOGICAL_MONITOR_PRIVATE_H
#define GF_LOGICAL_MONITOR_PRIVATE_H

#include "gf-direction.h"
#include "gf-logical-monitor-config-private.h"
#include "gf-monitor-config-manager-private.h"
#include "gf-monitor-manager-private.h"
#include "gf-monitor-private.h"

G_BEGIN_DECLS

#define GF_TYPE_LOGICAL_MONITOR (gf_logical_monitor_get_type ())
G_DECLARE_FINAL_TYPE (GfLogicalMonitor, gf_logical_monitor,
                      GF, LOGICAL_MONITOR, GObject)

struct _GfLogicalMonitor
{
  GObject             parent;

  gint                number;
  GfRectangle         rect;
  gboolean            is_primary;
  gboolean            is_presentation;
  gboolean            in_fullscreen;
  gfloat              scale;
  GfMonitorTransform  transform;

  /* The primary or first output for this monitor, 0 if we can't figure out.
   * It can be matched to a winsys_id of a GfOutput.
   *
   * This is used as an opaque token on reconfiguration when switching from
   * clone to extened, to decide on what output the windows should go next
   * (it's an attempt to keep windows on the same monitor, and preferably on
   * the primary one).
   */
  glong               winsys_id;

  GList              *monitors;
};

typedef void (* GfLogicalMonitorCrtcFunc) (GfLogicalMonitor *logical_monitor,
                                           GfMonitor        *monitor,
                                           GfOutput         *output,
                                           GfCrtc           *crtc,
                                           gpointer          user_data);


GfLogicalMonitor   *gf_logical_monitor_new           (GfMonitorManager       *monitor_manager,
                                                      GfLogicalMonitorConfig *logical_monitor_config,
                                                      gint                    monitor_number);

GfLogicalMonitor   *gf_logical_monitor_new_derived   (GfMonitorManager       *monitor_manager,
                                                      GfMonitor              *monitor,
                                                      GfRectangle            *layout,
                                                      gfloat                  scale,
                                                      gint                    monitor_number);

void                gf_logical_monitor_add_monitor   (GfLogicalMonitor       *logical_monitor,
                                                      GfMonitor              *monitor);

gboolean            gf_logical_monitor_is_primary    (GfLogicalMonitor       *logical_monitor);

void                gf_logical_monitor_make_primary  (GfLogicalMonitor       *logical_monitor);

gfloat              gf_logical_monitor_get_scale     (GfLogicalMonitor       *logical_monitor);

GfMonitorTransform  gf_logical_monitor_get_transform (GfLogicalMonitor       *logical_monitor);

GfRectangle         gf_logical_monitor_get_layout    (GfLogicalMonitor       *logical_monitor);

GList              *gf_logical_monitor_get_monitors  (GfLogicalMonitor       *logical_monitor);

gboolean            gf_logical_monitor_has_neighbor  (GfLogicalMonitor       *monitor,
                                                      GfLogicalMonitor       *neighbor,
                                                      GfDirection             direction);

void                gf_logical_monitor_foreach_crtc  (GfLogicalMonitor         *logical_monitor,
                                                      GfLogicalMonitorCrtcFunc  func,
                                                      gpointer                  user_data);


G_END_DECLS

#endif
