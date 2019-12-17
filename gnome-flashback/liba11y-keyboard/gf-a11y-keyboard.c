/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2018 Alberts Muktupāvels
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
#include "gf-a11y-keyboard.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>

#include <X11/XKBlib.h>
#include <X11/extensions/XKBstr.h>

#include "dbus/gf-fd-notifications-gen.h"

#define KEYBOARD_A11Y_SCHEMA "org.gnome.desktop.a11y.keyboard"

#define DEFAULT_XKB_SET_CONTROLS_MASK XkbSlowKeysMask        | \
                                      XkbBounceKeysMask      | \
                                      XkbStickyKeysMask      | \
                                      XkbMouseKeysMask       | \
                                      XkbMouseKeysAccelMask  | \
                                      XkbAccessXKeysMask     | \
                                      XkbAccessXTimeoutMask  | \
                                      XkbAccessXFeedbackMask | \
                                      XkbControlsEnabledMask

struct _GfA11yKeyboard
{
  GObject               parent;

  guint                 start_idle_id;
  int                   xkbEventBase;
  guint                 device_added_id;
  gboolean              stickykeys_shortcut_val;
  gboolean              slowkeys_shortcut_val;

  XkbDescRec           *desc;

  GSettings            *settings;

  GfFdNotificationsGen *notifications;
  guint                 slowkeys_id;
  guint                 stickykeys_id;
};

G_DEFINE_TYPE (GfA11yKeyboard, gf_a11y_keyboard, G_TYPE_OBJECT)

static int
get_int (GSettings  *settings,
         const char *key)
{
  int res;

  res = g_settings_get_int (settings, key);
  if (res <= 0)
    res = 1;

  return res;
}

static unsigned long
set_clear (gboolean      flag,
           unsigned long value,
           unsigned long mask)
{
  if (flag)
    return value | mask;

  return value & ~mask;
}

static gboolean
set_ctrl_from_gsettings (XkbDescRec    *desc,
                         GSettings     *settings,
                         char const    *key,
                         unsigned long  mask)
{
  gboolean result;

  result = g_settings_get_boolean (settings, key);
  desc->ctrls->enabled_ctrls = set_clear (result, desc->ctrls->enabled_ctrls, mask);

  return result;
}

static XkbDescRec *
get_xkb_desc_rec (void)
{
  GdkDisplay *display;
  Display *xdisplay;
  XkbDescRec *desc;
  Status status;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  gdk_x11_display_error_trap_push (display);

  desc = XkbGetMap (xdisplay, XkbAllMapComponentsMask, XkbUseCoreKbd);
  status = Success;

  if (desc != NULL)
    {
      desc->ctrls = NULL;
      status = XkbGetControls (xdisplay, XkbAllControlsMask, desc);
    }

  gdk_x11_display_error_trap_pop_ignored (display);

  g_return_val_if_fail (desc != NULL, NULL);
  g_return_val_if_fail (desc->ctrls != NULL, NULL);
  g_return_val_if_fail (status == Success, NULL);

  return desc;
}

static void
set_server_from_gsettings (GfA11yKeyboard *a11y_keyboard)
{
  XkbDescRec *desc;
  GSettings *settings;
  GdkDisplay *display;
  Display *xdisplay;

  desc = get_xkb_desc_rec ();
  if (!desc)
    return;

  settings = a11y_keyboard->settings;

  /* general */
  desc->ctrls->enabled_ctrls = set_clear (g_settings_get_boolean (settings, "enable"),
                                          desc->ctrls->enabled_ctrls,
                                          XkbAccessXKeysMask);

  if (set_ctrl_from_gsettings (desc, settings, "timeout-enable",
                               XkbAccessXTimeoutMask))
    {
      desc->ctrls->ax_timeout = get_int (settings, "disable-timeout");
      /* disable only the master flag via the server we will disable
       * the rest on the rebound without affecting GSettings state
       * don't change the option flags at all.
       */
      desc->ctrls->axt_ctrls_mask = XkbAccessXKeysMask | XkbAccessXFeedbackMask;
      desc->ctrls->axt_ctrls_values = 0;
      desc->ctrls->axt_opts_mask = 0;
    }

  desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "feature-state-change-beep"),
                                       desc->ctrls->ax_options,
                                       XkbAccessXFeedbackMask | XkbAX_FeatureFBMask | XkbAX_SlowWarnFBMask);

  /* bounce keys */
  if (set_ctrl_from_gsettings (desc, settings, "bouncekeys-enable", XkbBounceKeysMask))
    {
      desc->ctrls->debounce_delay = get_int (settings, "bouncekeys-delay");
      desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "bouncekeys-beep-reject"),
                                           desc->ctrls->ax_options,
                                           XkbAccessXFeedbackMask | XkbAX_BKRejectFBMask);
    }

  /* mouse keys */
  if (set_ctrl_from_gsettings (desc, settings, "mousekeys-enable", XkbMouseKeysMask | XkbMouseKeysAccelMask))
    {
      desc->ctrls->mk_interval = 100; /* msec between mousekey events */
      desc->ctrls->mk_curve = 50;

      /* We store pixels / sec, XKB wants pixels / event */
      desc->ctrls->mk_max_speed = get_int (settings, "mousekeys-max-speed") / (1000 / desc->ctrls->mk_interval);
      if (desc->ctrls->mk_max_speed <= 0)
        desc->ctrls->mk_max_speed = 1;

      desc->ctrls->mk_time_to_max = get_int (settings, /* events before max */
                                             "mousekeys-accel-time") / desc->ctrls->mk_interval;
      if (desc->ctrls->mk_time_to_max <= 0)
        desc->ctrls->mk_time_to_max = 1;

      desc->ctrls->mk_delay = get_int (settings, /* ms before 1st event */
                                       "mousekeys-init-delay");
    }


  /* slow keys */
  if (set_ctrl_from_gsettings (desc, settings, "slowkeys-enable", XkbSlowKeysMask))
    {
      desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "slowkeys-beep-press"),
                                           desc->ctrls->ax_options,
                                           XkbAccessXFeedbackMask | XkbAX_SKPressFBMask);
      desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "slowkeys-beep-accept"),
                                           desc->ctrls->ax_options,
                                           XkbAccessXFeedbackMask | XkbAX_SKAcceptFBMask);
      desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "slowkeys-beep-reject"),
                                           desc->ctrls->ax_options,
                                           XkbAccessXFeedbackMask | XkbAX_SKRejectFBMask);
      desc->ctrls->slow_keys_delay = get_int (settings, "slowkeys-delay");
      /* anything larger than 500 seems to loose all keyboard input */
      if (desc->ctrls->slow_keys_delay > 500)
        desc->ctrls->slow_keys_delay = 500;
    }

  /* sticky keys */
  if (set_ctrl_from_gsettings (desc, settings, "stickykeys-enable", XkbStickyKeysMask))
    {
      desc->ctrls->ax_options |= XkbAX_LatchToLockMask;
      desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "stickykeys-two-key-off"),
                                           desc->ctrls->ax_options,
                                           XkbAccessXFeedbackMask | XkbAX_TwoKeysMask);
      desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "stickykeys-modifier-beep"),
                                           desc->ctrls->ax_options,
                                           XkbAccessXFeedbackMask | XkbAX_StickyKeysFBMask);
    }

  /* toggle keys */
  desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "togglekeys-enable"),
                                       desc->ctrls->ax_options,
                                       XkbAccessXFeedbackMask | XkbAX_IndicatorFBMask);

  /*
  g_debug ("CHANGE to : 0x%x", desc->ctrls->enabled_ctrls);
  g_debug ("CHANGE to : 0x%x (2)", desc->ctrls->ax_options);
  */

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  gdk_x11_display_error_trap_push (display);

  XkbSetControls (xdisplay, DEFAULT_XKB_SET_CONTROLS_MASK, desc);
  XkbFreeKeyboard (desc, XkbAllComponentsMask, True);
  XSync (xdisplay, False);

  gdk_x11_display_error_trap_pop_ignored (display);
}

static void
slowkeys_notify_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  GError *error;
  guint id;
  GfA11yKeyboard *a11y_keyboard;

  error = NULL;
  gf_fd_notifications_gen_call_notify_finish (GF_FD_NOTIFICATIONS_GEN (source_object),
                                              &id, res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }
  else if (error != NULL)
    {
      g_warning ("a11y-keyboard: unable to show notification: %s", error->message);
      g_error_free (error);

      return;
    }

  a11y_keyboard = GF_A11Y_KEYBOARD (user_data);
  a11y_keyboard->slowkeys_id = id;
}

static void
ax_slowkeys_warning_post_bubble (GfA11yKeyboard *a11y_keyboard,
                                 gboolean        enabled)
{
  const char *icon;
  const char *title;
  const char *message;
  const char **actions;
  GVariantBuilder hints;

  icon = "preferences-desktop-accessibility-symbolic";

  title = enabled ?
          _("Slow Keys Turned On") :
          _("Slow Keys Turned Off");

  message = _("You just held down the Shift key for 8 seconds. This is the shortcut "
              "for the Slow Keys feature, which affects the way your keyboard works.");

  actions = g_new0 (const char *, 5);
  actions[0] = "reject";
  actions[1] = enabled ? _("Turn Off") : _("Turn On");
  actions[2] = "accept";
  actions[3] = enabled ? _("Leave On") : _("Leave Off");
  actions[4] = NULL;

  g_variant_builder_init (&hints, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&hints, "{sv}", "urgency",
                         g_variant_new_byte (0x02 /* critical */));

  gf_fd_notifications_gen_call_notify (a11y_keyboard->notifications,
                                       _("Universal Access"),
                                       0,
                                       icon,
                                       title,
                                       message,
                                       actions,
                                       g_variant_builder_end (&hints),
                                       0,
                                       NULL,
                                       slowkeys_notify_cb,
                                       a11y_keyboard);

  g_free (actions);
}

static void
ax_slowkeys_warning_post (GfA11yKeyboard *a11y_keyboard,
                          gboolean        enabled)
{
  a11y_keyboard->slowkeys_shortcut_val = enabled;
  ax_slowkeys_warning_post_bubble (a11y_keyboard, enabled);
}

static void
stickykeys_notify_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GError *error;
  guint id;
  GfA11yKeyboard *a11y_keyboard;

  error = NULL;
  gf_fd_notifications_gen_call_notify_finish (GF_FD_NOTIFICATIONS_GEN (source_object),
                                              &id, res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }
  else if (error != NULL)
    {
      g_warning ("a11y-keyboard: unable to show notification: %s", error->message);
      g_error_free (error);

      return;
    }

  a11y_keyboard = GF_A11Y_KEYBOARD (user_data);
  a11y_keyboard->stickykeys_id = id;
}

static void
ax_stickykeys_warning_post_bubble (GfA11yKeyboard *a11y_keyboard,
                                   gboolean        enabled)
{
  const char *icon;
  const char *title;
  const char *message;
  const char **actions;
  GVariantBuilder hints;

  icon = "preferences-desktop-accessibility-symbolic";

  title = enabled ?
          _("Sticky Keys Turned On") :
          _("Sticky Keys Turned Off");

  message = enabled ?
            _("You just pressed the Shift key 5 times in a row.  This is the shortcut "
              "for the Sticky Keys feature, which affects the way your keyboard works.") :
            _("You just pressed two keys at once, or pressed the Shift key 5 times in a row. "
              "This turns off the Sticky Keys feature, which affects the way your keyboard works.");

  actions = g_new0 (const char *, 5);
  actions[0] = "reject";
  actions[1] = enabled ? _("Turn Off") : _("Turn On");
  actions[2] = "accept";
  actions[3] = enabled ? _("Leave On") : _("Leave Off");
  actions[4] = NULL;

  g_variant_builder_init (&hints, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&hints, "{sv}", "urgency",
                         g_variant_new_byte (0x02 /* critical */));

  gf_fd_notifications_gen_call_notify (a11y_keyboard->notifications,
                                       _("Universal Access"),
                                       0,
                                       icon,
                                       title,
                                       message,
                                       actions,
                                       g_variant_builder_end (&hints),
                                       0,
                                       NULL,
                                       stickykeys_notify_cb,
                                       a11y_keyboard);

  g_free (actions);
}

static void
ax_stickykeys_warning_post (GfA11yKeyboard *a11y_keyboard,
                            gboolean        enabled)
{

  a11y_keyboard->stickykeys_shortcut_val = enabled;
  ax_stickykeys_warning_post_bubble (a11y_keyboard, enabled);
}

static gboolean
set_bool (GSettings  *settings,
          const char *key,
          int         val)
{
  gboolean bval;
  gboolean prev_val;

  bval = (val != 0);
  prev_val = g_settings_get_boolean (settings, key);

  g_settings_set_boolean (settings, key, bval ? TRUE : FALSE);

  if (bval != prev_val)
    {
      g_debug ("%s changed", key);
      return TRUE;
    }

  return bval != prev_val;
}

static gboolean
set_int (GSettings  *settings,
         const char *key,
         int         val)
{
  int prev_val;

  prev_val = g_settings_get_int (settings, key);

  g_settings_set_int (settings, key, val);

  if (val != prev_val)
    g_debug ("%s changed", key);

  return val != prev_val;
}

static void
set_gsettings_from_server (GfA11yKeyboard *a11y_keyboard)
{
  XkbDescRec *desc;
  GSettings *settings;
  gboolean changed;
  gboolean slowkeys_changed;
  gboolean stickykeys_changed;

  desc = get_xkb_desc_rec ();
  if (!desc)
    return;

  /* Create a new one, so that only those settings are delayed */
  settings = g_settings_new (KEYBOARD_A11Y_SCHEMA);
  g_settings_delay (settings);

  /*
  fprintf (stderr, "changed to : 0x%x\n", desc->ctrls->enabled_ctrls);
  fprintf (stderr, "changed to : 0x%x (2)\n", desc->ctrls->ax_options);
  */

  changed = FALSE;

  changed |= set_bool (settings, "enable",
                       desc->ctrls->enabled_ctrls & XkbAccessXKeysMask);

  changed |= set_bool (settings, "feature-state-change-beep",
                       desc->ctrls->ax_options & (XkbAX_FeatureFBMask | XkbAX_SlowWarnFBMask));

  changed |= set_bool (settings, "timeout-enable",
                       desc->ctrls->enabled_ctrls & XkbAccessXTimeoutMask);

  changed |= set_int (settings, "disable-timeout",
                      desc->ctrls->ax_timeout);

  changed |= set_bool (settings, "bouncekeys-enable",
                       desc->ctrls->enabled_ctrls & XkbBounceKeysMask);

  changed |= set_int (settings, "bouncekeys-delay",
                      desc->ctrls->debounce_delay);

  changed |= set_bool (settings, "bouncekeys-beep-reject",
                       desc->ctrls->ax_options & XkbAX_BKRejectFBMask);

  changed |= set_bool (settings, "mousekeys-enable",
                       desc->ctrls->enabled_ctrls & XkbMouseKeysMask);

  changed |= set_int (settings, "mousekeys-max-speed",
                      desc->ctrls->mk_max_speed * (1000 / desc->ctrls->mk_interval));

  /* NOTE : mk_time_to_max is measured in events not time */
  changed |= set_int (settings, "mousekeys-accel-time",
                      desc->ctrls->mk_time_to_max * desc->ctrls->mk_interval);

  changed |= set_int (settings, "mousekeys-init-delay",
                      desc->ctrls->mk_delay);

  slowkeys_changed = set_bool (settings, "slowkeys-enable",
                               desc->ctrls->enabled_ctrls & XkbSlowKeysMask);

  changed |= set_bool (settings, "slowkeys-beep-press",
                       desc->ctrls->ax_options & XkbAX_SKPressFBMask);

  changed |= set_bool (settings, "slowkeys-beep-accept",
                       desc->ctrls->ax_options & XkbAX_SKAcceptFBMask);

  changed |= set_bool (settings, "slowkeys-beep-reject",
                       desc->ctrls->ax_options & XkbAX_SKRejectFBMask);

  changed |= set_int (settings, "slowkeys-delay",
                      desc->ctrls->slow_keys_delay);

  stickykeys_changed = set_bool (settings, "stickykeys-enable",
                                 desc->ctrls->enabled_ctrls & XkbStickyKeysMask);

  changed |= set_bool (settings, "stickykeys-two-key-off",
                       desc->ctrls->ax_options & XkbAX_TwoKeysMask);

  changed |= set_bool (settings, "stickykeys-modifier-beep",
                       desc->ctrls->ax_options & XkbAX_StickyKeysFBMask);

  changed |= set_bool (settings, "togglekeys-enable",
                       desc->ctrls->ax_options & XkbAX_IndicatorFBMask);

  if (!changed && stickykeys_changed ^ slowkeys_changed)
    {
      /*
       * sticky or slowkeys has changed, singly, without our intervention.
       * 99% chance this is due to a keyboard shortcut being used.
       * we need to detect via this hack until we get
       * XkbAXN_AXKWarning notifications working (probable XKB bug),
       * at which time we can directly intercept such shortcuts instead.
       * See xkb_event_filter_cb () below.
       */

      /* sanity check: are keyboard shortcuts available? */
      if (desc->ctrls->enabled_ctrls & XkbAccessXKeysMask)
        {
          gboolean enabled;

          if (slowkeys_changed)
            {
              enabled = desc->ctrls->enabled_ctrls & XkbSlowKeysMask;
              ax_slowkeys_warning_post (a11y_keyboard, enabled);
            }
          else
            {
              enabled = desc->ctrls->enabled_ctrls & XkbStickyKeysMask;
              ax_stickykeys_warning_post (a11y_keyboard, enabled);
            }
        }
    }

  XkbFreeKeyboard (desc, XkbAllComponentsMask, True);

  g_settings_apply (settings);
  g_object_unref (settings);
}

static GdkFilterReturn
xkb_event_filter_cb (GdkXEvent *xevent,
                     GdkEvent  *event,
                     gpointer   user_data)
{
  GfA11yKeyboard *a11y_keyboard;
  XEvent *xev;
  XkbEvent *xkbEv;

  a11y_keyboard = GF_A11Y_KEYBOARD (user_data);
  xev = (XEvent *) xevent;
  xkbEv = (XkbEvent *) xevent;

  /* 'event_type' is set to zero on notifying us of updates in
   * response to client requests (including our own) and non-zero
   * to notify us of key/mouse events causing changes (like
   * pressing shift 5 times to enable sticky keys).
   *
   * We only want to update GSettings when it's in response to an
   * explicit user input event, so require a non-zero event_type.
   */
  if (xev->xany.type == (a11y_keyboard->xkbEventBase + XkbEventCode) &&
      xkbEv->any.xkb_type == XkbControlsNotify &&
      xkbEv->ctrls.event_type != 0)
    {
      g_debug ("XKB state changed");
      set_gsettings_from_server (a11y_keyboard);
    }
  else if (xev->xany.type == (a11y_keyboard->xkbEventBase + XkbEventCode) &&
           xkbEv->any.xkb_type == XkbAccessXNotify)
    {
      if (xkbEv->accessx.detail == XkbAXN_AXKWarning)
        {
          g_debug ("About to turn on an AccessX feature from the keyboard!");

          /*
           * TODO: when XkbAXN_AXKWarnings start working, we need to
           * invoke ax_keys_warning_dialog_run here instead of in
           * set_gsettings_from_server().
           */
        }
    }

  return GDK_FILTER_CONTINUE;
}

static gboolean
xkb_enabled (GfA11yKeyboard *a11y_keyboard)
{
  GdkDisplay *display;
  Display *xdisplay;
  int opcode, errorBase, major, minor;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  if (!XkbQueryExtension (xdisplay,
                          &opcode,
                          &a11y_keyboard->xkbEventBase,
                          &errorBase,
                          &major,
                          &minor))
    return FALSE;

  if (!XkbUseExtension (xdisplay, &major, &minor))
    return FALSE;

  return TRUE;
}

static void
keyboard_settings_changed_cb (GSettings      *settings,
                              const char     *key,
                              GfA11yKeyboard *a11y_keyboard)
{
  set_server_from_gsettings (a11y_keyboard);
}

static void
device_added_cb (GdkSeat        *seat,
                 GdkDevice      *device,
                 GfA11yKeyboard *a11y_keyboard)
{
  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    set_server_from_gsettings (a11y_keyboard);
}

static void
set_devicepresence_handler (GfA11yKeyboard *a11y_keyboard)
{
  GdkDisplay *display;
  GdkSeat *seat;

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);

  a11y_keyboard->device_added_id = g_signal_connect (seat, "device-added",
                                                     G_CALLBACK (device_added_cb),
                                                     a11y_keyboard);
}

static gboolean
start_a11y_keyboard_idle_cb (gpointer user_data)
{
  GfA11yKeyboard *a11y_keyboard;
  GdkDisplay *display;
  Display *xdisplay;
  guint event_mask;

  a11y_keyboard = GF_A11Y_KEYBOARD (user_data);

  if (!xkb_enabled (a11y_keyboard))
    {
      a11y_keyboard->start_idle_id = 0;
      return G_SOURCE_REMOVE;
    }

  a11y_keyboard->settings = g_settings_new (KEYBOARD_A11Y_SCHEMA);
  g_signal_connect (a11y_keyboard->settings, "changed",
                    G_CALLBACK (keyboard_settings_changed_cb),
                    a11y_keyboard);

  set_devicepresence_handler (a11y_keyboard);

  /* Get the original configuration from the server */
  a11y_keyboard->desc = get_xkb_desc_rec ();

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  event_mask = XkbControlsNotifyMask;
  event_mask |= XkbAccessXNotifyMask; /* make default when AXN_AXKWarning works */

  /* be sure to init before starting to monitor the server */
  set_server_from_gsettings (a11y_keyboard);

  XkbSelectEvents (xdisplay, XkbUseCoreKbd, event_mask, event_mask);

  gdk_window_add_filter (NULL, xkb_event_filter_cb, a11y_keyboard);

  a11y_keyboard->start_idle_id = 0;
  return G_SOURCE_REMOVE;
}

static void
ax_response_callback (GfA11yKeyboard *a11y_keyboard,
                      const char     *action,
                      guint           revert_controls_mask,
                      gboolean        enabled)
{
  if (g_strcmp0 (action, "reject") != 0)
    return;

  /* we're reverting, so we invert sense of 'enabled' flag */
  g_debug ("cancelling AccessX request");

  if (revert_controls_mask == XkbStickyKeysMask)
    {
      g_settings_set_boolean (a11y_keyboard->settings,
                              "stickykeys-enable",
                              !enabled);
    }
  else if (revert_controls_mask == XkbSlowKeysMask)
    {
      g_settings_set_boolean (a11y_keyboard->settings,
                              "slowkeys-enable",
                              !enabled);
    }

  set_server_from_gsettings (a11y_keyboard);
}

static void
action_invoked_cb (GfFdNotificationsGen *notifications,
                   guint                 id,
                   const gchar          *action_key,
                   GfA11yKeyboard       *a11y_keyboard)
{
  if (id == a11y_keyboard->slowkeys_id)
    {
      g_assert (action_key != NULL);

      ax_response_callback (a11y_keyboard, action_key, XkbSlowKeysMask,
                            a11y_keyboard->slowkeys_shortcut_val);
    }
  else if (id == a11y_keyboard->stickykeys_id)
    {
      g_assert (action_key != NULL);

      ax_response_callback (a11y_keyboard, action_key, XkbStickyKeysMask,
                            a11y_keyboard->stickykeys_shortcut_val);
    }
}

static void
notification_closed_cb (GfFdNotificationsGen *notifications,
                        guint                 id,
                        guint                 reason,
                        GfA11yKeyboard       *a11y_keyboard)
{
  if (id == a11y_keyboard->slowkeys_id)
    a11y_keyboard->slowkeys_id = 0;
  else if (id == a11y_keyboard->stickykeys_id)
    a11y_keyboard->stickykeys_id = 0;
}

static void
notifications_proxy_ready_cb (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  GError *error;
  GfFdNotificationsGen *notifications;
  GfA11yKeyboard *a11y_keyboard;

  error = NULL;
  notifications = gf_fd_notifications_gen_proxy_new_for_bus_finish (res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  a11y_keyboard = GF_A11Y_KEYBOARD (user_data);
  a11y_keyboard->notifications = notifications;

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return;
    }

  g_signal_connect (a11y_keyboard->notifications, "action-invoked",
                    G_CALLBACK (action_invoked_cb), a11y_keyboard);

  g_signal_connect (a11y_keyboard->notifications, "notification-closed",
                    G_CALLBACK (notification_closed_cb), a11y_keyboard);
}

static void
gf_a11y_keyboard_finalize (GObject *object)
{
  GfA11yKeyboard *a11y_keyboard;

  a11y_keyboard = GF_A11Y_KEYBOARD (object);

  if (a11y_keyboard->desc != NULL)
    {
      XkbDescRec *desc;

      desc = get_xkb_desc_rec ();

      if (desc != NULL)
        {
          if (a11y_keyboard->desc->ctrls->enabled_ctrls != desc->ctrls->enabled_ctrls)
            {
              GdkDisplay *display;
              Display *xdisplay;

              display = gdk_display_get_default ();
              xdisplay = gdk_x11_display_get_xdisplay (display);

              gdk_x11_display_error_trap_push (display);

              XkbSetControls (xdisplay,
                              DEFAULT_XKB_SET_CONTROLS_MASK,
                              a11y_keyboard->desc);

              XSync (xdisplay, False);

              gdk_x11_display_error_trap_pop_ignored (display);
            }

          XkbFreeKeyboard (desc, XkbAllComponentsMask, True);
        }

      XkbFreeKeyboard (a11y_keyboard->desc, XkbAllComponentsMask, True);
      a11y_keyboard->desc = NULL;
    }

  if (a11y_keyboard->start_idle_id != 0)
    {
      g_source_remove (a11y_keyboard->start_idle_id);
      a11y_keyboard->start_idle_id = 0;
    }

  if (a11y_keyboard->device_added_id != 0)
    {
      GdkDisplay *display;
      GdkSeat *seat;

      display = gdk_display_get_default ();
      seat = gdk_display_get_default_seat (display);

      g_signal_handler_disconnect (seat, a11y_keyboard->device_added_id);
      a11y_keyboard->device_added_id = 0;
    }

  g_clear_object (&a11y_keyboard->settings);

  gdk_window_remove_filter (NULL, xkb_event_filter_cb, a11y_keyboard);

  a11y_keyboard->slowkeys_shortcut_val = FALSE;
  a11y_keyboard->stickykeys_shortcut_val = FALSE;

  if (a11y_keyboard->slowkeys_id != 0)
    {
      gf_fd_notifications_gen_call_close_notification (a11y_keyboard->notifications,
                                                       a11y_keyboard->slowkeys_id,
                                                       NULL, NULL, NULL);
      a11y_keyboard->slowkeys_id = 0;
    }

  if (a11y_keyboard->stickykeys_id != 0)
    {
      gf_fd_notifications_gen_call_close_notification (a11y_keyboard->notifications,
                                                       a11y_keyboard->stickykeys_id,
                                                       NULL, NULL, NULL);
      a11y_keyboard->stickykeys_id = 0;
    }

  g_clear_object (&a11y_keyboard->notifications);

  G_OBJECT_CLASS (gf_a11y_keyboard_parent_class)->finalize (object);
}

static void
gf_a11y_keyboard_class_init (GfA11yKeyboardClass *a11y_keyboard_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (a11y_keyboard_class);

  object_class->finalize = gf_a11y_keyboard_finalize;
}

static void
gf_a11y_keyboard_init (GfA11yKeyboard *a11y_keyboard)
{
  a11y_keyboard->start_idle_id = g_idle_add (start_a11y_keyboard_idle_cb, a11y_keyboard);
  g_source_set_name_by_id (a11y_keyboard->start_idle_id, "[gnome-flashback] start_a11y_keyboard_idle_cb");

  gf_fd_notifications_gen_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                             G_DBUS_PROXY_FLAGS_NONE,
                                             "org.freedesktop.Notifications",
                                             "/org/freedesktop/Notifications",
                                             NULL,
                                             notifications_proxy_ready_cb,
                                             a11y_keyboard);
}

GfA11yKeyboard *
gf_a11y_keyboard_new (void)
{
  return g_object_new (GF_TYPE_A11Y_KEYBOARD, NULL);
}
