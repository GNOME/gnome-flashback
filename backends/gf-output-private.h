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

#ifndef GF_OUTPUT_PRIVATE_H
#define GF_OUTPUT_PRIVATE_H

#include <glib-object.h>
#include <stdint.h>

#include "gf-gpu-private.h"
#include "gf-output-info-private.h"

G_BEGIN_DECLS

typedef struct _GfOutputCtm
{
  uint64_t matrix[9];
} GfOutputCtm;

typedef struct
{
  GfOutput *output;
  gboolean  is_primary;
  gboolean  is_presentation;
  gboolean  is_underscanning;
  gboolean  has_max_bpc;
  unsigned  int max_bpc;
} GfOutputAssignment;

#define GF_TYPE_OUTPUT (gf_output_get_type ())
G_DECLARE_DERIVABLE_TYPE (GfOutput, gf_output, GF, OUTPUT, GObject)

struct _GfOutputClass
{
  GObjectClass parent_class;
};

uint64_t            gf_output_get_id                    (GfOutput                 *self);

GfGpu              *gf_output_get_gpu                   (GfOutput                 *self);

const GfOutputInfo *gf_output_get_info                  (GfOutput                 *self);

GfMonitor          *gf_output_get_monitor               (GfOutput                 *self);

void                gf_output_set_monitor               (GfOutput                 *self,
                                                         GfMonitor                *monitor);

void                gf_output_unset_monitor             (GfOutput                 *self);

const char         *gf_output_get_name                  (GfOutput                 *self);

void                gf_output_assign_crtc               (GfOutput                 *self,
                                                         GfCrtc                   *crtc,
                                                         const GfOutputAssignment *output_assignment);

void                gf_output_unassign_crtc             (GfOutput                 *self);

GfCrtc             *gf_output_get_assigned_crtc         (GfOutput                 *self);

gboolean            gf_output_is_laptop                 (GfOutput                 *self);

GfMonitorTransform  gf_output_logical_to_crtc_transform (GfOutput                 *self,
                                                         GfMonitorTransform        transform);

GfMonitorTransform  gf_output_crtc_to_logical_transform (GfOutput                 *self,
                                                         GfMonitorTransform        transform);

gboolean            gf_output_is_primary                (GfOutput                 *self);

gboolean            gf_output_is_presentation           (GfOutput                 *self);

gboolean            gf_output_is_underscanning          (GfOutput                 *self);

gboolean            gf_output_get_max_bpc               (GfOutput                 *self,
                                                         unsigned int             *max_bpc);

void                gf_output_set_backlight             (GfOutput                 *self,
                                                         int                       backlight);

int                 gf_output_get_backlight             (GfOutput                 *self);

void                gf_output_add_possible_clone        (GfOutput                 *self,
                                                         GfOutput                 *possible_clone);

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
