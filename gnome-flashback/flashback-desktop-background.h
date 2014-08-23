/* 
 * Copyright (C) 2014 Alberts Muktupāvels
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

#ifndef FLASHBACK_DESKTOP_BACKGROUND_H
#define FLASHBACK_DESKTOP_BACKGROUND_H

#include <glib-object.h>

G_BEGIN_DECLS

#define FLASHBACK_TYPE_DESKTOP_BACKGROUND (flashback_desktop_background_get_type ())
#define FLASHBACK_DESKTOP_BACKGROUND(o)   (G_TYPE_CHECK_INSTANCE_CAST ((o),  FLASHBACK_TYPE_DESKTOP_BACKGROUND, FlashbackDesktopBackground))

typedef struct _FlashbackDesktopBackground        FlashbackDesktopBackground;
typedef struct _FlashbackDesktopBackgroundClass   FlashbackDesktopBackgroundClass;
typedef struct _FlashbackDesktopBackgroundPrivate FlashbackDesktopBackgroundPrivate;

struct _FlashbackDesktopBackground {
	GObject                            parent;
	FlashbackDesktopBackgroundPrivate *priv;
};

struct _FlashbackDesktopBackgroundClass {
    GObjectClass parent_class;
};

GType                       flashback_desktop_background_get_type (void);
FlashbackDesktopBackground *flashback_desktop_background_new      (void);

G_END_DECLS

#endif
