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

#ifndef GF_INFO_BAR_H
#define GF_INFO_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GF_TYPE_INFO_BAR (gf_info_bar_get_type ())
G_DECLARE_FINAL_TYPE (GfInfoBar, gf_info_bar, GF, INFO_BAR, GtkInfoBar)

GtkWidget *gf_info_bar_new          (void);

void       gf_info_bar_show_message (GfInfoBar  *self,
                                     const char *summary,
                                     const char *body,
                                     const char *icon);

G_END_DECLS

#endif
