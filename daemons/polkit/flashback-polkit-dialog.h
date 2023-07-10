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

#ifndef FLASHBACK_POLKIT_DIALOG_H
#define FLASHBACK_POLKIT_DIALOG_H

#include <gtk/gtk.h>
#include <polkit/polkit.h>

G_BEGIN_DECLS

#define FLASHBACK_TYPE_POLKIT_DIALOG flashback_polkit_dialog_get_type ()
G_DECLARE_FINAL_TYPE (FlashbackPolkitDialog, flashback_polkit_dialog,
                      FLASHBACK, POLKIT_DIALOG, GtkWindow)

GtkWidget *flashback_polkit_dialog_new                           (const gchar            *action_id,
                                                                  const gchar            *vendor,
                                                                  const gchar            *vendor_url,
                                                                  const gchar            *icon_name,
                                                                  const gchar            *message_markup,
                                                                  PolkitDetails          *details,
                                                                  gchar                 **users);

gchar     *flashback_polkit_dialog_get_selected_user             (FlashbackPolkitDialog  *dialog);

gboolean   flashback_polkit_dialog_run_until_user_is_selected    (FlashbackPolkitDialog  *dialog);
gchar     *flashback_polkit_dialog_run_until_response_for_prompt (FlashbackPolkitDialog  *dialog,
                                                                  const gchar            *prompt,
                                                                  gboolean                echo_chars,
                                                                  gboolean               *was_cancelled,
                                                                  gboolean               *new_user_selected);

gboolean   flashback_polkit_dialog_cancel                        (FlashbackPolkitDialog  *dialog);
void       flashback_polkit_dialog_indicate_error                (FlashbackPolkitDialog  *dialog);
void       flashback_polkit_dialog_set_info_message              (FlashbackPolkitDialog  *dialog,
                                                                  const gchar            *info_markup);

void       flashback_polkit_dialog_present                       (FlashbackPolkitDialog  *dialog);

G_END_DECLS

#endif
