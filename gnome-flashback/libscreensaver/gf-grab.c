/*
 * Copyright (C) 2004-2006 William Jon McCann
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
#include "gf-grab.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

struct _GfGrab
{
  GObject    parent;

  GtkWidget *invisible;

  GdkWindow *grab_window;
};

G_DEFINE_TYPE (GfGrab, gf_grab, G_TYPE_OBJECT)

static const char *
grab_string (GdkGrabStatus status)
{
  switch (status)
    {
      case GDK_GRAB_SUCCESS:
        return "GrabSuccess";

      case GDK_GRAB_ALREADY_GRABBED:
        return "AlreadyGrabbed";

      case GDK_GRAB_INVALID_TIME:
        return "GrabInvalidTime";

      case GDK_GRAB_NOT_VIEWABLE:
        return "GrabNotViewable";

      case GDK_GRAB_FROZEN:
        return "GrabFrozen";

      case GDK_GRAB_FAILED:
        return "GrabFailed";

      default:
        break;
    }

  return "unknown";
}

static void
grab_window_reset (GfGrab *self)
{
  gpointer location;

  if (self->grab_window == NULL)
    return;

  location = &self->grab_window;
  g_object_remove_weak_pointer (G_OBJECT (self->grab_window), location);

  self->grab_window = NULL;
}

static void
prepare_cb (GdkSeat   *seat,
            GdkWindow *window,
            gpointer   user_data)
{
  gdk_window_show_unraised (window);
}

static GdkGrabStatus
grab (GfGrab    *self,
      GdkWindow *window)
{
  GdkDisplay *display;
  GdkSeat *seat;
  GdkSeatCapabilities capabilities;
  GdkGrabStatus status;

  display = gdk_window_get_display (window);
  seat = gdk_display_get_default_seat (display);
  capabilities = GDK_SEAT_CAPABILITY_ALL;

  g_debug ("Grabbing window 0x%lx", gdk_x11_window_get_xid (window));

  status = gdk_seat_grab (seat,
                          window,
                          capabilities,
                          TRUE,
                          NULL,
                          NULL,
                          prepare_cb,
                          NULL);

  if (status == GDK_GRAB_SUCCESS)
    {
      gpointer location;

      grab_window_reset (self);
      self->grab_window = window;

      location = &self->grab_window;
      g_object_add_weak_pointer (G_OBJECT (self->grab_window), location);
    }

  return status;
}

static void
move_input_focus (GdkWindow *window)
{
  GdkDisplay *display;
  Display *xdisplay;
  Window xwindow;
  Window old_focus;
  int old_revert_to;

  g_debug ("Moving input focus");

  display = gdk_window_get_display (window);
  xdisplay = gdk_x11_display_get_xdisplay (display);
  xwindow = gdk_x11_window_get_xid (window);

  gdk_x11_display_error_trap_push (display);

  XGetInputFocus (xdisplay, &old_focus, &old_revert_to);
  XSetInputFocus (xdisplay, xwindow, RevertToParent, CurrentTime);

  gdk_x11_display_error_trap_pop_ignored (display);
}

static gboolean
grab_window (GfGrab    *self,
             GdkWindow *window)
{
  int retries;
  int i;
  GdkGrabStatus status;

  retries = 4;

  for (i = 0; i < retries; i++)
    {
      status = grab (self, window);

      if (status == GDK_GRAB_SUCCESS)
        return TRUE;

      /* else, wait a second and try to grab again. */
      sleep (1);
    }

  move_input_focus (window);

  for (i = 0; i < retries; i++)
    {
      status = grab (self, window);

      if (status == GDK_GRAB_SUCCESS)
        return TRUE;

      /* else, wait a second and try to grab again. */
      sleep (1);
    }

  g_debug ("Couldn't grab window 0x%lx (%s)",
           gdk_x11_window_get_xid (window),
           grab_string (status));

  return FALSE;
}

static gboolean
grab_move (GfGrab    *self,
           GdkWindow *window)
{
  GdkDisplay *display;
  GdkWindow *old_window;
  GdkGrabStatus status;

  if (self->grab_window == window)
    {
      g_debug ("Window 0x%lx is already grabbed, skipping",
               gdk_x11_window_get_xid (window));
      return TRUE;
    }

  if (self->grab_window != NULL)
    {
      g_debug ("Moving grab from 0x%lx to 0x%lx",
               gdk_x11_window_get_xid (self->grab_window),
               gdk_x11_window_get_xid (window));
    }
  else
    {
      g_debug ("Grabbing window 0x%lx", gdk_x11_window_get_xid (window));
    }

  display = gdk_window_get_display (window);

  g_debug ("Grabbing X server");
  gdk_x11_display_grab (display);

  old_window = self->grab_window;

  gf_grab_release (self);
  status = grab (self, window);

  if (status != GDK_GRAB_SUCCESS)
    {
      sleep (1);
      status = grab (self, window);
    }

  if (status != GDK_GRAB_SUCCESS && old_window != NULL)
    {
      g_debug ("Could not grab new window. Resuming previous grab.");
      if (grab (self, window) != GDK_GRAB_SUCCESS)
        g_debug ("Could not resume previous grab");
    }

  g_debug ("Ungrabbing X server");
  gdk_x11_display_ungrab (display);
  gdk_display_flush (display);

  return (status == GDK_GRAB_SUCCESS);
}

static void
gf_grab_finalize (GObject *object)
{
  GfGrab *self;

  self = GF_GRAB (object);

  g_clear_pointer (&self->invisible, gtk_widget_destroy);

  G_OBJECT_CLASS (gf_grab_parent_class)->finalize (object);
}

static void
gf_grab_class_init (GfGrabClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->finalize = gf_grab_finalize;
}

static void
gf_grab_init (GfGrab *self)
{
  self->invisible = gtk_invisible_new ();
  gtk_widget_show (self->invisible);
}

GfGrab *
gf_grab_new (void)
{
  return g_object_new (GF_TYPE_GRAB, NULL);
}

/* this is used to grab the keyboard and mouse to the root */
gboolean
gf_grab_grab_root (GfGrab *self)
{
  GdkDisplay *display;
  GdkScreen *screen;
  GdkWindow *window;

  g_debug ("Grabbing the root window");

  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);
  window = gdk_screen_get_root_window (screen);

  return grab_window (self, window);
}

/* this is used to grab the keyboard and mouse to an offscreen window */
gboolean
gf_grab_grab_offscreen (GfGrab *self)
{
  GdkWindow *window;

  window = gtk_widget_get_window (self->invisible);

  return grab_window (self, window);
}

void
gf_grab_move_to_window (GfGrab    *self,
                        GdkWindow *window)
{
  GdkDisplay *display;
  gboolean result;

  display = gdk_window_get_display (window);

  do
    {
      result = grab_move (self, window);
      gdk_display_flush (display);
    }
  while (!result);
}

void
gf_grab_release (GfGrab *self)
{
  GdkDisplay *display;
  GdkSeat *seat;

  g_debug ("Releasing all grabs");

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);

  grab_window_reset (self);
  gdk_seat_ungrab (seat);

  gdk_display_sync (display);
  gdk_display_flush (display);
}
