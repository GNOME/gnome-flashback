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

#ifndef FLASHBACK_KEY_GRABBER_H
#define FLASHBACK_KEY_GRABBER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define FLASHBACK_TYPE_KEY_GRABBER         (flashback_key_grabber_get_type ())
#define FLASHBACK_KEY_GRABBER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), FLASHBACK_TYPE_KEY_GRABBER, FlashbackKeyGrabber))
#define FLASHBACK_KEY_GRABBER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    FLASHBACK_TYPE_KEY_GRABBER, FlashbackKeyGrabberClass))
#define FLASHBACK_IS_KEY_GRABBER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), FLASHBACK_TYPE_KEY_GRABBER))
#define FLASHBACK_IS_KEY_GRABBER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    FLASHBACK_TYPE_KEY_GRABBER))
#define FLASHBACK_KEY_GRABBER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o),   FLASHBACK_TYPE_KEY_GRABBER, FlashbackKeyGrabberClass))

typedef struct _FlashbackKeyGrabber        FlashbackKeyGrabber;
typedef struct _FlashbackKeyGrabberClass   FlashbackKeyGrabberClass;
typedef struct _FlashbackKeyGrabberPrivate FlashbackKeyGrabberPrivate;

struct _FlashbackKeyGrabber {
	GObject                     parent;
	FlashbackKeyGrabberPrivate *priv;
};

struct _FlashbackKeyGrabberClass {
    GObjectClass parent_class;
};

GType                flashback_key_grabber_get_type (void);

FlashbackKeyGrabber *flashback_key_grabber_new      (void);

G_END_DECLS

#endif
