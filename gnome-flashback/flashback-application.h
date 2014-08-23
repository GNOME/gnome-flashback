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

#ifndef FLASHBACK_APPLICATION_H
#define FLASHBACK_APPLICATION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define FLASHBACK_TYPE_APPLICATION (flashback_application_get_type ())
#define FLASHBACK_APPLICATION(o)   (G_TYPE_CHECK_INSTANCE_CAST ((o),  FLASHBACK_TYPE_APPLICATION, FlashbackApplication))

typedef struct _FlashbackApplication        FlashbackApplication;
typedef struct _FlashbackApplicationClass   FlashbackApplicationClass;
typedef struct _FlashbackApplicationPrivate FlashbackApplicationPrivate;

struct _FlashbackApplication {
	GtkApplication               parent;
	FlashbackApplicationPrivate *priv;
};

struct _FlashbackApplicationClass {
	GtkApplicationClass parent;
};

GType                 flashback_application_get_type (void);
FlashbackApplication *flashback_application_new      (void);

G_END_DECLS

#endif
