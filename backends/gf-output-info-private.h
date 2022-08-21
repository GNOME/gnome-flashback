/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2017-2020 Alberts Muktupāvels
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
 */

#ifndef GF_OUTPUT_INFO_PRIVATE_H
#define GF_OUTPUT_INFO_PRIVATE_H

#include <glib-object.h>
#include <stdint.h>

#include "gf-monitor-manager-enums-private.h"
#include "gf-monitor-manager-types-private.h"
#include "gf-monitor-transform.h"

G_BEGIN_DECLS

typedef struct
{
  uint32_t group_id;
  uint32_t flags;
  uint32_t max_h_tiles;
  uint32_t max_v_tiles;
  uint32_t loc_h_tile;
  uint32_t loc_v_tile;
  uint32_t tile_w;
  uint32_t tile_h;
} GfTileInfo;

typedef struct
{
  grefcount            ref_count;

  char                *name;
  char                *vendor;
  char                *product;
  char                *serial;
  int                  width_mm;
  int                  height_mm;

  GfConnectorType      connector_type;
  GfMonitorTransform   panel_orientation_transform;

  GfCrtcMode          *preferred_mode;
  GfCrtcMode         **modes;
  guint                n_modes;

  GfCrtc             **possible_crtcs;
  guint                n_possible_crtcs;

  GfOutput           **possible_clones;
  guint                n_possible_clones;

  int                  backlight_min;
  int                  backlight_max;

  gboolean             supports_underscanning;
  gboolean             supports_color_transform;

  unsigned int         max_bpc_min;
  unsigned int         max_bpc_max;

  /* Get a new preferred mode on hotplug events, to handle
   * dynamic guest resizing
   */
  gboolean             hotplug_mode_update;
  int                  suggested_x;
  int                  suggested_y;

  GfTileInfo           tile_info;
} GfOutputInfo;

#define GF_TYPE_OUTPUT_INFO (gf_output_info_get_type ())

GType         gf_output_info_get_type   (void);

GfOutputInfo *gf_output_info_new        (void);

GfOutputInfo *gf_output_info_ref        (GfOutputInfo *self);

void          gf_output_info_unref      (GfOutputInfo *self);

void          gf_output_info_parse_edid (GfOutputInfo *self,
                                         GBytes       *edid);

G_END_DECLS

#endif
