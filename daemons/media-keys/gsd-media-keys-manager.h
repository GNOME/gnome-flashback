/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __GSD_MEDIA_KEYS_MANAGER_H
#define __GSD_MEDIA_KEYS_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GSD_TYPE_MEDIA_KEYS_MANAGER         (gsd_media_keys_manager_get_type ())

G_DECLARE_DERIVABLE_TYPE (GsdMediaKeysManager, gsd_media_keys_manager, GSD, MEDIA_KEYS_MANAGER, GObject)

struct _GsdMediaKeysManagerClass
{
        GObjectClass   parent_class;
};

GsdMediaKeysManager * gsd_media_keys_manager_new                       (void);
gboolean              gsd_media_keys_manager_start                     (GsdMediaKeysManager *manager,
                                                                        GError             **error);
void                  gsd_media_keys_manager_stop                      (GsdMediaKeysManager *manager);


G_END_DECLS

#endif /* __GSD_MEDIA_KEYS_MANAGER_H */
