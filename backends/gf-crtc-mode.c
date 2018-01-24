/*
 * Copyright (C) 2017 Red Hat
 * Copyright (C) 2018 Alberts Muktupāvels
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

G_DEFINE_TYPE (GfCrtcMode, gf_crtc_mode, G_TYPE_OBJECT)

static void
gf_crtc_mode_finalize (GObject *object)
{
  GfCrtcMode *crtc_mode;

  crtc_mode = GF_CRTC_MODE (object);

  if (crtc_mode->driver_notify)
    crtc_mode->driver_notify (crtc_mode);

  G_OBJECT_CLASS (gf_crtc_mode_parent_class)->finalize (object);
}

static void
gf_crtc_mode_class_init (GfCrtcModeClass *crtc_mode_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (crtc_mode_class);

  object_class->finalize = gf_crtc_mode_finalize;
}

static void
gf_crtc_mode_init (GfCrtcMode *crtc_mode)
{
}
