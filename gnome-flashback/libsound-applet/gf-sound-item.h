/*
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
 */

#ifndef GF_SOUND_ITEM_H
#define GF_SOUND_ITEM_H

#include <glib-object.h>

#include "gvc-mixer-stream.h"
#include "libstatus-notifier/sn-item.h"

G_BEGIN_DECLS

#define GF_TYPE_SOUND_ITEM gf_sound_item_get_type ()
G_DECLARE_FINAL_TYPE (GfSoundItem, gf_sound_item, GF, SOUND_ITEM, SnItem)

GfSoundItem *gf_sound_item_new              (SnItemCategory   category,
                                             const gchar     *id,
                                             const gchar     *title,
                                             const gchar     *display_name,
                                             const gchar    **icon_names);

void         gf_sound_item_set_mixer_stream (GfSoundItem     *item,
                                             GvcMixerStream  *stream);

G_END_DECLS

#endif
