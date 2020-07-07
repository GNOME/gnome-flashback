/*
 * Copyright (C) 2001 Havoc Pennington
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
 * - src/backends/meta-monitor-manager-private.h
 */

#ifndef GF_OUTPUT_PRIVATE_H
#define GF_OUTPUT_PRIVATE_H

#include <glib-object.h>

#include "gf-gpu-private.h"
#include "gf-monitor-manager-enums-private.h"
#include "gf-monitor-manager-types-private.h"

G_BEGIN_DECLS

typedef struct
{
  guint32 group_id;
  guint32 flags;
  guint32 max_h_tiles;
  guint32 max_v_tiles;
  guint32 loc_h_tile;
  guint32 loc_v_tile;
  guint32 tile_w;
  guint32 tile_h;
} GfTileInfo;

typedef struct
{
  GfOutput *output;
  gboolean  is_primary;
  gboolean  is_presentation;
  gboolean  is_underscanning;
} GfOutputInfo;

struct _GfOutput
{
  GObject              parent;

  /* The low-level ID of this output, used to apply back configuration */
  glong                winsys_id;
  gchar               *name;
  gchar               *vendor;
  gchar               *product;
  gchar               *serial;
  gint                 width_mm;
  gint                 height_mm;

  GfConnectorType      connector_type;
  GfMonitorTransform   panel_orientation_transform;

  GfCrtcMode          *preferred_mode;
  GfCrtcMode         **modes;
  guint                n_modes;

  GfCrtc             **possible_crtcs;
  guint                n_possible_crtcs;

  GfOutput           **possible_clones;
  guint                n_possible_clones;

  gint                 backlight;
  gint                 backlight_min;
  gint                 backlight_max;

  /* The low-level bits used to build the high-level info in GfLogicalMonitor */
  gboolean             is_primary;
  gboolean             is_presentation;
  gboolean             is_underscanning;
  gboolean             supports_underscanning;

  gpointer             driver_private;
  GDestroyNotify       driver_notify;

  /* Get a new preferred mode on hotplug events, to handle
   * dynamic guest resizing
   */
  gboolean             hotplug_mode_update;
  gint                 suggested_x;
  gint                 suggested_y;

  GfTileInfo           tile_info;
};

#define GF_TYPE_OUTPUT (gf_output_get_type ())
G_DECLARE_FINAL_TYPE (GfOutput, gf_output, GF, OUTPUT, GObject)

GfGpu              *gf_output_get_gpu                   (GfOutput           *output);

void                gf_output_assign_crtc               (GfOutput           *output,
                                                         GfCrtc             *crtc);

void                gf_output_unassign_crtc             (GfOutput           *output);

GfCrtc             *gf_output_get_assigned_crtc         (GfOutput           *output);

void                gf_output_parse_edid                (GfOutput           *output,
                                                         GBytes             *edid);

gboolean            gf_output_is_laptop                 (GfOutput           *output);

GfMonitorTransform  gf_output_logical_to_crtc_transform (GfOutput           *output,
                                                         GfMonitorTransform  transform);

GfMonitorTransform  gf_output_crtc_to_logical_transform (GfOutput           *output,
                                                         GfMonitorTransform  transform);

G_END_DECLS

#endif
