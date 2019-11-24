/*
 * Copyright (C) 2019 Alberts Muktupāvels
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
#include "gf-background-utils.h"

cairo_surface_t *
gf_background_surface_create (GdkDisplay *display,
                              GnomeBG    *bg,
                              GdkWindow  *window,
                              int         width,
                              int         height)
{
  return gnome_bg_create_surface (bg, window, width, height, TRUE);
}

cairo_surface_t *
gf_background_surface_get_from_root (GdkDisplay *display)
{
  GdkScreen *screen;

  screen = gdk_display_get_default_screen (display);

  return gnome_bg_get_surface_from_root (screen);
}

void
gf_background_surface_set_as_root (GdkDisplay      *display,
                                   cairo_surface_t *surface)
{
  GdkScreen *screen;

  screen = gdk_display_get_default_screen (display);

  gnome_bg_set_surface_as_root (screen, surface);
}
