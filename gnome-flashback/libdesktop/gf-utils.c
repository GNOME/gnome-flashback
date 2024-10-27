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
#include "gf-utils.h"

#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <systemd/sd-journal.h>

static void
child_setup (gpointer user_data)
{
  GAppInfo *app_info;
  const gchar *id;
  gint stdout_fd;
  gint stderr_fd;

  app_info = G_APP_INFO (user_data);
  id = g_app_info_get_id (app_info);

  stdout_fd = sd_journal_stream_fd (id, LOG_INFO, FALSE);
  if (stdout_fd >= 0)
    {
      dup2 (stdout_fd, STDOUT_FILENO);
      close (stdout_fd);
    }

  stderr_fd = sd_journal_stream_fd (id, LOG_WARNING, FALSE);
  if (stderr_fd >= 0)
    {
      dup2 (stderr_fd, STDERR_FILENO);
      close (stderr_fd);
    }
}

static void
close_pid (GPid     pid,
           gint     status,
           gpointer user_data)
{
  g_spawn_close_pid (pid);
}

static void
pid_cb (GDesktopAppInfo *app_info,
        GPid             pid,
        gpointer         user_data)
{
  g_child_watch_add (pid, close_pid, NULL);
}

static gboolean
app_info_launch_uris (GDesktopAppInfo  *app_info,
                      GList            *uris,
                      GError          **error)
{
  GdkDisplay *display;
  GdkAppLaunchContext *context;
  GSpawnFlags flags;
  gboolean ret;

  display = gdk_display_get_default ();
  context = gdk_display_get_app_launch_context (display);
  flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;

  ret = g_desktop_app_info_launch_uris_as_manager (app_info,
                                                   uris,
                                                   G_APP_LAUNCH_CONTEXT (context),
                                                   flags,
                                                   child_setup,
                                                   app_info,
                                                   pid_cb,
                                                   NULL,
                                                   error);

  g_object_unref (context);

  return ret;
}

static GAppInfo *
get_app_info_for_uri (const gchar  *uri,
                      GError      **error)
{
  GAppInfo *app_info;
  gchar *scheme;
  GFile *file;

  app_info = NULL;
  scheme = g_uri_parse_scheme (uri);

  if (scheme && scheme[0] != '\0')
    app_info = g_app_info_get_default_for_uri_scheme (scheme);

  g_free (scheme);

  if (app_info != NULL)
    return app_info;

  file = g_file_new_for_uri (uri);
  app_info = g_file_query_default_handler (file, NULL, error);
  g_object_unref (file);

  return app_info;
}

gboolean
gf_launch_app_info (GDesktopAppInfo  *app_info,
                    GError          **error)
{
  return app_info_launch_uris (app_info, NULL, error);
}

gboolean
gf_launch_desktop_file (const char  *desktop_file,
                        GError     **error)
{
  GDesktopAppInfo *app_info;

  app_info = g_desktop_app_info_new (desktop_file);

  if (app_info == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Desktop file `%s` does not exists",
                   desktop_file);

      return FALSE;
    }

  if (!app_info_launch_uris (app_info, NULL, error))
    {
      g_object_unref (app_info);

      return FALSE;
    }

  g_object_unref (app_info);

  return TRUE;
}

gboolean
gf_launch_uri (const char  *uri,
               GError     **error)
{
  GAppInfo *app_info;
  GList *uris;
  gboolean launched;

  app_info = get_app_info_for_uri (uri, error);

  if (app_info == NULL)
    return FALSE;

  uris = g_list_append (NULL, (gchar *) uri);
  launched = app_info_launch_uris (G_DESKTOP_APP_INFO (app_info), uris, error);

  g_object_unref (app_info);
  g_list_free (uris);

  return launched;
}

double
gf_get_nautilus_scale (void)
{
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;
  GSettings *settings;
  int zoom_level;
  double size;

  source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (source,
                                            "org.gnome.nautilus.icon-view",
                                            FALSE);

  if (schema == NULL)
    return 1.0;

  g_settings_schema_unref  (schema);
  settings = g_settings_new ("org.gnome.nautilus.icon-view");

  zoom_level = g_settings_get_enum (settings, "default-zoom-level");
  g_object_unref (settings);

  if (zoom_level == 0)
    size = 48.0;
  else if (zoom_level == 1)
    size = 64.0;
  else if (zoom_level == 2)
    size = 96.0;
  else if (zoom_level == 3)
    size = 128.0;
  else if (zoom_level == 4)
    size = 256.0;
  else
    size = 64.0;

  return size / 64.0;
}
