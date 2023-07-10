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

#ifndef FLASHBACK_AUTHENTICATOR_H
#define FLASHBACK_AUTHENTICATOR_H

#include <glib-object.h>
#include <polkitagent/polkitagent.h>

G_BEGIN_DECLS

#define FLASHBACK_TYPE_AUTHENTICATOR flashback_authenticator_get_type ()
G_DECLARE_FINAL_TYPE (FlashbackAuthenticator, flashback_authenticator,
                      FLASHBACK, AUTHENTICATOR, GObject)

FlashbackAuthenticator *flashback_authenticator_new        (const gchar            *action_id,
                                                            const gchar            *message,
                                                            const gchar            *icon_name,
                                                            PolkitDetails          *details,
                                                            const gchar            *cookie,
                                                            GList                  *identities);

void                    flashback_authenticator_initiate   (FlashbackAuthenticator *authenticator);
void                    flashback_authenticator_cancel     (FlashbackAuthenticator *authenticator);
const gchar            *flashback_authenticator_get_cookie (FlashbackAuthenticator *authenticator);



G_END_DECLS

#endif
