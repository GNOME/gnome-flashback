/*
 * Copyright (C) 2004-2008 William Jon McCann
 * Copyright (C) 2008-2011 Red Hat, Inc.
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
#include "gf-unlock-dialog.h"

#include <gdm/gdm-user-switching.h>
#include <glib/gi18n.h>

#include "dbus/gf-dm-seat-gen.h"
#include "gf-auth.h"
#include "gf-user-image.h"
#include "gf-screensaver-enum-types.h"

#define DIALOG_TIMEOUT_MSEC 60000
#define MAX_FAILURES 5

struct _GfUnlockDialog
{
  GtkBox          parent;

  GCancellable   *cancellable;

  GfDmSeatGen    *dm_seat;

  GfAuth         *auth;
  gulong          auth_message_id;
  gulong          auth_complete_id;

  GtkWidget      *face_image;
  GtkWidget      *prompt_label;
  GtkWidget      *prompt_entry;
  GtkWidget      *capslock_label;

  GtkWidget      *message_label;

  GtkWidget      *buttons_box;
  GtkWidget      *switch_button;
  GtkWidget      *unlock_button;

  gboolean        user_switch_enabled;

  gulong          keymap_state_changed_id;

  guint           cancel_timeout_id;
  guint           emit_cancel_id;

  GtkWidget      *indicator_box;
  GtkWidget      *input_source_button;

  guint           failure_count;

  guint           shake_timeout_id;

  guint           reset_timeout_id;
};

enum
{
  RESPONSE,
  CLOSE,

  LAST_SIGNAL
};

static guint unlock_dialog_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfUnlockDialog, gf_unlock_dialog, GTK_TYPE_BOX)

static void
set_sensitive (GfUnlockDialog *self,
               gboolean        sensitive)
{
  gtk_widget_set_sensitive (self->prompt_entry, sensitive);
  gtk_widget_set_sensitive (self->indicator_box, sensitive);
  gtk_widget_set_sensitive (self->buttons_box, sensitive);
}

static void
set_message (GfUnlockDialog *self,
             const char     *message)
{
  if (message == NULL)
    message = "";

  gtk_label_set_text (GTK_LABEL (self->message_label), message);
}

static void
set_busy (GfUnlockDialog *self)
{
  GdkDisplay *display;
  GtkWidget *toplevel;
  GdkWindow *window;
  GdkCursor *cursor;

  display = gtk_widget_get_display (GTK_WIDGET (self));
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  window = gtk_widget_get_window (toplevel);

  cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
  gdk_window_set_cursor (window, cursor);
  g_clear_object (&cursor);
}

static void
set_ready (GfUnlockDialog *self)
{
  GdkDisplay *display;
  GtkWidget *toplevel;
  GdkWindow *window;
  GdkCursor *cursor;

  display = gtk_widget_get_display (GTK_WIDGET (self));
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  window = gtk_widget_get_window (toplevel);

  cursor = gdk_cursor_new_for_display (display, GDK_LEFT_PTR);
  gdk_window_set_cursor (window, cursor);
  g_clear_object (&cursor);
}

static gboolean
emit_cancel_cb (gpointer user_data)
{
  GfUnlockDialog *self;

  self = GF_UNLOCK_DIALOG (user_data);
  self->emit_cancel_id = 0;

  gtk_entry_set_text (GTK_ENTRY (self->prompt_entry), "");

  g_signal_emit (self, unlock_dialog_signals[RESPONSE], 0,
                 GF_UNLOCK_DIALOG_RESPONSE_CANCEL);

  return G_SOURCE_REMOVE;
}

static void
emit_cancel_remove (GfUnlockDialog *self)
{
  if (self->emit_cancel_id == 0)
    return;

  g_source_remove (self->emit_cancel_id);
  self->emit_cancel_id = 0;
}

static void
emit_cancel_add (GfUnlockDialog *self)
{
  g_assert (self->emit_cancel_id == 0);
  self->emit_cancel_id = g_timeout_add_seconds (2, emit_cancel_cb, self);

  g_source_set_name_by_id (self->emit_cancel_id,
                           "[gnome-flashback] emit_cancel_cb");
}

static void
switch_to_greeter_failed (GfUnlockDialog *self)
{
  set_sensitive (self, FALSE);
  set_message (self, _("Failed to switch to greeter!"));

  emit_cancel_add (self);
}

static void
switch_to_greeter_cb (GObject      *object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  GError *error;
  GfUnlockDialog *self;

  error = NULL;
  gf_dm_seat_gen_call_switch_to_greeter_finish (GF_DM_SEAT_GEN (object),
                                                res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);
          switch_to_greeter_failed (GF_UNLOCK_DIALOG (user_data));
        }

      g_error_free (error);
      return;
    }

  self = GF_UNLOCK_DIALOG (user_data);

  emit_cancel_add (self);
  set_ready (self);
}

static void
switch_user (GfUnlockDialog *self)
{
  if (self->dm_seat != NULL)
    {
      gf_dm_seat_gen_call_switch_to_greeter (self->dm_seat,
                                             self->cancellable,
                                             switch_to_greeter_cb,
                                             self);
    }
  else
    {
      GError *error;

      error = NULL;
      if (!gdm_goto_login_session_sync (NULL, &error))
        {
          g_warning ("Failed to switch to greeter: %s", error->message);
          switch_to_greeter_failed (self);
        }
      else
        {
          emit_cancel_add (self);
          set_ready (self);
        }

      g_clear_error (&error);
    }
}

static gboolean
cancel_timeout_cb (gpointer user_data)
{
  GfUnlockDialog *self;

  self = GF_UNLOCK_DIALOG (user_data);
  self->cancel_timeout_id = 0;

  set_sensitive (self, FALSE);
  set_message (self, _("Time has expired."));

  emit_cancel_remove (self);
  emit_cancel_add (self);

  return G_SOURCE_REMOVE;
}

static void
cancel_timeout_remove (GfUnlockDialog *self)
{
  if (self->cancel_timeout_id == 0)
    return;

  g_source_remove (self->cancel_timeout_id);
  self->cancel_timeout_id = 0;
}

static void
cancel_timeout_add (GfUnlockDialog *self)
{
  g_assert (self->cancel_timeout_id == 0);
  self->cancel_timeout_id = g_timeout_add (DIALOG_TIMEOUT_MSEC,
                                           cancel_timeout_cb,
                                           self);

  g_source_set_name_by_id (self->cancel_timeout_id,
                           "[gnome-flashback] cancel_timeout_cb");
}

static void
cancel_timeout_restart (GfUnlockDialog *self)
{
  cancel_timeout_remove (self);
  cancel_timeout_add (self);
}

static void
disable_prompt (GfUnlockDialog *self)
{
  set_sensitive (self, FALSE);
}

static void
enable_prompt (GfUnlockDialog *self,
               const char     *message,
               gboolean        visible)
{
  char *markup;

  g_debug ("Setting prompt to: %s", message);

  markup = g_strdup_printf ("<b><big>%s</big></b>", message);
  gtk_label_set_markup (GTK_LABEL (self->prompt_label), markup);
  gtk_widget_show (self->prompt_label);
  g_free (markup);

  gtk_entry_set_visibility (GTK_ENTRY (self->prompt_entry), visible);
  set_sensitive (self, TRUE);

  if (!gtk_widget_has_focus (self->prompt_entry))
    gtk_widget_grab_focus (self->prompt_entry);

  gtk_widget_grab_default (self->unlock_button);

  cancel_timeout_restart (self);
}

static void
clear_clipboards (GfUnlockDialog *self)
{
  GtkClipboard *clipboard;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self),
                                        GDK_SELECTION_PRIMARY);

  gtk_clipboard_clear (clipboard);
  gtk_clipboard_set_text (clipboard, "", -1);

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self),
                                        GDK_SELECTION_CLIPBOARD);

  gtk_clipboard_clear (clipboard);
  gtk_clipboard_set_text (clipboard, "", -1);
}

static gboolean
prompt_entry_button_press_event_cb (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    return TRUE;

  return FALSE;
}

static GtkWidget *
create_page_one_content (GfUnlockDialog *self)
{
  GtkWidget *hbox;
  GtkWidget *grid;
  char *markup;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  grid = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (hbox), grid, TRUE, TRUE, 0);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 16);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_widget_show (grid);

  self->face_image = gf_user_image_new ();
  gtk_grid_attach (GTK_GRID (grid), self->face_image, 0, 0, 1, 2);
  gtk_widget_set_valign (self->face_image, GTK_ALIGN_END);

  markup = g_strdup_printf ("%s", _("_Password:"));
  self->prompt_label = gtk_label_new (NULL);
  gtk_label_set_markup_with_mnemonic (GTK_LABEL (self->prompt_label), markup);
  gtk_label_set_xalign (GTK_LABEL (self->prompt_label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (self->prompt_label), 0.0);
  gtk_grid_attach (GTK_GRID (grid), self->prompt_label, 1, 0, 1, 1);
  gtk_widget_set_valign (self->prompt_label, GTK_ALIGN_END);
  gtk_widget_set_vexpand (self->prompt_label, TRUE);
  gtk_widget_show (self->prompt_label);
  g_free (markup);

  self->prompt_entry = gtk_entry_new ();
  gtk_entry_set_visibility (GTK_ENTRY (self->prompt_entry), FALSE);
  gtk_entry_set_activates_default (GTK_ENTRY (self->prompt_entry), TRUE);
  gtk_grid_attach (GTK_GRID (grid), self->prompt_entry, 1, 1, 1, 1);
  gtk_widget_set_hexpand (self->prompt_entry, TRUE);
  gtk_widget_set_valign (self->prompt_entry, GTK_ALIGN_END);
  gtk_widget_show (self->prompt_entry);

  self->indicator_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_grid_attach (GTK_GRID (grid), self->indicator_box, 2, 1, 1, 1);
  gtk_widget_set_valign (self->indicator_box, GTK_ALIGN_END);

  self->capslock_label = gtk_label_new (NULL);
  gtk_grid_attach (GTK_GRID (grid), self->capslock_label, 1, 2, 1, 1);
  gtk_widget_show (self->capslock_label);

  gtk_label_set_mnemonic_widget (GTK_LABEL (self->prompt_label),
                                 self->prompt_entry);

  /* button press handler used to inhibit popup menu */
  g_signal_connect (self->prompt_entry, "button-press-event",
                    G_CALLBACK (prompt_entry_button_press_event_cb),
                    NULL);

  return hbox;
}

static void
update_user_switch_button (GfUnlockDialog *self)
{
  gboolean enabled;

  enabled = self->user_switch_enabled;

  if (enabled && self->dm_seat != NULL)
    enabled = gf_dm_seat_gen_get_can_switch (self->dm_seat);

  gtk_widget_set_visible (self->switch_button, enabled);
}

static void
dm_seat_ready_cb (GObject      *object,
                  GAsyncResult *res,
                  gpointer      user_data)

{
  GError *error;
  GfDmSeatGen *seat;
  GfUnlockDialog *self;

  error = NULL;
  seat = gf_dm_seat_gen_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_UNLOCK_DIALOG (user_data);
  self->dm_seat = seat;

  update_user_switch_button (self);
}

static void
switch_button_clicked_cb (GtkButton      *button,
                          GfUnlockDialog *self)
{
  set_sensitive (self, FALSE);
  set_busy (self);

  cancel_timeout_remove (self);
  emit_cancel_remove (self);

  switch_user (self);
}

static void
unlock_button_clicked_cb (GtkButton      *button,
                          GfUnlockDialog *self)
{
  if (gf_auth_awaits_response (self->auth))
    {
      const char *text;

      set_sensitive (self, FALSE);
      set_message (self, _("Checking…"));

      text = gtk_entry_get_text (GTK_ENTRY (self->prompt_entry));

      gf_auth_set_response (self->auth, text);
    }

  cancel_timeout_remove (self);
  emit_cancel_remove (self);

  gtk_entry_set_text (GTK_ENTRY (self->prompt_entry), "");

  disable_prompt (self);
  set_busy (self);
}

static GtkWidget *
create_page_one_buttons (GfUnlockDialog *self)
{
  GtkWidget *hbox;

  hbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
  gtk_box_set_spacing (GTK_BOX (hbox), 5);

  self->switch_button = gtk_button_new_with_mnemonic (_("S_witch User…"));
  gtk_widget_set_focus_on_click (self->switch_button, FALSE);
  gtk_widget_set_can_default (self->switch_button, TRUE);
  gtk_box_pack_end (GTK_BOX (hbox), self->switch_button, FALSE, TRUE, 0);
  gtk_widget_show (self->switch_button);

  g_signal_connect (self->switch_button, "clicked",
                    G_CALLBACK (switch_button_clicked_cb), self);

  self->unlock_button = gtk_button_new_with_mnemonic (_("_Unlock"));
  gtk_widget_set_focus_on_click (self->unlock_button, FALSE);
  gtk_widget_set_can_default (self->unlock_button, TRUE);
  gtk_box_pack_end (GTK_BOX (hbox), self->unlock_button, FALSE, TRUE, 0);
  gtk_widget_show (self->unlock_button);

  g_signal_connect (self->unlock_button, "clicked",
                    G_CALLBACK (unlock_button_clicked_cb), self);

  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (hbox),
                                      self->switch_button,
                                      TRUE);

  update_user_switch_button (self);

  return hbox;
}

static GtkWidget *
create_page_one (GfUnlockDialog *self)
{
  GtkWidget *vbox;
  GtkWidget *content;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);

  content = create_page_one_content (self);
  gtk_box_pack_start (GTK_BOX (vbox), content, FALSE, FALSE, 0);
  gtk_widget_show (content);

  self->message_label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbox), self->message_label, FALSE, FALSE, 0);
  gtk_widget_show (self->message_label);

  self->buttons_box = create_page_one_buttons (self);
  gtk_box_pack_end (GTK_BOX (vbox), self->buttons_box, FALSE, FALSE, 0);
  gtk_widget_show (self->buttons_box);

  return vbox;
}

static void
auth_message_cb (GfAuth            *auth,
                 GfAuthMessageType  type,
                 const char        *message,
                 GfUnlockDialog    *self)
{
  g_debug ("Got message: type - %d, message - %s", type, message);

  gtk_widget_show (GTK_WIDGET (self));
  set_ready (self);

  switch (type)
    {
      case GF_AUTH_MESSAGE_PROMPT_ECHO_ON:
      case GF_AUTH_MESSAGE_PROMPT_ECHO_OFF:
        set_sensitive (self, TRUE);
        enable_prompt (self, message, type == GF_AUTH_MESSAGE_PROMPT_ECHO_ON);
        break;

      case GF_AUTH_MESSAGE_ERROR_MSG:
      case GF_AUTH_MESSAGE_TEXT_INFO:
        set_message (self, message);
        break;

      default:
        g_assert_not_reached ();
        break;
    }
}

typedef void (* ShakeDoneFunc) (GfUnlockDialog *self);

typedef struct
{
  GfUnlockDialog *self;
  ShakeDoneFunc   done_func;
  guint           count;
} ShakeData;

static gboolean
reset_idle_cb (gpointer user_data)
{
  GfUnlockDialog *self;

  self = GF_UNLOCK_DIALOG (user_data);
  self->reset_timeout_id = 0;

  set_sensitive (self, TRUE);
  set_message (self, NULL);

  return G_SOURCE_REMOVE;
}

static void
try_again (GfUnlockDialog *self)
{
  g_debug ("Authentication failed, retrying (%u)", self->failure_count);

  if (self->reset_timeout_id != 0)
    g_source_remove (self->reset_timeout_id);

  self->reset_timeout_id = g_timeout_add_seconds (3, reset_idle_cb, self);

  g_source_set_name_by_id (self->reset_timeout_id,
                           "[gnome-flashback] reset_idle_cb");

  disable_prompt (self);
  set_busy (self);

  gf_auth_verify (self->auth);
}

static void
max_failures (GfUnlockDialog *self)
{
  g_debug ("Authentication failed, quitting (max failures)");
  g_signal_emit (self, unlock_dialog_signals[CLOSE], 0);
}

static gboolean
shake_cb (gpointer user_data)
{
  ShakeData *data;
  GfUnlockDialog *self;

  data = user_data;
  self = data->self;

  if (data->count < 9)
    {
      if (data->count % 2 == 0)
        {
          gtk_widget_set_margin_start (GTK_WIDGET (self), 30);
          gtk_widget_set_margin_end (GTK_WIDGET (self), 0);
        }
      else
        {
          gtk_widget_set_margin_start (GTK_WIDGET (self), 0);
          gtk_widget_set_margin_end (GTK_WIDGET (self), 30);
        }

      data->count++;
      return G_SOURCE_CONTINUE;
    }

  data->done_func (self);

  self->shake_timeout_id = 0;
  return G_SOURCE_REMOVE;
}

static void
shake_dialog (GfUnlockDialog *self,
              ShakeDoneFunc   done_func)
{
  ShakeData *data;

  data = g_new0 (ShakeData, 1);
  data->self = self;
  data->done_func = done_func;

  g_assert (self->shake_timeout_id == 0);
  self->shake_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                               10,
                                               shake_cb,
                                               data,
                                               g_free);

  g_source_set_name_by_id (self->shake_timeout_id,
                           "[gnome-flashback] shake_cb");
}

static void
auth_complete_cb (GfAuth         *auth,
                  gboolean        verified,
                  const char     *message,
                  GfUnlockDialog *self)
{
  if (verified)
    {
      g_signal_emit (self,
                     unlock_dialog_signals[RESPONSE],
                     0,
                     GF_UNLOCK_DIALOG_RESPONSE_OK);
    }
  else
    {
      set_message (self, message);

      self->failure_count++;

      if (self->failure_count < MAX_FAILURES)
        shake_dialog (self, try_again);
      else
        shake_dialog (self, max_failures);
    }
}

static void
update_capslock_label (GfUnlockDialog *self)
{
  GdkDisplay *display;
  GdkKeymap *keymap;

  display = gtk_widget_get_display (GTK_WIDGET (self));
  keymap = gdk_keymap_get_for_display (display);

  if (gdk_keymap_get_caps_lock_state (keymap))
    {
      gtk_label_set_text (GTK_LABEL (self->capslock_label),
                          _("You have the Caps Lock key on."));
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (self->capslock_label), "");
    }
}

static void
keymap_state_changed_cb (GdkKeymap      *keymap,
                         GfUnlockDialog *self)
{
  update_capslock_label (self);
}

static void
gf_unlock_dialog_constructed (GObject *object)
{
  GfUnlockDialog *self;
  GdkDisplay *display;
  GdkKeymap *keymap;
  const char *username;
  const char *env_display;

  self = GF_UNLOCK_DIALOG (object);

  G_OBJECT_CLASS (gf_unlock_dialog_parent_class)->constructed (object);

  display = gtk_widget_get_display (GTK_WIDGET (self));
  keymap = gdk_keymap_get_for_display (display);

  self->keymap_state_changed_id = g_signal_connect (keymap,
                                                    "state-changed",
                                                    G_CALLBACK (keymap_state_changed_cb),
                                                    self);

  username = g_get_user_name ();
  env_display = g_getenv ("DISPLAY");

  if (env_display == NULL)
    env_display = ":0.0";

  self->auth = gf_auth_new (username, env_display);

  self->auth_message_id = g_signal_connect (self->auth,
                                            "message",
                                            G_CALLBACK (auth_message_cb),
                                            self);

  self->auth_complete_id = g_signal_connect (self->auth,
                                             "complete",
                                             G_CALLBACK (auth_complete_cb),
                                             self);

  gf_auth_verify (self->auth);
}

static void
gf_unlock_dialog_dispose (GObject *object)
{
  GfUnlockDialog *self;

  self = GF_UNLOCK_DIALOG (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->dm_seat);

  if (self->auth_complete_id != 0)
    {
      g_signal_handler_disconnect (self->auth, self->auth_complete_id);
      self->auth_complete_id = 0;
    }

  if (self->auth_message_id != 0)
    {
      g_signal_handler_disconnect (self->auth, self->auth_message_id);
      self->auth_message_id = 0;
    }

  if (self->auth != NULL)
    {
      gf_auth_cancel (self->auth);

      g_object_unref (self->auth);
      self->auth = NULL;
    }

  G_OBJECT_CLASS (gf_unlock_dialog_parent_class)->dispose (object);
}

static void
gf_unlock_dialog_finalize (GObject *object)
{
  GfUnlockDialog *self;

  self = GF_UNLOCK_DIALOG (object);

  if (self->keymap_state_changed_id != 0)
    {
      GdkDisplay *display;
      GdkKeymap *keymap;

      display = gtk_widget_get_display (GTK_WIDGET (self));
      keymap = gdk_keymap_get_for_display (display);

      g_signal_handler_disconnect (keymap, self->keymap_state_changed_id);
      self->keymap_state_changed_id = 0;
    }

  cancel_timeout_remove (self);
  emit_cancel_remove (self);

  if (self->shake_timeout_id != 0)
    {
      g_source_remove (self->shake_timeout_id);
      self->shake_timeout_id = 0;
    }

  if (self->reset_timeout_id != 0)
    {
      g_source_remove (self->reset_timeout_id);
      self->reset_timeout_id = 0;
    }

  G_OBJECT_CLASS (gf_unlock_dialog_parent_class)->finalize (object);
}

static void
gf_unlock_dialog_show (GtkWidget *widget)
{
  GfUnlockDialog *self;

  self = GF_UNLOCK_DIALOG (widget);

  clear_clipboards (self);

  GTK_WIDGET_CLASS (gf_unlock_dialog_parent_class)->show (widget);

  update_capslock_label (self);

  gtk_widget_grab_default (self->unlock_button);
  gtk_widget_grab_focus (self->prompt_entry);
  cancel_timeout_add (self);
}

static void
install_signals (void)
{
  unlock_dialog_signals[RESPONSE] =
    g_signal_new ("response", GF_TYPE_UNLOCK_DIALOG,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GF_TYPE_UNLOCK_DIALOG_RESPONSE);

  unlock_dialog_signals[CLOSE] =
    g_signal_new ("close", GF_TYPE_UNLOCK_DIALOG,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
gf_unlock_dialog_class_init (GfUnlockDialogClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);

  object_class->constructed = gf_unlock_dialog_constructed;
  object_class->dispose = gf_unlock_dialog_dispose;
  object_class->finalize = gf_unlock_dialog_finalize;

  widget_class->show = gf_unlock_dialog_show;

  install_signals ();

  gtk_widget_class_set_css_name (widget_class, "gf-unlock-dialog");

  binding_set = gtk_binding_set_by_class (widget_class);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);
}

static void
gf_unlock_dialog_init (GfUnlockDialog *self)
{
  const gchar *xdg_seat_path;
  GtkStyleContext *style;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *notebook;
  GtkWidget *page;

  self->cancellable = g_cancellable_new ();

  xdg_seat_path = g_getenv ("XDG_SEAT_PATH");
  if (xdg_seat_path != NULL && *xdg_seat_path != '\0')
    {
      gf_dm_seat_gen_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        "org.freedesktop.DisplayManager",
                                        xdg_seat_path,
                                        self->cancellable,
                                        dm_seat_ready_cb,
                                        self);
    }

  gtk_widget_set_size_request (GTK_WIDGET (self), 450, -1);

  style = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (style, GTK_STYLE_CLASS_BACKGROUND);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (self), frame);
  gtk_widget_show (frame);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 24);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  notebook = gtk_notebook_new ();
  gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
  gtk_widget_show (notebook);

  page = create_page_one (self);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, NULL);
  gtk_widget_show (page);
}

GtkWidget *
gf_unlock_dialog_new (void)
{
  return g_object_new (GF_TYPE_UNLOCK_DIALOG,
                       "halign", GTK_ALIGN_CENTER,
                       "orientation", GTK_ORIENTATION_VERTICAL,
                       "valign", GTK_ALIGN_CENTER,
                       NULL);
}

void
gf_unlock_dialog_set_input_sources (GfUnlockDialog *self,
                                    GfInputSources *input_sources)
{
  if (self->input_source_button != NULL)
    {
      gtk_widget_hide (self->indicator_box);
      gtk_widget_destroy (self->input_source_button);
      self->input_source_button = NULL;
      return;
    }

  if (input_sources == NULL)
    return;

  self->input_source_button = gf_input_sources_create_button (input_sources);
  gtk_container_add (GTK_CONTAINER (self->indicator_box),
                     self->input_source_button);

  g_object_bind_property (self->input_source_button,
                          "visible",
                          self->indicator_box,
                          "visible",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
}

void
gf_unlock_dialog_set_user_switch_enabled (GfUnlockDialog *self,
                                          gboolean        user_switch_enabled)
{
  if (self->user_switch_enabled == user_switch_enabled)
    return;

  self->user_switch_enabled = user_switch_enabled;

  update_user_switch_button (self);
}

void
gf_unlock_dialog_forward_key_event (GfUnlockDialog *self,
                                    GdkEvent       *event)
{
  gtk_widget_event (self->prompt_entry, event);
}
