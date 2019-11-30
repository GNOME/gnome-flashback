/*
 * Copyright (C) 2014 - 2019 Alberts Muktupāvels
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

#ifndef GF_KEYBINDINGS_H
#define GF_KEYBINDINGS_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
  GF_KEYBINDING_NONE,
  GF_KEYBINDING_ISO_NEXT_GROUP,
  GF_KEYBINDING_ISO_FIRST_GROUP,
  GF_KEYBINDING_ISO_LAST_GROUP
} GfKeybindingType;

#define GF_TYPE_KEYBINDINGS gf_keybindings_get_type ()
G_DECLARE_FINAL_TYPE (GfKeybindings, gf_keybindings, GF, KEYBINDINGS, GObject)

GfKeybindings   *gf_keybindings_new            (void);

void             gf_keybindings_grab_iso_group (GfKeybindings *keybindings,
                                                const gchar   *iso_group);

guint            gf_keybindings_grab           (GfKeybindings *keybindings,
                                                const gchar   *accelerator);

gboolean         gf_keybindings_ungrab         (GfKeybindings *keybindings,
                                                guint          action);

guint            gf_keybindings_get_keyval     (GfKeybindings *keybindings,
                                                guint          action);

GdkModifierType  gf_keybindings_get_modifiers  (GfKeybindings *keybindings,
                                                guint          action);

G_END_DECLS

#endif
