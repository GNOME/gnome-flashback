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

#ifndef DESKTOP_BACKGROUND_H
#define DESKTOP_BACKGROUND_H

#include <glib-object.h>

G_BEGIN_DECLS

#define DESKTOP_BACKGROUND_TYPE     (desktop_background_get_type ())
#define DESKTOP_BACKGROUND(o)       (G_TYPE_CHECK_INSTANCE_CAST ((o), DESKTOP_BACKGROUND_TYPE, DesktopBackground))
#define DESKTOP_BACKGROUND_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), DESKTOP_BACKGROUND_TYPE, DesktopBackgroundClass))

typedef struct _DesktopBackground        DesktopBackground;
typedef struct _DesktopBackgroundClass   DesktopBackgroundClass;
typedef struct _DesktopBackgroundPrivate DesktopBackgroundPrivate;

struct _DesktopBackground {
	GObject                   parent;
	DesktopBackgroundPrivate *priv;
};

struct _DesktopBackgroundClass {
    GObjectClass parent_class;
};

GType              desktop_background_get_type (void);
DesktopBackground *desktop_background_new      (void);

G_END_DECLS

#endif
