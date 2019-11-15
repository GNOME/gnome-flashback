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

#ifndef GF_RENAME_POPOVER_H
#define GF_RENAME_POPOVER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GF_TYPE_RENAME_POPOVER (gf_rename_popover_get_type ())
G_DECLARE_FINAL_TYPE (GfRenamePopover, gf_rename_popover,
                      GF, RENAME_POPOVER, GtkPopover)

GtkWidget *gf_rename_popover_new       (GtkWidget       *relative_to,
                                        GFileType        file_type,
                                        const char      *name);

void       gf_rename_popover_set_valid (GfRenamePopover *self,
                                        gboolean         valid,
                                        const char      *message);

char      *gf_rename_popover_get_name  (GfRenamePopover *self);

G_END_DECLS

#endif
