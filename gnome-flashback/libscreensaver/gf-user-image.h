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

#ifndef GF_USER_IMAGE_H
#define GF_USER_IMAGE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GF_TYPE_USER_IMAGE (gf_user_image_get_type ())
G_DECLARE_FINAL_TYPE (GfUserImage, gf_user_image, GF, USER_IMAGE, GtkImage)

GtkWidget *gf_user_image_new (void);

G_END_DECLS

#endif
