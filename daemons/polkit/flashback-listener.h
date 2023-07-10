/*
 * Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2015 Alberts Muktupāvels
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     David Zeuthen <davidz@redhat.com>
 */

#ifndef FLASHBACK_LISTENER_H
#define FLASHBACK_LISTENER_H

#include <polkitagent/polkitagent.h>

G_BEGIN_DECLS

#define FLASHBACK_TYPE_LISTENER         (flashback_listener_get_type())
#define FLASHBACK_LISTENER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o),      \
                                         FLASHBACK_TYPE_LISTENER,              \
                                         FlashbackListener))
#define FLASHBACK_IS_LISTENER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o),      \
                                         FLASHBACK_TYPE_LISTENER))
#define FLASHBACK_LISTENER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k),          \
                                         FLASHBACK_TYPE_LISTENER,              \
                                         FlashbackListenerClass))
#define FLASHBACK_IS_LISTENER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k),         \
                                         FLASHBACK_TYPE_LISTENER))
#define FLASHBACK_LISTENER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),       \
                                         FLASHBACK_TYPE_LISTENER,              \
                                         FlashbackListenerClass))

typedef struct _FlashbackListener      FlashbackListener;
typedef struct _FlashbackListenerClass FlashbackListenerClass;

struct _FlashbackListenerClass
{
  PolkitAgentListenerClass parent_class;
};

GType                flashback_listener_get_type (void) G_GNUC_CONST;
PolkitAgentListener *flashback_listener_new      (void);

G_END_DECLS

#endif
