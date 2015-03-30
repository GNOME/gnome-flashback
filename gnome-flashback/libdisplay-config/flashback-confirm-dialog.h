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

#ifndef FLASHBACK_CONFIRM_DIALOG_H
#define FLASHBACK_CONFIRM_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum _FlashbackConfirmDialogResponseType FlashbackConfirmDialogResponseType;

#define FLASHBACK_TYPE_CONFIRM_DIALOG flashback_confirm_dialog_get_type ()
G_DECLARE_DERIVABLE_TYPE (FlashbackConfirmDialog, flashback_confirm_dialog,
                          FLASHBACK, CONFIRM_DIALOG,
                          GtkWindow)

enum FlashbackConfirmDialogResponseType
{
  FLASHBACK_CONFIRM_DIALOG_RESPONSE_REVERT_SETTINGS,
  FLASHBACK_CONFIRM_DIALOG_RESPONSE_KEEP_CHANGES,
};

struct _FlashbackConfirmDialogClass
{
  GtkWindowClass parent_class;

  void (* response) (FlashbackConfirmDialog *dialog,
                     gint                    response_id);
  void (* close)    (FlashbackConfirmDialog *dialog);
};

GtkWidget *flashback_confirm_dialog_new (gint timeout);

G_END_DECLS

#endif
