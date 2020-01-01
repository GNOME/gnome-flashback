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

#ifndef SI_INDICATOR_H
#define SI_INDICATOR_H

#include <libgnome-panel/gp-applet.h>

G_BEGIN_DECLS

#define SI_TYPE_INDICATOR (si_indicator_get_type ())
G_DECLARE_DERIVABLE_TYPE (SiIndicator, si_indicator, SI, INDICATOR, GObject)

struct _SiIndicatorClass
{
  GObjectClass parent_class;
};

GpApplet  *si_indicator_get_applet        (SiIndicator *self);

GtkWidget *si_indicator_get_menu_item     (SiIndicator *self);

void       si_indicator_set_icon_name     (SiIndicator *self,
                                           const char  *icon_name);

void       si_indicator_set_icon_filename (SiIndicator *self,
                                           const char  *filename);

void       si_indicator_set_icon          (SiIndicator *self,
                                           GIcon       *icon);

G_END_DECLS

#endif
