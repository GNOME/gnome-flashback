/*
 * Copyright (C) 2017-2020 Red Hat
 * Copyright (C) 2020 Alberts Muktupāvels
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

#include "config.h"
#include "gf-crtc-mode-info-private.h"

G_DEFINE_BOXED_TYPE (GfCrtcModeInfo,
                     gf_crtc_mode_info,
                     gf_crtc_mode_info_ref,
                     gf_crtc_mode_info_unref)

GfCrtcModeInfo *
gf_crtc_mode_info_new (void)
{
  GfCrtcModeInfo *self;

  self = g_new0 (GfCrtcModeInfo, 1);
  g_ref_count_init (&self->ref_count);

  return self;
}

GfCrtcModeInfo *
gf_crtc_mode_info_ref (GfCrtcModeInfo *self)
{
  g_ref_count_inc (&self->ref_count);

  return self;
}

void
gf_crtc_mode_info_unref (GfCrtcModeInfo *self)
{
  if (g_ref_count_dec (&self->ref_count))
    g_free (self);
}
