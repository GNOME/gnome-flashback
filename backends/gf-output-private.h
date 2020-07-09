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
#include <stdint.h>

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
} GfOutputAssignment;

struct _GfOutput
{
  GObject              parent;

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

  gint                 backlight_min;
  gint                 backlight_max;

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

uint64_t            gf_output_get_id                    (GfOutput                 *self);

GfGpu              *gf_output_get_gpu                   (GfOutput                 *self);

const char         *gf_output_get_name                  (GfOutput                 *self);

void                gf_output_assign_crtc               (GfOutput                 *self,
                                                         GfCrtc                   *crtc,
                                                         const GfOutputAssignment *output_assignment);

void                gf_output_unassign_crtc             (GfOutput                 *self);

GfCrtc             *gf_output_get_assigned_crtc         (GfOutput                 *self);

void                gf_output_parse_edid                (GfOutput                 *self,
                                                         GBytes                   *edid);

gboolean            gf_output_is_laptop                 (GfOutput                 *self);

GfMonitorTransform  gf_output_logical_to_crtc_transform (GfOutput                 *self,
                                                         GfMonitorTransform        transform);

GfMonitorTransform  gf_output_crtc_to_logical_transform (GfOutput                 *self,
                                                         GfMonitorTransform        transform);

gboolean            gf_output_is_primary                (GfOutput                 *self);

gboolean            gf_output_is_presentation           (GfOutput                 *self);

gboolean            gf_output_is_underscanning          (GfOutput                 *self);

void                gf_output_set_backlight             (GfOutput                 *self,
                                                         int                       backlight);

int                 gf_output_get_backlight             (GfOutput                 *self);

static inline GfOutputAssignment *
gf_find_output_assignment (GfOutputAssignment **outputs,
                           unsigned int         n_outputs,
                           GfOutput            *output)
{
  unsigned int i;

  for (i = 0; i < n_outputs; i++)
    {
      GfOutputAssignment *output_assignment;

      output_assignment = outputs[i];

      if (output == output_assignment->output)
        return output_assignment;
    }

  return NULL;
}

G_END_DECLS

#endif
