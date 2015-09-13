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

#ifndef DESKTOP_WINDOW_H
#define DESKTOP_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DESKTOP_WINDOW_TYPE     (desktop_window_get_type ())
#define DESKTOP_WINDOW(o)       (G_TYPE_CHECK_INSTANCE_CAST ((o), DESKTOP_WINDOW_TYPE, DesktopWindow))
#define DESKTOP_WINDOW_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), DESKTOP_WINDOW_TYPE, DesktopWindowClass))

typedef struct _DesktopWindow        DesktopWindow;
typedef struct _DesktopWindowClass   DesktopWindowClass;

struct _DesktopWindow {
	GtkWindow parent;
};

struct _DesktopWindowClass {
	GtkWindowClass parent_class;
};

GType      desktop_window_get_type (void);
GtkWidget *desktop_window_new      (void);

G_END_DECLS

#endif
