/*
 * Copyright (C) 2014 - 2015 Alberts Muktupāvels
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

#ifndef GF_SESSION_H
#define GF_SESSION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GF_TYPE_SESSION gf_session_get_type ()
G_DECLARE_FINAL_TYPE (GfSession, gf_session, GF, SESSION, GObject)

/**
 * GfSessionReadyCallback:
 * @session: a #GfSession
 * @user_data: user data
 */
typedef void (*GfSessionReadyCallback) (GfSession *session,
                                        gpointer   user_data);

/**
 * GfSessionEndCallback:
 * @session: a #GfSession
 * @user_data: user data
 */
typedef void (*GfSessionEndCallback) (GfSession *session,
                                      gpointer   user_data);

GfSession *gf_session_new             (gboolean                replace,
                                       GfSessionReadyCallback  ready_cb,
                                       GfSessionEndCallback    end_cb,
                                       gpointer                user_data);

gboolean   gf_session_set_environment (GfSession              *session,
                                       const gchar            *name,
                                       const gchar            *value);

gboolean   gf_session_register        (GfSession              *session);

G_END_DECLS

#endif
