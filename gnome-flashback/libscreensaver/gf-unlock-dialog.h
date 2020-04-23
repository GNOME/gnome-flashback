/*
 * Copyright (C) 2020 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOgf_unlock_dialog_newSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GF_UNLOCK_DIALOG_H
#define GF_UNLOCK_DIALOG_H

#include <gtk/gtk.h>
#include <libinput-sources/gf-input-sources.h>

G_BEGIN_DECLS

typedef enum
{
  GF_UNLOCK_DIALOG_RESPONSE_NONE,

  GF_UNLOCK_DIALOG_RESPONSE_OK,
  GF_UNLOCK_DIALOG_RESPONSE_CANCEL
} GfUnlockDialogResponse;

#define GF_TYPE_UNLOCK_DIALOG (gf_unlock_dialog_get_type ())
G_DECLARE_FINAL_TYPE (GfUnlockDialog, gf_unlock_dialog,
                      GF, UNLOCK_DIALOG, GtkBox)

GtkWidget *gf_unlock_dialog_new                     (void);

void       gf_unlock_dialog_set_input_sources       (GfUnlockDialog *self,
                                                     GfInputSources *input_sources);

void       gf_unlock_dialog_set_user_switch_enabled (GfUnlockDialog *self,
                                                     gboolean        user_switch_enabled);

void       gf_unlock_dialog_forward_key_event       (GfUnlockDialog *self,
                                                     GdkEvent       *event);

G_END_DECLS

#endif
