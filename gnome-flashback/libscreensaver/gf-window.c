/*
 * Copyright (C) 2004-2008 William Jon McCann
 * Copyright (C) 2008-2011 Red Hat, Inc
 * Copyright (C) 2020 Alberts Muktupāvels
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
#include "gf-window.h"

#include <gdk/gdkx.h>

#include "gf-info-bar.h"
#include "gf-panel.h"
#include "gf-unlock-dialog.h"

#define MAX_QUEUED_EVENTS 16

struct _GfWindow
{
  GtkWindow        parent;

  GtkWidget       *vbox;
  GtkWidget       *panel;
  GtkWidget       *info_bar;
  GtkWidget       *unlock_dialog;

  GdkMonitor      *monitor;

  cairo_surface_t *surface;

  GfInputSources  *input_sources;

  GList           *key_events;

  gboolean         lock_enabled;
  gboolean         user_switch_enabled;

  gboolean         dialog_up;

  guint            emit_deactivated_idle_id;

  guint            popup_dialog_idle_id;

  double           last_x;
  double           last_y;
};

enum
{
  PROP_0,

  PROP_MONITOR,

  LAST_PROP
};

static GParamSpec *window_properties[LAST_PROP] = { NULL };

enum
{
  ACTIVITY,
  DEACTIVATED,

  SIZE_CHANGED,

  DIALOG_UP,

  LAST_SIGNAL
};

static guint window_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfWindow, gf_window, GTK_TYPE_WINDOW)

static void
set_invisible_cursor (GfWindow *self,
                      gboolean  invisible)
{
  GdkDisplay *display;
  GdkWindow *window;
  GdkCursor *cursor;

  display = gtk_widget_get_display (GTK_WIDGET (self));
  window = gtk_widget_get_window (GTK_WIDGET (self));

  cursor = NULL;
  if (invisible)
    cursor = gdk_cursor_new_for_display (display, GDK_BLANK_CURSOR);

  gdk_window_set_cursor (window, cursor);
  g_clear_object (&cursor);
}

static gboolean
maybe_emit_activity (GfWindow *self)
{
  if (self->unlock_dialog != NULL ||
      !gtk_widget_get_sensitive (GTK_WIDGET (self)))
    return FALSE;

  g_signal_emit (self, window_signals[ACTIVITY], 0);

  return TRUE;
}

static gboolean
emit_deactivated_idle_cb (gpointer user_data)
{
  GfWindow *self;

  self = GF_WINDOW (user_data);

  g_signal_emit (self, window_signals[DEACTIVATED], 0);
  self->emit_deactivated_idle_id = 0;

  return G_SOURCE_REMOVE;
}

static void
emit_deactivated_idle (GfWindow *self)
{
  if (self->emit_deactivated_idle_id != 0)
    return;

  self->emit_deactivated_idle_id = g_idle_add (emit_deactivated_idle_cb, self);
  g_source_set_name_by_id (self->emit_deactivated_idle_id,
                           "[gnome-flashback] emit_deactivated_idle_cb");
}

static void
popdown_dialog (GfWindow *self)
{
  g_debug ("Popping down dialog");

  set_invisible_cursor (self, TRUE);

  if (self->dialog_up)
    {
      g_signal_emit (self, window_signals[DIALOG_UP], 0, FALSE);
      self->dialog_up = FALSE;
    }

  if (self->unlock_dialog != NULL)
    {
      gtk_container_remove (GTK_CONTAINER (self->vbox), self->unlock_dialog);
      self->unlock_dialog = NULL;
    }

  self->last_x = -1;
  self->last_y = -1;
}

static void
unlock_dialog_response_cb (GfUnlockDialog         *dialog,
                           GfUnlockDialogResponse  response,
                           GfWindow               *self)
{
  popdown_dialog (self);

  if (response == GF_UNLOCK_DIALOG_RESPONSE_OK)
    emit_deactivated_idle (self);
}

static void
unlock_dialog_close_cb (GfUnlockDialog *dialog,
                        GfWindow        *self)
{
  popdown_dialog (self);
}

static void
unlock_dialog_show_cb (GtkWidget *widget,
                       GfWindow  *self)
{
  GList *l;

  self->key_events = g_list_reverse (self->key_events);

  for (l = self->key_events; l != NULL; l = l->next)
    gf_unlock_dialog_forward_key_event (GF_UNLOCK_DIALOG (widget), l->data);

  g_list_free_full (self->key_events, (GDestroyNotify) gdk_event_free);
  self->key_events = NULL;
}

static void
popup_dialog (GfWindow *self)
{
  g_debug ("Popping up dialog");

  set_invisible_cursor (self, FALSE);

  g_assert (self->unlock_dialog == NULL);
  self->unlock_dialog = gf_unlock_dialog_new ();

  gf_unlock_dialog_set_input_sources (GF_UNLOCK_DIALOG (self->unlock_dialog),
                                      self->input_sources);

  gf_unlock_dialog_set_user_switch_enabled (GF_UNLOCK_DIALOG (self->unlock_dialog),
                                            self->user_switch_enabled);

  g_signal_connect (self->unlock_dialog, "response",
                    G_CALLBACK (unlock_dialog_response_cb), self);

  g_signal_connect (self->unlock_dialog, "close",
                    G_CALLBACK (unlock_dialog_close_cb), self);

  g_signal_connect (self->unlock_dialog, "show",
                    G_CALLBACK (unlock_dialog_show_cb), self);

  gtk_box_pack_start (GTK_BOX (self->vbox), self->unlock_dialog, TRUE, TRUE, 0);
  gdk_window_raise (gtk_widget_get_window (GTK_WIDGET (self)));
}

static gboolean
popup_dialog_idle_cb (gpointer user_data)
{
  GfWindow *self;

  self = GF_WINDOW (user_data);
  self->popup_dialog_idle_id = 0;

  popup_dialog (self);

  return G_SOURCE_REMOVE;
}

static void
move_resize (GfWindow *self)
{
  GdkRectangle rect;
  GdkWindow *window;

  gdk_monitor_get_geometry (self->monitor, &rect);

  g_debug ("Move and/or resize window: x - %d, y - %d, width - %d, height - %d",
           rect.x, rect.y, rect.width, rect.height);

  window = gtk_widget_get_window (GTK_WIDGET (self));
  gdk_window_move_resize (window, rect.x, rect.y, rect.width, rect.height);
}

static void
geometry_changed_cb (GdkMonitor *monitor,
                     GParamSpec *pspec,
                     GfWindow   *self)
{
  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  move_resize (self);

  g_signal_emit (self, window_signals[SIZE_CHANGED], 0);
}

static void
invalidate_cb (GdkMonitor *monitor,
               GfWindow   *self)
{
  GdkDisplay *display;
  GdkMonitor *primary_monitor;

  display = gdk_monitor_get_display (monitor);
  primary_monitor = gdk_display_get_primary_monitor (display);

  gtk_widget_set_visible (self->panel, primary_monitor == monitor);
}

static void
gf_window_constructed (GObject *object)
{
  GfWindow *self;

  self = GF_WINDOW (object);

  G_OBJECT_CLASS (gf_window_parent_class)->constructed (object);

  g_signal_connect_object (self->monitor, "notify::geometry",
                           G_CALLBACK (geometry_changed_cb),
                           self, 0);

  g_signal_connect_object (self->monitor, "invalidate",
                           G_CALLBACK (invalidate_cb),
                           self, 0);

  invalidate_cb (self->monitor, self);
}

static void
gf_window_finalize (GObject *object)
{
  GfWindow *self;

  self = GF_WINDOW (object);

  g_clear_pointer (&self->surface, cairo_surface_destroy);

  if (self->key_events != NULL)
    {
      g_list_free_full (self->key_events, (GDestroyNotify) gdk_event_free);
      self->key_events = NULL;
    }

  if (self->emit_deactivated_idle_id != 0)
    {
      g_source_remove (self->emit_deactivated_idle_id);
      self->emit_deactivated_idle_id = 0;
    }

  if (self->popup_dialog_idle_id != 0)
    {
      g_source_remove (self->popup_dialog_idle_id);
      self->popup_dialog_idle_id = 0;
    }

  G_OBJECT_CLASS (gf_window_parent_class)->finalize (object);
}

static void
gf_window_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GfWindow *self;

  self = GF_WINDOW (object);

  switch (property_id)
    {
      case PROP_MONITOR:
        g_assert (self->monitor == NULL);
        self->monitor = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static gboolean
gf_window_button_press_event (GtkWidget      *widget,
                              GdkEventButton *event)
{
  GfWindow *self;

  self = GF_WINDOW (widget);
  maybe_emit_activity (self);

  return GTK_WIDGET_CLASS (gf_window_parent_class)->button_press_event (widget,
                                                                        event);
}

static gboolean
gf_window_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  GfWindow *self;

  self = GF_WINDOW (widget);

  if (self->surface != NULL)
    {
      cairo_set_source_surface (cr, self->surface, 0, 0);
      cairo_paint (cr);
    }

  return GTK_WIDGET_CLASS (gf_window_parent_class)->draw (widget, cr);
}

static gboolean
gf_window_grab_broken_event (GtkWidget          *widget,
                             GdkEventGrabBroken *event)
{
  if (event->grab_window != NULL)
    {
      g_debug ("%s grab broken on window 0x%lx, new grab on window 0x%lx",
               event->keyboard ? "Keyboard" : "Pointer",
               gdk_x11_window_get_xid (event->window),
               gdk_x11_window_get_xid (event->grab_window));
    }
  else
    {
      g_debug ("%s grab broken on window 0x%lx, new grab is outside application",
               event->keyboard ? "Keyboard" : "Pointer",
               gdk_x11_window_get_xid (event->window));
    }

  return GTK_WIDGET_CLASS (gf_window_parent_class)->grab_broken_event (widget,
                                                                       event);
}

static gboolean
gf_window_key_press_event (GtkWidget   *widget,
                           GdkEventKey *event)
{
  GfWindow *self;

  self = GF_WINDOW (widget);

  if (maybe_emit_activity (self))
    {
      if (event->keyval == GDK_KEY_Return ||
          event->keyval == GDK_KEY_KP_Enter ||
          event->keyval == GDK_KEY_Escape ||
          event->keyval == GDK_KEY_space)
        return TRUE;
    }

  if (self->unlock_dialog == NULL ||
      !gtk_widget_is_visible (self->unlock_dialog))
    {
      /* Only cache MAX_QUEUED_EVENTS key events. If there are any more
       * than this then something is wrong.
       *
       * Don't queue keys that may cause focus navigation in the dialog.
       */
      if (g_list_length (self->key_events) < MAX_QUEUED_EVENTS &&
          event->keyval != GDK_KEY_Tab &&
          event->keyval != GDK_KEY_Up &&
          event->keyval != GDK_KEY_Down)
        {
          self->key_events = g_list_prepend (self->key_events,
                                             gdk_event_copy ((GdkEvent *) event));
        }
    }

  return GTK_WIDGET_CLASS (gf_window_parent_class)->key_press_event (widget,
                                                                     event);
}

static gboolean
gf_window_motion_notify_event (GtkWidget      *widget,
                               GdkEventMotion *event)
{
  GfWindow *self;

  self = GF_WINDOW (widget);

  /* if the last position was not set then don't detect motion */
  if (self->last_x < 0 || self->last_y < 0)
    {
      self->last_x = event->x;
      self->last_y = event->y;
    }
  else
    {
      GdkRectangle rect;
      double min_percentage;
      double min_distance;
      double distance;

      gdk_monitor_get_geometry (self->monitor, &rect);

      min_percentage = 0.1;
      min_distance = rect.width * min_percentage;

      /* just an approximate distance */
      distance = MAX (ABS (self->last_x - event->x),
                      ABS (self->last_y - event->y));

      if (distance > min_distance)
        {
          maybe_emit_activity (self);

          self->last_x = -1;
          self->last_y = -1;
        }
    }

  return GTK_WIDGET_CLASS (gf_window_parent_class)->motion_notify_event (widget,
                                                                         event);
}

static void
gf_window_realize (GtkWidget *widget)
{
  GfWindow *self;

  self = GF_WINDOW (widget);

  GTK_WIDGET_CLASS (gf_window_parent_class)->realize (widget);

  move_resize (self);
}

static gboolean
gf_window_scroll_event (GtkWidget      *widget,
                        GdkEventScroll *event)
{
  GfWindow *self;

  self = GF_WINDOW (widget);
  maybe_emit_activity (self);

  return GTK_WIDGET_CLASS (gf_window_parent_class)->scroll_event (widget,
                                                                  event);
}

static void
gf_window_show (GtkWidget *widget)
{
  GfWindow *self;

  self = GF_WINDOW (widget);

  GTK_WIDGET_CLASS (gf_window_parent_class)->show (widget);

  set_invisible_cursor (self, TRUE);
}

static void
install_properties (GObjectClass *object_class)
{
  window_properties[PROP_MONITOR] =
    g_param_spec_object ("monitor",
                         "monitor",
                         "monitor",
                         GDK_TYPE_MONITOR,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     window_properties);
}

static void
install_signals (void)
{
  window_signals[ACTIVITY] =
    g_signal_new ("activity", GF_TYPE_WINDOW, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[DEACTIVATED] =
    g_signal_new ("deactivated", GF_TYPE_WINDOW, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[SIZE_CHANGED] =
    g_signal_new ("size-changed", GF_TYPE_WINDOW, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  window_signals[DIALOG_UP] =
    g_signal_new ("dialog-up", GF_TYPE_WINDOW, G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
gf_window_class_init (GfWindowClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);

  object_class->constructed = gf_window_constructed;
  object_class->finalize = gf_window_finalize;
  object_class->set_property = gf_window_set_property;

  widget_class->button_press_event = gf_window_button_press_event;
  widget_class->draw = gf_window_draw;
  widget_class->grab_broken_event = gf_window_grab_broken_event;
  widget_class->key_press_event = gf_window_key_press_event;
  widget_class->motion_notify_event = gf_window_motion_notify_event;
  widget_class->realize = gf_window_realize;
  widget_class->scroll_event = gf_window_scroll_event;
  widget_class->show = gf_window_show;

  install_properties (object_class);
  install_signals ();
}

static void
gf_window_init (GfWindow *self)
{
  GdkEventMask events;

  events = GDK_POINTER_MOTION_MASK |
           GDK_BUTTON_PRESS_MASK |
           GDK_BUTTON_RELEASE_MASK |
           GDK_KEY_PRESS_MASK |
           GDK_KEY_RELEASE_MASK |
           GDK_SCROLL_MASK |
           GDK_SMOOTH_SCROLL_MASK |
           GDK_ENTER_NOTIFY_MASK |
           GDK_LEAVE_NOTIFY_MASK;

  gtk_widget_add_events (GTK_WIDGET (self), events);

  gtk_window_set_keep_above (GTK_WINDOW (self), TRUE);
  gtk_window_fullscreen (GTK_WINDOW (self));

  self->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (self), self->vbox);
  gtk_widget_show (self->vbox);

  self->panel = gf_panel_new ();
  gtk_box_pack_start (GTK_BOX (self->vbox), self->panel, FALSE, FALSE, 0);
  gtk_widget_show (self->panel);

  self->info_bar = gf_info_bar_new ();
  gtk_box_pack_start (GTK_BOX (self->vbox), self->info_bar, FALSE, FALSE, 0);

  self->last_x = -1;
  self->last_y = -1;
}

GfWindow *
gf_window_new (GdkMonitor *monitor)
{
  return g_object_new (GF_TYPE_WINDOW,
                       "app-paintable", TRUE,
                       "decorated", FALSE,
                       "monitor", monitor,
                       "skip-pager-hint", TRUE,
                       "skip-taskbar-hint", TRUE,
                       "type", GTK_WINDOW_POPUP,
                       NULL);
}

void
gf_window_set_input_sources (GfWindow       *self,
                             GfInputSources *input_sources)
{
  self->input_sources = input_sources;
}

GdkMonitor *
gf_window_get_monitor (GfWindow *self)
{
  return self->monitor;
}

void
gf_window_set_background (GfWindow        *self,
                          cairo_surface_t *surface)
{
  g_clear_pointer (&self->surface, cairo_surface_destroy);

  if (surface != NULL)
    self->surface = cairo_surface_reference (surface);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
gf_window_set_lock_enabled (GfWindow *self,
                            gboolean  lock_enabled)
{
  self->lock_enabled = lock_enabled;
}

void
gf_window_set_user_switch_enabled (GfWindow *self,
                                   gboolean  user_switch_enabled)
{
  self->user_switch_enabled = user_switch_enabled;
}

void
gf_window_show_message (GfWindow   *self,
                        const char *summary,
                        const char *body,
                        const char *icon)
{
  gf_info_bar_show_message (GF_INFO_BAR (self->info_bar), summary, body, icon);
}

void
gf_window_request_unlock (GfWindow *self)
{
  if (!gtk_widget_get_visible (GTK_WIDGET (self)))
    {
      g_debug ("Requesting unlock but window is not visible!");
      return;
    }

  if (!self->lock_enabled)
    {
      g_debug ("Requesting unlock but lock is not enabled!");
      emit_deactivated_idle (self);
      return;
    }

  if (self->dialog_up)
    {
      g_debug ("Requesting unlock but dialog is already up!");
      return;
    }

  if (self->popup_dialog_idle_id != 0)
    {
      g_debug ("Unlock request already requested!");
      return;
    }

  g_debug ("Requesting unlock");

  self->popup_dialog_idle_id = g_idle_add (popup_dialog_idle_cb, self);
  g_source_set_name_by_id (self->popup_dialog_idle_id,
                           "[gnome-flashback] popup_dialog_idle_cb");

  g_signal_emit (self, window_signals[DIALOG_UP], 0, TRUE);
  self->dialog_up = TRUE;
}

void
gf_window_cancel_unlock_request (GfWindow *self)
{
  g_debug ("Canceling unlock request");

  if (self->popup_dialog_idle_id != 0)
    {
      g_source_remove (self->popup_dialog_idle_id);
      self->popup_dialog_idle_id = 0;
    }

  popdown_dialog (self);
}
