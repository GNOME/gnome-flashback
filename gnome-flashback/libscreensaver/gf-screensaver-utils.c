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

#include "config.h"
#include "gf-screensaver-utils.h"

#include <systemd/sd-login.h>
#include <unistd.h>

gboolean
gf_find_systemd_session (char **session_id)
{
  int n_sessions;
  char **sessions;
  char *session;
  int i;

  n_sessions = sd_uid_get_sessions (getuid (), 0, &sessions);

  if (n_sessions < 0)
    {
      g_debug ("Failed to get sessions for user %d", getuid ());
      return FALSE;
    }

  session = NULL;

  for (i = 0; i < n_sessions; i++)
    {
      int r;
      char *type;

      r = sd_session_get_type (sessions[i], &type);

      if (r < 0)
        {
          g_debug ("Couldn't get type for session '%s': %s",
                   sessions[i], strerror (-r));
          continue;
        }

      if (g_strcmp0 (type, "x11") != 0)
        {
          free (type);
          continue;
        }

      session = sessions[i];
      free (type);
    }

  if (session != NULL)
    *session_id = g_strdup (session);

  for (i = 0; i < n_sessions; i++)
    free (sessions[i]);
  free (sessions);

  return session != NULL;
}
