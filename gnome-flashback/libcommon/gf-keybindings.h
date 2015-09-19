/*
 * Copyright (C) 2014 - 2015 Alberts Muktupāvels
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

#ifndef FLASHBACK_KEY_BINDINGS_H
#define FLASHBACK_KEY_BINDINGS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define FLASHBACK_TYPE_KEY_BINDINGS         (flashback_key_bindings_get_type ())
#define FLASHBACK_KEY_BINDINGS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), FLASHBACK_TYPE_KEY_BINDINGS, FlashbackKeyBindings))
#define FLASHBACK_KEY_BINDINGS_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    FLASHBACK_TYPE_KEY_BINDINGS, FlashbackKeyBindingsClass))
#define FLASHBACK_IS_KEY_BINDINGS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), FLASHBACK_TYPE_KEY_BINDINGS))
#define FLASHBACK_IS_KEY_BINDINGS_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    FLASHBACK_TYPE_KEY_BINDINGS))
#define FLASHBACK_KEY_BINDINGS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o),   FLASHBACK_TYPE_KEY_BINDINGS, FlashbackKeyBindingsClass))

typedef struct _FlashbackKeyBindings        FlashbackKeyBindings;
typedef struct _FlashbackKeyBindingsClass   FlashbackKeyBindingsClass;
typedef struct _FlashbackKeyBindingsPrivate FlashbackKeyBindingsPrivate;

struct _FlashbackKeyBindings {
	GObject                      parent;
	FlashbackKeyBindingsPrivate *priv;
};

struct _FlashbackKeyBindingsClass {
    GObjectClass parent_class;

	void (*binding_activated) (FlashbackKeyBindings *bindings,
	                           guint                 action,
	                           GVariant             *parameters);
};

GType                 flashback_key_bindings_get_type (void);

FlashbackKeyBindings *flashback_key_bindings_new      (void);

guint                 flashback_key_bindings_grab     (FlashbackKeyBindings *bindings,
                                                       const gchar          *accelerator);
gboolean              flashback_key_bindings_ungrab   (FlashbackKeyBindings *bindings,
                                                       guint                 action);

G_END_DECLS

#endif
