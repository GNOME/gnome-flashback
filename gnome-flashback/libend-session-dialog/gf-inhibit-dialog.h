/*
 * Copyright (C) 2008 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2015 Alberts Muktupāvels
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

#ifndef GF_INHIBIT_DIALOG_H
#define GF_INHIBIT_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
  GF_LOGOUT_ACTION_LOGOUT,
  GF_LOGOUT_ACTION_SHUTDOWN,
  GF_LOGOUT_ACTION_REBOOT,
  GF_LOGOUT_ACTION_HIBERNATE,
  GF_LOGOUT_ACTION_SUSPEND,
  GF_LOGOUT_ACTION_HYBRID_SLEEP
} GfLogoutAction;

typedef enum
{
  GF_RESPONSE_CANCEL,
  GF_RESPONSE_ACCEPT
} GfResponseType;

#define GF_TYPE_INHIBIT_DIALOG gf_inhibit_dialog_get_type ()
G_DECLARE_DERIVABLE_TYPE (GfInhibitDialog, gf_inhibit_dialog,
                          GF, INHIBIT_DIALOG, GtkWindow)

struct _GfInhibitDialogClass
{
  GtkWindowClass parent_class;

  void (* response) (GfInhibitDialog *dialog,
                     gint             response_id);
  void (* close)    (GfInhibitDialog *dialog);
};

GtkWidget *gf_inhibit_dialog_new      (gint                action,
                                       gint                seconds,
                                       const gchar *const *inhibitor_paths);

void       gf_inhibit_dialog_response (GfInhibitDialog    *dialog,
                                       gint                response_id);

void       gf_inhibit_dialog_close    (GfInhibitDialog    *dialog);

void       gf_inhibit_dialog_present  (GfInhibitDialog    *dialog,
                                       guint32             timestamp);

G_END_DECLS

#endif
