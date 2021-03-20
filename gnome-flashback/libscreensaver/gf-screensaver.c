/*
 * Copyright (C) 2004-2006 William Jon McCann
 * Copyright (C) 2016-2020 Alberts Muktupāvels
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
 *     William Jon McCann <mccann@jhu.edu>
 */

#include "config.h"
#include "gf-screensaver.h"

#include "gf-fade.h"
#include "gf-grab.h"
#include "gf-listener.h"
#include "gf-manager.h"
#include "gf-prefs.h"
#include "gf-watcher.h"

#define PAM_CONFIG_FILE "/etc/pam.d/gnome-flashback"

struct _GfScreensaver
{
  GObject     parent;

  GfGrab     *grab;
  GfFade     *fade;

  GfPrefs    *prefs;

  GfListener *listener;
  GfWatcher  *watcher;

  GfManager  *manager;

  guint       release_grab_id;
};

G_DEFINE_TYPE (GfScreensaver, gf_screensaver, G_TYPE_OBJECT)

static gboolean
pam_config_exists (void)
{
  return g_file_test (PAM_CONFIG_FILE, G_FILE_TEST_EXISTS) &&
         g_file_test (PAM_CONFIG_FILE, G_FILE_TEST_IS_REGULAR);
}

static gboolean
release_grab_cb (gpointer user_data)
{
  GfScreensaver *self;

  self = GF_SCREENSAVER (user_data);

  if (!gf_manager_get_active (self->manager))
    gf_grab_release (self->grab);

  self->release_grab_id = 0;

  return G_SOURCE_REMOVE;
}

static void
simulate_user_activity (GfScreensaver *self)
{
  /* request that the manager unlock - will pop up a dialog if necessary */
  gf_manager_request_unlock (self->manager);
}

static void
update_from_prefs (GfScreensaver *self)
{
  gboolean lock_enabled;
  guint lock_timeout;
  gboolean user_switch_enabled;

  lock_enabled = gf_prefs_get_lock_enabled (self->prefs) &&
                 !gf_prefs_get_lock_disabled (self->prefs);

  if (lock_enabled && !pam_config_exists ())
    {
      g_warning ("Locking disabled because PAM configuration file `"
                 PAM_CONFIG_FILE "` does not exist");
      lock_enabled = FALSE;
    }

  gf_manager_set_lock_enabled (self->manager, lock_enabled);

  lock_timeout = gf_prefs_get_lock_delay (self->prefs);
  gf_manager_set_lock_timeout (self->manager, lock_timeout);

  user_switch_enabled = gf_prefs_get_user_switch_enabled (self->prefs) &&
                        !gf_prefs_get_user_switch_disabled (self->prefs);

  gf_manager_set_user_switch_enabled (self->manager, user_switch_enabled);

  /* in the case where idle detection is reenabled we may need to
   * activate the watcher too
   */

  if (!gf_manager_get_active (self->manager) &&
      !gf_watcher_get_active (self->watcher))
    {
      gf_watcher_set_active (self->watcher, TRUE);
    }
}

static void
prefs_changed_cb (GfPrefs       *prefs,
                  GfScreensaver *self)
{
  update_from_prefs (self);
}

static void
listener_lock_cb (GfListener    *listener,
                  GfScreensaver *self)
{
  gboolean locked;

  if (gf_prefs_get_lock_disabled (self->prefs))
    {
      g_debug ("Locking disabled by the administrator");
      return;
    }

  if (!pam_config_exists ())
    {
      g_warning ("Locking disabled because PAM configuration file `"
                 PAM_CONFIG_FILE "` does not exist");
      return;
    }

  /* set lock flag before trying to activate screensaver
   * in case something tries to react to the ActiveChanged signal
   */

  locked = gf_manager_get_lock_active (self->manager);
  gf_manager_set_lock_active (self->manager, TRUE);

  if (!gf_manager_get_active (self->manager))
    {
      if (!gf_listener_set_active (self->listener, TRUE))
        {
          /* If we've failed then restore lock status */
          gf_manager_set_lock_active (self->manager, locked);
          g_debug ("Unable to lock the screen");
        }
    }
}

static gboolean
listener_active_changed_cb (GfListener    *listener,
                            gboolean       active,
                            GfScreensaver *self)
{
  if (!gf_manager_set_active (self->manager, active))
    {
      g_debug ("Unable to set manager active: %d", active);
      return FALSE;
    }

  if (!gf_watcher_set_active (self->watcher, !active))
    g_debug ("Unable to set the idle watcher active: %d", !active);

  return TRUE;
}

static void
listener_simulate_user_activity_cb (GfListener    *listener,
                                    GfScreensaver *self)
{
  simulate_user_activity (self);
}

static void
listener_show_message_cb (GfListener    *listener,
                          const char    *summary,
                          const char    *body,
                          const char    *icon,
                          GfScreensaver *self)
{
  gf_manager_show_message (self->manager, summary, body, icon);
}

static void
listener_prepare_for_sleep_cb (GfListener    *listener,
                               GfScreensaver *self)
{
  if (!gf_prefs_get_lock_enabled (self->prefs))
    return;

  listener_lock_cb (listener, self);
}

static gboolean
watcher_idle_changed_cb (GfWatcher     *watcher,
                         gboolean       is_idle,
                         GfScreensaver *self)
{
  g_debug ("Idle signal detected: %d", is_idle);

  return gf_listener_set_session_idle (self->listener, is_idle);
}

static gboolean
watcher_idle_notice_changed_cb (GfWatcher     *watcher,
                                gboolean       in_effect,
                                GfScreensaver *self)
{
  g_debug ("Idle notice signal detected: %d", in_effect);

  if (in_effect)
    {
      /* start slow fade */
      if (gf_grab_grab_offscreen (self->grab))
        gf_fade_async (self->fade, 10000, NULL, NULL, NULL);
      else
        g_debug ("Could not grab the keyboard so not performing idle warning fade-out");

      return TRUE;
    }
  else
    {
      /* cancel the fade unless manager was activated */
      if (!gf_manager_get_active (self->manager))
        {
          g_debug ("manager not active, performing fade cancellation");
          gf_fade_reset (self->fade);

          /* don't release the grab immediately to prevent
           * typing passwords into windows
           */

          if (self->release_grab_id != 0)
            g_source_remove (self->release_grab_id);

          self->release_grab_id = g_timeout_add_seconds (1, release_grab_cb, self);

          g_source_set_name_by_id (self->release_grab_id,
                                   "[gnome-flashback] release_grab_cb");
        }
      else
        {
          g_debug ("manager active, skipping fade cancellation");
        }

      return TRUE;
    }

  return FALSE;
}

static void
manager_activated_cb (GfManager     *monitor,
                      GfScreensaver *self)
{
}

static void
manager_deactivated_cb (GfManager     *monitor,
                        GfScreensaver *self)
{
  gf_listener_set_active (self->listener, FALSE);
}

static void
gf_screensaver_dispose (GObject *object)
{
  GfScreensaver *self;

  self = GF_SCREENSAVER (object);

  g_clear_object (&self->prefs);

  g_clear_object (&self->listener);
  g_clear_object (&self->watcher);

  g_clear_object (&self->manager);

  if (self->release_grab_id != 0)
    {
      g_source_remove (self->release_grab_id);
      self->release_grab_id = 0;
    }

  g_clear_object (&self->grab);
  g_clear_object (&self->fade);

  G_OBJECT_CLASS (gf_screensaver_parent_class)->dispose (object);
}

static void
gf_screensaver_class_init (GfScreensaverClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_screensaver_dispose;
}

static void
gf_screensaver_init (GfScreensaver *self)
{
  self->grab = gf_grab_new ();
  self->fade = gf_fade_new ();

  self->prefs = gf_prefs_new ();

  self->listener = gf_listener_new ();
  self->watcher = gf_watcher_new ();

  self->manager = gf_manager_new (self->grab, self->fade);

  g_signal_connect (self->prefs, "changed",
                    G_CALLBACK (prefs_changed_cb),
                    self);

  g_signal_connect (self->listener, "lock",
                    G_CALLBACK (listener_lock_cb),
                    self);

  g_signal_connect (self->listener, "active-changed",
                    G_CALLBACK (listener_active_changed_cb),
                    self);

  g_signal_connect (self->listener, "simulate-user-activity",
                    G_CALLBACK (listener_simulate_user_activity_cb),
                    self);

  g_signal_connect (self->listener, "show-message",
                    G_CALLBACK (listener_show_message_cb),
                    self);

  g_signal_connect (self->listener, "prepare-for-sleep",
                    G_CALLBACK (listener_prepare_for_sleep_cb),
                    self);

  g_signal_connect (self->watcher, "idle-changed",
                    G_CALLBACK (watcher_idle_changed_cb),
                    self);

  g_signal_connect (self->watcher, "idle-notice-changed",
                    G_CALLBACK (watcher_idle_notice_changed_cb),
                    self);

  g_signal_connect (self->manager, "activated",
                    G_CALLBACK (manager_activated_cb),
                    self);

  g_signal_connect (self->manager, "deactivated",
                    G_CALLBACK (manager_deactivated_cb),
                    self);

  update_from_prefs (self);
}

GfScreensaver *
gf_screensaver_new (void)
{
  return g_object_new (GF_TYPE_SCREENSAVER, NULL);
}

void
gf_screensaver_set_monitor_manager (GfScreensaver    *self,
                                    GfMonitorManager *monitor_manager)
{
  gf_manager_set_monitor_manager (self->manager, monitor_manager);
}

void
gf_screensaver_set_input_sources (GfScreensaver  *self,
                                  GfInputSources *input_sources)
{
  gf_manager_set_input_sources (self->manager, input_sources);
}
