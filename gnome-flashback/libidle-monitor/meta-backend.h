/*
 * Copyright (C) 2014 Red Hat
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
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#ifndef META_BACKEND_H
#define META_BACKEND_H

#include <glib-object.h>
#include "meta-idle-monitor.h"

#define META_TYPE_BACKEND         (meta_backend_get_type ())
#define META_BACKEND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), META_TYPE_BACKEND, MetaBackend))
#define META_BACKEND_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k),    META_TYPE_BACKEND, MetaBackendClass))
#define META_IS_BACKEND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), META_TYPE_BACKEND))
#define META_IS_BACKEND_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k),    META_TYPE_BACKEND))
#define META_BACKEND_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  META_TYPE_BACKEND, MetaBackendClass))

typedef struct _MetaBackend      MetaBackend;
typedef struct _MetaBackendClass MetaBackendClass;

struct _MetaBackend {
	GObject          parent;

	MetaIdleMonitor *device_monitors[256];
	int              device_id_max;
};

struct _MetaBackendClass {
	GObjectClass parent_class;
};

GType            meta_backend_get_type         (void);
MetaBackend     *meta_get_backend              (void);
MetaIdleMonitor *meta_backend_get_idle_monitor (MetaBackend *backend,
                                                int          device_id);

#endif /* META_BACKEND_H */
