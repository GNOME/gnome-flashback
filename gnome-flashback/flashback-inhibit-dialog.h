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

#define FLASHBACK_TYPE_INHIBIT_DIALOG         (flashback_inhibit_dialog_get_type ())
#define FLASHBACK_INHIBIT_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), FLASHBACK_TYPE_INHIBIT_DIALOG, FlashbackInhibitDialog))
#define FLASHBACK_INHIBIT_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k),     FLASHBACK_TYPE_INHIBIT_DIALOG, FlashbackInhibitDialogClass))
#define FLASHBACK_IS_INHIBIT_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), FLASHBACK_TYPE_INHIBIT_DIALOG))
#define FLASHBACK_IS_INHIBIT_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k),    FLASHBACK_TYPE_INHIBIT_DIALOG))
#define FLASHBACK_INHIBIT_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  FLASHBACK_TYPE_INHIBIT_DIALOG, FlashbackInhibitDialogClass))

typedef enum _FlashbackLogoutAction FlashbackLogoutAction;

typedef struct _FlashbackInhibitDialog        FlashbackInhibitDialog;
typedef struct _FlashbackInhibitDialogClass   FlashbackInhibitDialogClass;
typedef struct _FlashbackInhibitDialogPrivate FlashbackInhibitDialogPrivate;

enum _FlashbackLogoutAction {
	FLASHBACK_LOGOUT_ACTION_LOGOUT,
	FLASHBACK_LOGOUT_ACTION_SHUTDOWN,
	FLASHBACK_LOGOUT_ACTION_REBOOT
};

struct _FlashbackInhibitDialog {
	GtkDialog                      parent;
	FlashbackInhibitDialogPrivate *priv;
};

struct _FlashbackInhibitDialogClass {
	GtkDialogClass parent_class;
};

GType      flashback_inhibit_dialog_get_type (void);
GtkWidget *flashback_inhibit_dialog_new      (int action,
                                              int seconds,
                                              const char *const *inhibitor_paths);

G_END_DECLS

#endif
