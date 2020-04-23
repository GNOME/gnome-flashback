/*
 * Copyright (C) 2019 Alberts Muktupāvels
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

#ifndef GF_AUTH_H
#define GF_AUTH_H

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * GfAuthMessageType:
 * @GF_AUTH_MESSAGE_PROMPT_ECHO_OFF: obtain a string whilst echoing text.
 * @GF_AUTH_MESSAGE_PROMPT_ECHO_ON: obtain a string without echoing any text.
 * @GF_AUTH_MESSAGE_ERROR_MSG: display an error message.
 * @GF_AUTH_MESSAGE_TEXT_INFO: display some text.
 *
 * Message type.
 */
typedef enum
{
  GF_AUTH_MESSAGE_PROMPT_ECHO_OFF,
  GF_AUTH_MESSAGE_PROMPT_ECHO_ON,
  GF_AUTH_MESSAGE_ERROR_MSG,
  GF_AUTH_MESSAGE_TEXT_INFO
} GfAuthMessageType;

#define GF_TYPE_AUTH (gf_auth_get_type ())
G_DECLARE_FINAL_TYPE (GfAuth, gf_auth, GF, AUTH, GObject)

GfAuth   *gf_auth_new             (const char *username,
                                   const char *display);

gboolean  gf_auth_awaits_response (GfAuth     *self);

void      gf_auth_set_response    (GfAuth     *self,
                                   const char *response);

void      gf_auth_verify          (GfAuth     *self);

void      gf_auth_cancel          (GfAuth     *self);

G_END_DECLS

#endif
