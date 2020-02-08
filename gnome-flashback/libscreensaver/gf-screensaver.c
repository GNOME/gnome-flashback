/*
 * Copyright (C) 2016-2020 Alberts Muktupāvels
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

#include "gf-screensaver.h"

struct _GfScreensaver
{
  GObject parent;
};

G_DEFINE_TYPE (GfScreensaver, gf_screensaver, G_TYPE_OBJECT)

static void
gf_screensaver_class_init (GfScreensaverClass *screensaver_class)
{
}

static void
gf_screensaver_init (GfScreensaver *screensaver)
{
}

GfScreensaver *
gf_screensaver_new (void)
{
  return g_object_new (GF_TYPE_SCREENSAVER, NULL);
}

void
gf_screensaver_set_input_sources (GfScreensaver  *self,
                                  GfInputSources *input_sources)
{
}
