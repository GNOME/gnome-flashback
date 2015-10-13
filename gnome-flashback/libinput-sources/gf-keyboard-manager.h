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

#ifndef GF_KEYBOARD_MANAGER_H
#define GF_KEYBOARD_MANAGER_H

#include <glib-object.h>
#include <libgnome-desktop/gnome-xkb-info.h>

G_BEGIN_DECLS

#define GF_TYPE_KEYBOARD_MANAGER gf_keyboard_manager_get_type ()
G_DECLARE_FINAL_TYPE (GfKeyboardManager, gf_keyboard_manager,
                      GF, KEYBOARD_MANAGER, GObject)

GfKeyboardManager *gf_keyboard_manager_new                (void);

GnomeXkbInfo      *gf_keyboard_manager_get_xkb_info       (GfKeyboardManager  *manager);

void               gf_keyboard_manager_set_xkb_options    (GfKeyboardManager  *manager,
                                                           gchar             **options);

void               gf_keyboard_manager_set_user_layouts   (GfKeyboardManager  *manager,
                                                           gchar             **ids);

void               gf_keyboard_manager_apply              (GfKeyboardManager  *manager,
                                                           const gchar        *id);

void               gf_keyboard_manager_reapply            (GfKeyboardManager  *manager);

gboolean           gf_keyboard_manager_grab               (GfKeyboardManager  *manager,
                                                           guint32             timestamp);

void               gf_keyboard_manager_ungrab             (GfKeyboardManager  *manager,
                                                           guint32             timestamp);

const gchar       *gf_keyboard_manager_get_default_layout (GfKeyboardManager  *manager);

G_END_DECLS

#endif
