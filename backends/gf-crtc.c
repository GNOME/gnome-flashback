/*
 * Copyright (C) 2017 Red Hat
 * Copyright (C) 2018-2019 Alberts Muktupāvels
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
#include "gf-crtc-private.h"

G_DEFINE_TYPE (GfCrtc, gf_crtc, G_TYPE_OBJECT)

static void
gf_crtc_finalize (GObject *object)
{
  GfCrtc *crtc;

  crtc = GF_CRTC (object);

  if (crtc->driver_notify)
    crtc->driver_notify (crtc);

  G_OBJECT_CLASS (gf_crtc_parent_class)->finalize (object);
}

static void
gf_crtc_class_init (GfCrtcClass *crtc_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (crtc_class);

  object_class->finalize = gf_crtc_finalize;
}

static void
gf_crtc_init (GfCrtc *crtc)
{
}

GfGpu *
gf_crtc_get_gpu (GfCrtc *crtc)
{
  return crtc->gpu;
}
