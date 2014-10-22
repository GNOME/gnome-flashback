/*
 * Copyright (C) 2008 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FLASHBACK_INHIBIT_DIALOG_H
#define FLASHBACK_INHIBIT_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
	FLASHBACK_LOGOUT_ACTION_LOGOUT,
	FLASHBACK_LOGOUT_ACTION_SHUTDOWN,
	FLASHBACK_LOGOUT_ACTION_REBOOT,
	FLASHBACK_LOGOUT_ACTION_HIBERNATE,
	FLASHBACK_LOGOUT_ACTION_SUSPEND,
	FLASHBACK_LOGOUT_ACTION_HYBRID_SLEEP
} FlashbackLogoutAction;

typedef enum {
	FLASHBACK_RESPONSE_CANCEL,
	FLASHBACK_RESPONSE_ACCEPT
} FlashbackResponseType;

#define FLASHBACK_TYPE_INHIBIT_DIALOG         (flashback_inhibit_dialog_get_type ())
#define FLASHBACK_INHIBIT_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), FLASHBACK_TYPE_INHIBIT_DIALOG, FlashbackInhibitDialog))
#define FLASHBACK_INHIBIT_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k),     FLASHBACK_TYPE_INHIBIT_DIALOG, FlashbackInhibitDialogClass))
#define FLASHBACK_IS_INHIBIT_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), FLASHBACK_TYPE_INHIBIT_DIALOG))
#define FLASHBACK_IS_INHIBIT_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k),    FLASHBACK_TYPE_INHIBIT_DIALOG))
#define FLASHBACK_INHIBIT_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  FLASHBACK_TYPE_INHIBIT_DIALOG, FlashbackInhibitDialogClass))

typedef struct _FlashbackInhibitDialog        FlashbackInhibitDialog;
typedef struct _FlashbackInhibitDialogClass   FlashbackInhibitDialogClass;
typedef struct _FlashbackInhibitDialogPrivate FlashbackInhibitDialogPrivate;

struct _FlashbackInhibitDialog {
	GtkWindow                      parent;
	FlashbackInhibitDialogPrivate *priv;
};

struct _FlashbackInhibitDialogClass {
	GtkWindowClass parent_class;

	void (* response) (FlashbackInhibitDialog *dialog, gint response_id);
	void (* close)    (FlashbackInhibitDialog *dialog);
};

GType      flashback_inhibit_dialog_get_type (void);
GtkWidget *flashback_inhibit_dialog_new      (gint                action,
                                              gint                seconds,
                                              const gchar *const *inhibitor_paths);

void       flashback_inhibit_dialog_response (FlashbackInhibitDialog *dialog,
                                              gint                    response_id);
void       flashback_inhibit_dialog_close    (FlashbackInhibitDialog *dialog);
G_END_DECLS

#endif
