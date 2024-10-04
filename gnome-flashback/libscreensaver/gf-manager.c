/*
 * Copyright (C) 2004-2008 William Jon McCann
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     William Jon McCann <mccann@jhu.edu>
 */

#include "config.h"
#include "gf-manager.h"

#include "gf-window.h"
#include "libcommon/gf-bg.h"

struct _GfManager
{
  GObject           parent;

  GfBG             *bg;

  GfGrab           *grab;
  GfFade           *fade;

  GfMonitorManager *monitor_manager;

  GfInputSources   *input_sources;

  GSList           *windows;

  gboolean          active;

  gboolean          lock_active;

  gboolean          lock_enabled;

  glong             lock_timeout;

  gboolean          user_switch_enabled;

  time_t            activate_time;

  guint             lock_timeout_id;
  guint             unfade_timeout_id;

  gboolean          dialog_up;

  gulong            monitor_added_id;
  gulong            monitor_removed_id;
};

enum
{
  PROP_0,

  PROP_GRAB,
  PROP_FADE,

  LAST_PROP
};

static GParamSpec *manager_properties[LAST_PROP] = { NULL };

enum
{
  ACTIVATED,
  DEACTIVATED,

  LAST_SIGNAL
};

static guint manager_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfManager, gf_manager, G_TYPE_OBJECT)

static GfWindow *
find_window_at_pointer (GfManager *self)
{
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *pointer;
  GfWindow *window;

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);
  pointer = gdk_seat_get_pointer (seat);

  window = NULL;
  if (pointer != NULL)
    {
      int x;
      int y;
      GdkMonitor *monitor;
      GSList *l;

      gdk_device_get_position (pointer, NULL, &x, &y);
      monitor = gdk_display_get_monitor_at_point (display, x, y);

      /* find the GfWindow that is on screen */
      for (l = self->windows; l != NULL; l = l->next)
        {
          GfWindow *tmp;

          tmp = GF_WINDOW (l->data);
          if (gf_window_get_monitor (tmp) != monitor)
            continue;

          window = tmp;
          break;
        }
    }

  if (window == NULL)
    {
      g_debug ("WARNING: Could not find the GfWindow for screen");
      window = self->windows->data;
    }

  return window;
}

static void
lock_timeout_activate (GfManager *self)
{
  if (!self->lock_enabled)
    return;

  gf_manager_set_lock_active (self, TRUE);
}

static gboolean
lock_timeout_cb (gpointer user_data)
{
  GfManager *self;

  self = GF_MANAGER (user_data);

  lock_timeout_activate (self);
  self->lock_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
lock_timeout_add (GfManager *self,
                  glong      timeout)
{
  g_assert (self->lock_timeout_id == 0);
  self->lock_timeout_id = g_timeout_add (timeout, lock_timeout_cb, self);

  g_source_set_name_by_id (self->lock_timeout_id,
                           "[gnome-flashback] lock_timeout_cb");
}

static void
lock_timeout_remove (GfManager *self)
{
  if (self->lock_timeout_id == 0)
    return;

  g_source_remove (self->lock_timeout_id);
  self->lock_timeout_id = 0;
}

static gboolean
unfade_timeout_cb (gpointer user_data)
{
  GfManager *self;

  self = GF_MANAGER (user_data);

  gf_fade_reset (self->fade);
  self->unfade_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
unfade_timeout_add (GfManager *self)
{
  g_assert (self->unfade_timeout_id == 0);
  self->unfade_timeout_id = g_timeout_add (500, unfade_timeout_cb, self);

  g_source_set_name_by_id (self->unfade_timeout_id,
                           "[gnome-flashback] unfade_timeout_cb");
}

static void
unfade_timeout_remove (GfManager *self)
{
  if (self->unfade_timeout_id == 0)
    return;

  g_source_remove (self->unfade_timeout_id);
  self->unfade_timeout_id = 0;
}

static void
apply_background (GfManager *self,
                  GfWindow  *window)
{
  GdkMonitor *monitor;
  GdkRectangle rect;
  cairo_surface_t *surface;

  monitor = gf_window_get_monitor (window);
  gdk_monitor_get_geometry (monitor, &rect);

  g_debug ("Creating background: size %dx%d", rect.width, rect.height);

  surface = gf_bg_create_surface (self->bg,
                                  gtk_widget_get_window (GTK_WIDGET (window)),
                                  rect.width,
                                  rect.height,
                                  FALSE);

  gf_window_set_background (window, surface);
  cairo_surface_destroy (surface);
}

static void
bg_changed_cb (GfBG      *bg,
               GfManager *self)
{
  GSList *l;

  g_debug ("Background changed");

  for (l = self->windows; l != NULL; l = l->next)
    apply_background (self, GF_WINDOW (l->data));
}

static void
window_activity_cb (GfWindow  *window,
                    GfManager *self)
{
  gf_manager_request_unlock (self);
}

static gboolean
window_deactivated_idle_cb (gpointer user_data)
{
  GfManager *self;

  self = GF_MANAGER (user_data);

  /* don't deactivate directly but only emit a signal so
   * that we let the parent deactivate
   */
  g_signal_emit (self, manager_signals[DEACTIVATED], 0);

  return G_SOURCE_REMOVE;
}

static void
window_deactivated_cb (GfWindow  *window,
                       GfManager *self)
{
  g_idle_add (window_deactivated_idle_cb, self);
}

static void
window_size_changed_cb (GfWindow  *window,
                        GfManager *self)
{
  apply_background (self, window);
}

static void
window_dialog_up_cb (GfWindow  *window,
                     gboolean   up,
                     GfManager *self)
{
  GSList *l;

  g_debug ("Handling window dialog up changed: %s", up ? "up" : "down");

  if (up)
    {
      GdkWindow *grab_window;

      g_debug ("Handling dialog up");

      self->dialog_up = TRUE;

      /* Make all other windows insensitive so we don't get events */
      for (l = self->windows; l != NULL; l = l->next)
        {
          if (l->data == window)
            continue;

          gtk_widget_set_sensitive (GTK_WIDGET (l->data), FALSE);
        }

      /* Move keyboard and mouse grabs so dialog can be used */
      grab_window = gtk_widget_get_window (GTK_WIDGET (window));
      gf_grab_move_to_window (self->grab, grab_window);
    }
  else
    {
      g_debug ("Handling dialog down");

      /* Make all windows sensitive so we get events */
      for (l = self->windows; l != NULL; l = l->next)
        gtk_widget_set_sensitive (GTK_WIDGET (l->data), TRUE);

      self->dialog_up = FALSE;
    }
}

static void
window_show_cb (GtkWidget *widget,
                GfManager *self)
{
  g_debug ("Handling window show");

  apply_background (self, GF_WINDOW (widget));

  self->activate_time = time (NULL);

  if (self->lock_timeout >= 0)
    {
      lock_timeout_remove (self);
      lock_timeout_add (self, self->lock_timeout);
    }

  unfade_timeout_remove (self);
  unfade_timeout_add (self);

  g_signal_emit (self, manager_signals[ACTIVATED], 0);
}

static void
window_map_cb (GtkWidget *widget,
               GfManager *self)
{
  g_debug ("Handling window map event");
}

static void
window_unmap_cb (GtkWidget *widget,
                 GfManager *self)
{
  g_debug ("Window unmapped");
}

static gboolean
window_map_event_cb (GtkWidget *widget,
                     GdkEvent  *event,
                     GfManager *self)
{
  GfWindow *pointer_window;
  GdkDisplay *display;
  GdkWindow *grab_window;

  g_debug ("Handling window map-event event");

  pointer_window = find_window_at_pointer (self);
  if (pointer_window != GF_WINDOW (widget))
    return FALSE;

  g_debug ("Moving grab to %p", widget);

  display = gdk_display_get_default ();
  gdk_display_flush (display);

  grab_window = gtk_widget_get_window (widget);
  gf_grab_move_to_window (self->grab, grab_window);

  return FALSE;
}

static GfWindow *
create_window_for_monitor (GfManager  *self,
                           GdkMonitor *monitor)
{
  GdkRectangle rect;
  GfWindow *window;

  gdk_monitor_get_geometry (monitor, &rect);
  g_debug ("Creating window for monitor: x - %d, y - %d, width - %d, height - %d",
           rect.x, rect.y, rect.width, rect.height);

  window = gf_window_new (monitor);

  gf_window_set_input_sources (window, self->input_sources);

  gf_window_set_lock_enabled (window, self->lock_active);
  gf_window_set_user_switch_enabled (window, self->user_switch_enabled);

  g_signal_connect (window, "activity",
                    G_CALLBACK (window_activity_cb),
                    self);

  g_signal_connect (window, "deactivated",
                    G_CALLBACK (window_deactivated_cb),
                    self);

  g_signal_connect (window, "size-changed",
                    G_CALLBACK (window_size_changed_cb),
                    self);

  g_signal_connect (window, "dialog-up",
                    G_CALLBACK (window_dialog_up_cb),
                    self);

  g_signal_connect (window, "show",
                    G_CALLBACK (window_show_cb),
                    self);

  g_signal_connect (window, "map",
                    G_CALLBACK (window_map_cb),
                    self);

  g_signal_connect (window, "unmap",
                    G_CALLBACK (window_unmap_cb),
                    self);

  g_signal_connect (window, "map-event",
                    G_CALLBACK (window_map_event_cb),
                    self);

  if (self->active)
    gtk_widget_show (GTK_WIDGET (window));

  return window;
}

static void
monitor_added_cb (GdkDisplay *display,
                  GdkMonitor *monitor,
                  GfManager  *self)
{
  GSList *l;
  GfWindow *window;

  g_debug ("Monitor added");

  /* tear down unlock dialog in case we want to move it to a new monitor */
  for (l = self->windows; l != NULL; l = l->next)
    gf_window_cancel_unlock_request (GF_WINDOW (l->data));

  /* add new window */
  window = create_window_for_monitor (self, monitor);
  self->windows = g_slist_prepend (self->windows, window);

  /* put unlock dialog up where ever it's supposed to be */
  gf_manager_request_unlock (self);
}

static void
cancel_unlock_func (gpointer data,
                    gpointer user_data)
{
  gf_window_cancel_unlock_request (GF_WINDOW (data));
}

static void
monitor_removed_cb (GdkDisplay *display,
                    GdkMonitor *monitor,
                    GfManager  *self)
{
  GSList *l;

  g_debug ("Monitor removed");

  /* tear down unlock dialog in case we want to move it to a new monitor */
  g_slist_foreach (self->windows, cancel_unlock_func, NULL);

  /* destroy removed monitor window */
  for (l = self->windows; l != NULL; l = l->next)
    {
      if (gf_window_get_monitor (GF_WINDOW (l->data)) != monitor)
        continue;

      self->windows = g_slist_remove_link (self->windows, l);
      gtk_widget_destroy (GTK_WIDGET (l->data));
      g_slist_free (l);
      break;
    }

  /* put unlock dialog up where ever it's supposed to be */
  gf_manager_request_unlock (self);
}

static void
create_windows (GfManager *self)
{
  GdkDisplay *display;
  int n_monitors;
  int i;

  if (self->windows != NULL)
    return;

  display = gdk_display_get_default ();
  n_monitors = gdk_display_get_n_monitors (display);

  g_assert (self->monitor_added_id == 0);
  self->monitor_added_id = g_signal_connect (display, "monitor-added",
                                             G_CALLBACK (monitor_added_cb),
                                             self);

  g_assert (self->monitor_removed_id == 0);
  self->monitor_removed_id = g_signal_connect (display, "monitor-removed",
                                               G_CALLBACK (monitor_removed_cb),
                                               self);

  for (i = 0; i < n_monitors; i++)
    {
      GdkMonitor *monitor;
      GfWindow *window;

      monitor = gdk_display_get_monitor (display, i);

      window = create_window_for_monitor (self, monitor);
      self->windows = g_slist_prepend (self->windows, window);
      gtk_widget_show (GTK_WIDGET (window));
    }
}

static void
free_window (gpointer data)
{
  gtk_widget_destroy (GTK_WIDGET (data));
}

static void
destroy_windows (GfManager *self)
{
  GdkDisplay *display;

  if (self->windows == NULL)
    return;

  display = gdk_display_get_default ();

  g_clear_signal_handler (&self->monitor_added_id, display);
  g_clear_signal_handler (&self->monitor_removed_id, display);

  g_slist_free_full (self->windows, free_window);
  self->windows = NULL;
}

static void
gf_manager_dispose (GObject *object)
{
  GfManager *self;

  self = GF_MANAGER (object);

  g_clear_object (&self->bg);

  G_OBJECT_CLASS (gf_manager_parent_class)->dispose (object);
}

static void
gf_manager_finalize (GObject *object)
{
  GfManager *self;

  self = GF_MANAGER (object);

  gf_grab_release (self->grab);
  gf_fade_reset (self->fade);

  lock_timeout_remove (self);
  unfade_timeout_remove (self);

  destroy_windows (self);

  G_OBJECT_CLASS (gf_manager_parent_class)->finalize (object);
}

static void
gf_manager_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GfManager *self;

  self = GF_MANAGER (object);

  switch (property_id)
    {
      case PROP_GRAB:
        g_assert (self->grab == NULL);
        self->grab = g_value_get_object (value);
        break;

      case PROP_FADE:
        g_assert (self->fade == NULL);
        self->fade = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  manager_properties[PROP_GRAB] =
    g_param_spec_object ("grab",
                         "grab",
                         "grab",
                         GF_TYPE_GRAB,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  manager_properties[PROP_FADE] =
    g_param_spec_object ("fade",
                         "fade",
                         "fade",
                         GF_TYPE_FADE,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     manager_properties);
}

static void
install_signals (void)
{
  manager_signals[ACTIVATED] =
    g_signal_new ("activated", GF_TYPE_MANAGER, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  manager_signals[DEACTIVATED] =
    g_signal_new ("deactivated", GF_TYPE_MANAGER, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gf_manager_class_init (GfManagerClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_manager_dispose;
  object_class->finalize = gf_manager_finalize;
  object_class->set_property = gf_manager_set_property;

  install_properties (object_class);
  install_signals ();
}

static void
gf_manager_init (GfManager *self)
{
  self->bg = gf_bg_new ("org.gnome.desktop.screensaver");

  g_signal_connect (self->bg, "changed",
                    G_CALLBACK (bg_changed_cb),
                    self);

  gf_bg_load_from_preferences (self->bg);
}

GfManager *
gf_manager_new (GfGrab *grab,
                GfFade *fade)
{
  return g_object_new (GF_TYPE_MANAGER,
                       "grab", grab,
                       "fade", fade,
                       NULL);
}

void
gf_manager_set_monitor_manager (GfManager        *self,
                                GfMonitorManager *monitor_manager)
{
  self->monitor_manager = monitor_manager;
}

void
gf_manager_set_input_sources (GfManager      *self,
                              GfInputSources *input_sources)
{
  GSList *l;

  self->input_sources = input_sources;

  for (l = self->windows; l != NULL; l = l->next)
    gf_window_set_input_sources (GF_WINDOW (l->data), input_sources);
}

gboolean
gf_manager_set_active (GfManager *self,
                       gboolean   active)
{
  if (active)
    {
      if (self->active)
        {
          g_debug ("Trying to activate manager when already active");
          return FALSE;
        }

      if (!gf_grab_grab_root (self->grab))
        return FALSE;

      create_windows (self);

      self->active = TRUE;
    }
  else
    {
      if (!self->active)
        {
          g_debug ("Trying to deactivate a screensaver that is not active");
          return FALSE;
        }

      gf_grab_release (self->grab);

      lock_timeout_remove (self);
      unfade_timeout_remove (self);

      gf_fade_reset (self->fade);
      destroy_windows (self);

      self->lock_active = FALSE;
      self->activate_time = 0;
      self->dialog_up = FALSE;

      self->active = FALSE;
    }

  return TRUE;
}

gboolean
gf_manager_get_active (GfManager *self)
{
  return self->active;
}

gboolean
gf_manager_get_lock_active (GfManager *self)
{
  return self->lock_active;
}

void
gf_manager_set_lock_active (GfManager *self,
                            gboolean   lock_active)
{
  GSList *l;

  g_debug ("Setting lock active: %d", lock_active);

  if (self->lock_active == lock_active)
    return;

  self->lock_active = lock_active;

  for (l = self->windows; l != NULL; l = l->next)
    gf_window_set_lock_enabled (GF_WINDOW (l->data), lock_active);
}

gboolean
gf_manager_get_lock_enabled (GfManager *self)
{
  return self->lock_enabled;
}

void
gf_manager_set_lock_enabled (GfManager *self,
                             gboolean   lock_enabled)
{
  if (self->lock_enabled == lock_enabled)
    return;

  g_debug ("GfManager: lock-enabled=%d", lock_enabled);
  self->lock_enabled = lock_enabled;
}

void
gf_manager_set_lock_timeout (GfManager *self,
                             glong      lock_timeout)
{
  glong elapsed;

  if (self->lock_timeout == lock_timeout)
    return;

  g_debug ("GfManager: lock-timeout=%ld", lock_timeout);
  self->lock_timeout = lock_timeout;

  if (!self->active || self->lock_active || lock_timeout < 0)
    return;

  elapsed = (time (NULL) - self->activate_time) * 1000;

  lock_timeout_remove (self);

  if (elapsed >= lock_timeout)
    lock_timeout_activate (self);
  else
    lock_timeout_add (self, lock_timeout - elapsed);
}

void
gf_manager_set_user_switch_enabled (GfManager *self,
                                    gboolean   user_switch_enabled)
{
  GSList *l;

  if (self->user_switch_enabled == user_switch_enabled)
    return;

  self->user_switch_enabled = user_switch_enabled;

  for (l = self->windows; l != NULL; l = l->next)
    gf_window_set_user_switch_enabled (GF_WINDOW (l->data), user_switch_enabled);
}

void
gf_manager_show_message (GfManager  *self,
                         const char *summary,
                         const char *body,
                         const char *icon)
{
  GfWindow *window;

  /* find the GfWindow that contains the pointer */
  window = find_window_at_pointer (self);
  gf_window_show_message (window, summary, body, icon);

  gf_manager_request_unlock (self);
}

void
gf_manager_request_unlock (GfManager *self)
{
  GfWindow *window;

  if (!self->active)
    {
      g_debug ("Request unlock but manager is not active");
      return;
    }

  if (self->dialog_up)
    {
      g_debug ("Request unlock but dialog is already up");
      return;
    }

  if (self->windows == NULL)
    {
      g_debug ("We don't have any windows!");
      return;
    }

  /* find the GfWindow that contains the pointer */
  window = find_window_at_pointer (self);
  gf_window_request_unlock (window);
}
