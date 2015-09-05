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

#ifndef FLASHBACK_SESSION_H
#define FLASHBACK_SESSION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define FLASHBACK_TYPE_SESSION         (flashback_session_get_type ())
#define FLASHBACK_SESSION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), FLASHBACK_TYPE_SESSION, FlashbackSession))
#define FLASHBACK_IS_SESSION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), FLASHBACK_TYPE_SESSION))
#define FLASHBACK_SESSION_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    FLASHBACK_TYPE_SESSION, FlashbackSessionClass))
#define FLASHBACK_IS_SESSION_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    FLASHBACK_TYPE_SESSION))
#define FLASHBACK_SESSION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  FLASHBACK_TYPE_SESSION, FlashbackSessionClass))

typedef struct _FlashbackSession        FlashbackSession;
typedef struct _FlashbackSessionClass   FlashbackSessionClass;
typedef struct _FlashbackSessionPrivate FlashbackSessionPrivate;

struct _FlashbackSession {
	GObject                  parent;
	FlashbackSessionPrivate *priv;
};

struct _FlashbackSessionClass {
	GObjectClass parent_class;
};

GType             flashback_session_get_type        (void);

FlashbackSession *flashback_session_new             (gboolean          replace);

gboolean          flashback_session_set_environment (FlashbackSession *session,
                                                     const gchar      *name,
                                                     const gchar      *value);
gboolean          flashback_session_register_client (FlashbackSession *session);

G_END_DECLS

#endif
