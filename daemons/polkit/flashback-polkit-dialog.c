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

#include "config.h"

#include <errno.h>
#include <gdk/gdkx.h>
#include <glib/gi18n-lib.h>
#include <pwd.h>

#include "flashback-polkit-dialog.h"

#define RESPONSE_USER_SELECTED 1001

struct _FlashbackPolkitDialog
{
  GtkWindow       parent;

  GtkWidget      *image;
  GtkWidget      *main_message_label;
  GtkWidget      *secondary_message_label;
  GtkWidget      *users_combobox;
  GtkWidget      *entry_box;
  GtkWidget      *prompt_label;
  GtkWidget      *password_entry;
  GtkWidget      *info_label;
  GtkWidget      *details_grid;
  GtkWidget      *auth_button;

  GtkListStore   *users_store;

  gchar          *action_id;
  gchar          *vendor;
  gchar          *vendor_url;
  gchar          *icon_name;
  gchar          *message;
  PolkitDetails  *details;
  gchar         **users;

  gchar          *selected_user;

  gint            response;

  gboolean        is_running;
};

enum
{
  PROP_0,

  PROP_ACTION_ID,
  PROP_VENDOR,
  PROP_VENDOR_URL,
  PROP_ICON_NAME,
  PROP_MESSAGE,
  PROP_DETAILS,
  PROP_USERS,

  PROP_SELECTED_USER,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

enum
{
  SIGNAL_CLOSE,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum
{
  COLUMN_PIXBUF,
  COLUMN_TEXT,
  COLUMN_USERNAME,

  N_COLUMNS
};

G_DEFINE_TYPE (FlashbackPolkitDialog, flashback_polkit_dialog, GTK_TYPE_WINDOW)

static void
cancel_button_clicked_cb (GtkButton             *button,
                          FlashbackPolkitDialog *dialog)
{
  dialog->response = GTK_RESPONSE_CANCEL;

  if (dialog->is_running == TRUE)
    gtk_main_quit ();

  gtk_window_close (GTK_WINDOW (dialog));
}

static void
auth_button_clicked_cb (GtkButton             *button,
                        FlashbackPolkitDialog *dialog)
{
  dialog->response = GTK_RESPONSE_OK;

  if (dialog->is_running == TRUE)
    gtk_main_quit ();
}

static void
setup_image (FlashbackPolkitDialog *dialog)
{
  GtkIconTheme *icon_theme;
  GdkPixbuf *vendor;
  GdkPixbuf *password;
  GdkPixbuf *copy;

  if (dialog->icon_name == NULL || strlen (dialog->icon_name) == 0)
    return;

  icon_theme = gtk_icon_theme_get_default ();
  vendor = gtk_icon_theme_load_icon (icon_theme, dialog->icon_name, 48, 0, NULL);

  if (vendor == NULL)
    {
      g_warning ("No icon for themed icon with name '%s'", dialog->icon_name);

      return;
    }

  password = gtk_icon_theme_load_icon (icon_theme, "dialog-password", 48, 0, NULL);

  if (password == NULL)
    {
      g_warning ("Unable to get a pixbuf for icon with name 'dialog-password' at size 48");

      gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->image), vendor);
      g_object_unref (vendor);

      return;
    }

  copy = gdk_pixbuf_copy (password);

  if (copy == NULL)
    {
      g_object_unref (vendor);
      g_object_unref (password);

      return;
    }

  gdk_pixbuf_composite (vendor, copy, 24, 24, 24, 24, 24, 24, 0.5, 0.5,
                        GDK_INTERP_BILINEAR, 255);

  gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->image), copy);

  g_object_unref (vendor);
  g_object_unref (password);
  g_object_unref (copy);
}

static void
setup_main_message_label (FlashbackPolkitDialog *dialog)
{
  gchar *message;

  message = g_strdup_printf ("<big><b>%s</b></big>", dialog->message);

  gtk_label_set_markup (GTK_LABEL (dialog->main_message_label), message);
  gtk_label_set_xalign (GTK_LABEL (dialog->main_message_label), 0);

  g_free (message);
}

static void
setup_secondary_message_label (FlashbackPolkitDialog *dialog)
{
  const gchar *message;
  const gchar *user_name1;
  const gchar *user_name2;

  if (dialog->users != NULL && g_strv_length (dialog->users) > 1)
    {
      message = _("An application is attempting to perform an action that requires privileges. "
                  "Authentication as one of the users below is required to perform this action.");
    }
  else
    {
      user_name1 = g_get_user_name ();
      user_name2 = dialog->users != NULL ? dialog->users[0] : NULL;

      if (g_strcmp0 (user_name1, user_name2) == 0)
        {
          message = _("An application is attempting to perform an action that requires privileges. "
                      "Authentication is required to perform this action.");
        }
      else
        {
          message = _("An application is attempting to perform an action that requires privileges. "
                      "Authentication as the super user is required to perform this action.");
        }
    }

  gtk_label_set_markup (GTK_LABEL (dialog->secondary_message_label), message);
  gtk_label_set_xalign (GTK_LABEL (dialog->secondary_message_label), 0);
}

static void
combobox_set_sensitive (GtkCellLayout   *cell_layout,
                        GtkCellRenderer *cell,
                        GtkTreeModel    *tree_model,
                        GtkTreeIter     *iter,
                        gpointer         user_data)
{
  GtkTreePath *path;
  gint *indices;

  path = gtk_tree_model_get_path (tree_model, iter);
  indices = gtk_tree_path_get_indices (path);

  g_object_set (cell, "sensitive", indices[0] == 0 ? FALSE : TRUE, NULL);
  gtk_tree_path_free (path);
}

static void
users_combobox_changed_cb (GtkComboBox *combobox,
                           gpointer     user_data)
{
  FlashbackPolkitDialog *dialog;
  GtkTreeIter iter;
  gchar *user_name;

  dialog = FLASHBACK_POLKIT_DIALOG (user_data);

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combobox), &iter))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (dialog->users_store), &iter,
                      COLUMN_USERNAME, &user_name,
                      -1);

  g_free (dialog->selected_user);
  dialog->selected_user = user_name;

  g_object_notify (G_OBJECT (dialog), "selected-user");

  dialog->response = RESPONSE_USER_SELECTED;

  gtk_widget_set_sensitive (dialog->prompt_label, TRUE);
  gtk_widget_set_sensitive (dialog->password_entry, TRUE);
  gtk_widget_set_sensitive (dialog->auth_button, TRUE);
}

static GdkPixbuf *
get_user_icon (char *username)
{
  GError *error;
  GDBusConnection *connection;
  GVariant *find_user_result;
  GVariant *get_icon_result;
  GVariant *icon_result_variant;
  const gchar *user_path;
  const gchar *icon_filename;
  GdkPixbuf *pixbuf;

  error = NULL;
  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);

  if (connection == NULL)
    {
      g_warning ("Unable to connect to system bus: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  find_user_result = g_dbus_connection_call_sync (connection,
                                          "org.freedesktop.Accounts",
                                          "/org/freedesktop/Accounts",
                                          "org.freedesktop.Accounts",
                                          "FindUserByName",
                                          g_variant_new ("(s)",
                                          username),
                                          G_VARIANT_TYPE ("(o)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);

  if (find_user_result == NULL)
    {
      g_warning ("Accounts couldn't find user: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  user_path = g_variant_get_string (g_variant_get_child_value (find_user_result, 0),
                                    NULL);

  get_icon_result = g_dbus_connection_call_sync (connection,
                                                 "org.freedesktop.Accounts",
                                                 user_path,
                                                 "org.freedesktop.DBus.Properties",
                                                 "Get",
                                                 g_variant_new ("(ss)",
                                                                "org.freedesktop.Accounts.User",
                                                                "IconFile"),
                                                 G_VARIANT_TYPE ("(v)"),
                                                 G_DBUS_CALL_FLAGS_NONE,
                                                 -1,
                                                 NULL,
                                                 &error);

  g_variant_unref (find_user_result);

  if (get_icon_result == NULL)
    {
      g_warning ("Accounts couldn't find user icon: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  g_variant_get_child (get_icon_result, 0, "v", &icon_result_variant);
  icon_filename = g_variant_get_string (icon_result_variant, NULL);

  if (icon_filename == NULL)
    {
      g_warning ("Accounts didn't return a valid filename for user icon");
      pixbuf = NULL;
    }
  else
    {
      pixbuf = gdk_pixbuf_new_from_file_at_size (icon_filename, 16, 16, &error);
      if (pixbuf == NULL)
        {
          if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_warning ("Couldn't open user icon: %s", error->message);
          else
            g_debug ("Couldn't open user icon: %s", error->message);
          g_error_free (error);
        }
    }

  g_variant_unref (icon_result_variant);
  g_variant_unref (get_icon_result);

  return pixbuf;
}

static void
setup_users_store (FlashbackPolkitDialog *dialog)
{
  GtkTreeIter iter;
  gint i;
  gint index;
  gint selected_index;
  GtkComboBox *combobox;
  GtkCellRenderer *renderer;

  if (dialog->users_store != NULL)
    return;

  dialog->users_store = gtk_list_store_new (N_COLUMNS, GDK_TYPE_PIXBUF,
                                            G_TYPE_STRING, G_TYPE_STRING);

  gtk_list_store_append (dialog->users_store, &iter);
  gtk_list_store_set (dialog->users_store, &iter,
                      COLUMN_PIXBUF, NULL,
                      COLUMN_TEXT, _("Select user..."),
                      COLUMN_USERNAME, NULL,
                      -1);

  index = 0;
  selected_index = 0;

  for (i = 0; dialog->users[i] != NULL; i++)
    {
      gchar *gecos;
      gchar *real_name;
      GdkPixbuf *pixbuf;
      struct passwd *passwd;

      errno = 0;
      passwd = getpwnam (dialog->users[i]);

      if (passwd == NULL)
        {
          g_warning ("Error doing getpwnam(\"%s\"): %s", dialog->users[i], strerror (errno));
          continue;
        }

      if (passwd->pw_gecos != NULL)
        gecos = g_locale_to_utf8 (passwd->pw_gecos, -1, NULL, NULL, NULL);
      else
        gecos = NULL;

      if (gecos != NULL && strlen (gecos) > 0)
        {
          gchar *first_comma;

          first_comma = strchr (gecos, ',');

          if (first_comma != NULL)
            *first_comma = '\0';
        }

      if (g_strcmp0 (gecos, dialog->users[i]) != 0)
        real_name = g_strdup_printf (_("%s (%s)"), gecos, dialog->users[i]);
      else
        real_name = g_strdup (dialog->users[i]);

      g_free (gecos);

      pixbuf = get_user_icon (dialog->users[i]);

      if (pixbuf == NULL)
        {
          GtkIconTheme *icon_theme;

          icon_theme = gtk_icon_theme_get_default ();
          pixbuf = gtk_icon_theme_load_icon (icon_theme, "avatar-default",
                                             GTK_ICON_SIZE_MENU, 0, NULL);
        }

      gtk_list_store_append (dialog->users_store, &iter);
      gtk_list_store_set (dialog->users_store, &iter,
                          COLUMN_PIXBUF, pixbuf,
                          COLUMN_TEXT, real_name,
                          COLUMN_USERNAME, dialog->users[i],
                          -1);

      index++;
      if (passwd->pw_uid == getuid ())
        {
          selected_index = index;

          g_free (dialog->selected_user);
          dialog->selected_user = g_strdup (dialog->users[i]);
        }

      g_free (real_name);
      g_object_unref (pixbuf);
    }

  combobox = GTK_COMBO_BOX (dialog->users_combobox);
  gtk_combo_box_set_model (combobox, GTK_TREE_MODEL (dialog->users_store));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                  "pixbuf", COLUMN_PIXBUF, NULL);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combobox), renderer,
                                      (GtkCellLayoutDataFunc) combobox_set_sensitive,
                                      NULL, NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                  "text", COLUMN_TEXT, NULL);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combobox), renderer,
                                      (GtkCellLayoutDataFunc) combobox_set_sensitive,
                                      NULL, NULL);

  gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), selected_index);

  g_signal_connect (dialog->users_combobox, "changed",
                    G_CALLBACK (users_combobox_changed_cb), dialog);

  gtk_widget_show (dialog->users_combobox);
}

static void
setup_users_combobox (FlashbackPolkitDialog *dialog)
{
  gtk_widget_hide (dialog->users_combobox);
  gtk_widget_set_no_show_all (dialog->users_combobox, TRUE);

  if (dialog->users == NULL)
    return;

  if (g_strv_length (dialog->users) > 1)
    {
      gboolean sensitive;

      setup_users_store (dialog);

      sensitive = dialog->selected_user != NULL ? TRUE : FALSE;

      gtk_widget_set_sensitive (dialog->prompt_label, sensitive);
      gtk_widget_set_sensitive (dialog->password_entry, sensitive);
      gtk_widget_set_sensitive (dialog->auth_button, sensitive);
    }
  else
    {
      dialog->selected_user = g_strdup (dialog->users[0]);
    }
}

static void
setup_entry_box (FlashbackPolkitDialog *dialog)
{
  gtk_entry_set_visibility (GTK_ENTRY (dialog->password_entry), FALSE);

  gtk_widget_hide (dialog->entry_box);
  gtk_widget_set_no_show_all (dialog->entry_box, TRUE);

  gtk_widget_hide (dialog->info_label);
}

static void
add_row (FlashbackPolkitDialog *dialog,
         gint                   row,
         const gchar           *text1,
         const gchar           *text2,
         const gchar           *tooltip)
{
  GtkGrid *grid;
  GtkWidget *label1;
  GtkWidget *label2;

  grid = GTK_GRID (dialog->details_grid);

  label1 = gtk_label_new_with_mnemonic (text1);
  gtk_label_set_use_markup (GTK_LABEL (label1), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label1), 1.0);

  label2 = gtk_label_new_with_mnemonic (text2);
  gtk_label_set_use_markup (GTK_LABEL (label2), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label2), 0.0);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label1), label2);

  gtk_grid_attach (grid, label1, 0, row, 1, 1);
  gtk_grid_attach (grid, label2, 1, row, 1, 1);

  if (tooltip != NULL)
    gtk_widget_set_tooltip_markup (label2,tooltip);
  gtk_widget_set_hexpand (label2, TRUE);
}

static void
setup_details (FlashbackPolkitDialog *dialog)
{
  gint row;
  gchar *s1;
  gchar *s2;
  gchar *tooltip;

  row = 0;

  if (dialog->details != NULL)
    {
      gchar **keys;
      gint i;

      keys = polkit_details_get_keys (dialog->details);

      for (i = 0; keys[i] != NULL; i++)
        {
          const gchar *key;
          const gchar *value;

          key = keys[i];
          if (g_str_has_prefix (key, "polkit."))
            continue;

          value = polkit_details_lookup (dialog->details, key);

          s1 = g_strdup_printf ("<small><b>%s:</b></small>", key);
          s2 = g_strdup_printf ("<small>%s</small>", value);

          add_row (dialog, row, s1, s2, NULL);

          g_free (s1);
          g_free (s2);

          row++;
        }

      g_strfreev (keys);
    }

  s1 = g_strdup_printf ("<small><b>%s</b></small>", _("Action:"));
  s2 = g_strdup_printf ("<small><a href=\"%s\">%s</a></small>",
                        dialog->action_id, dialog->action_id);
  tooltip = g_strdup_printf (_("Click to edit %s"), dialog->action_id);

  add_row (dialog, row++, s1, s2, tooltip);

  g_free (s1);
  g_free (s2);
  g_free (tooltip);

  s1 = g_strdup_printf ("<small><b>%s</b></small>", _("Vendor:"));
  s2 = g_strdup_printf ("<small><a href=\"%s\">%s</a></small>",
                        dialog->vendor_url, dialog->vendor);
  tooltip = g_strdup_printf (_("Click to open %s"), dialog->vendor_url);

  add_row (dialog, row++, s1, s2, tooltip);

  g_free (s1);
  g_free (s2);
  g_free (tooltip);
}

static void
flashback_polkit_dialog_constructed (GObject *object)
{
  FlashbackPolkitDialog *dialog;

  dialog = FLASHBACK_POLKIT_DIALOG (object);

  G_OBJECT_CLASS (flashback_polkit_dialog_parent_class)->constructed (object);

  dialog->response = GTK_RESPONSE_OK;

  setup_image (dialog);
  setup_main_message_label (dialog);
  setup_secondary_message_label (dialog);
  setup_users_combobox (dialog);
  setup_entry_box (dialog);
  setup_details (dialog);
}

static void
flashback_polkit_dialog_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  FlashbackPolkitDialog *dialog;

  dialog = FLASHBACK_POLKIT_DIALOG (object);

  switch (property_id)
    {
      case PROP_ACTION_ID:
        g_value_set_string (value, dialog->action_id);
        break;

      case PROP_VENDOR:
        g_value_set_string (value, dialog->vendor);
        break;

      case PROP_VENDOR_URL:
        g_value_set_string (value, dialog->vendor_url);
        break;

      case PROP_ICON_NAME:
        g_value_set_string (value, dialog->icon_name);
        break;

      case PROP_MESSAGE:
        g_value_set_string (value, dialog->message);
        break;

      case PROP_DETAILS:
        g_value_set_object (value, dialog->details);
        break;

      case PROP_USERS:
        g_value_set_boxed (value, dialog->users);
        break;

      case PROP_SELECTED_USER:
        g_value_set_string (value, dialog->selected_user);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
flashback_polkit_dialog_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  FlashbackPolkitDialog *dialog;

  dialog = FLASHBACK_POLKIT_DIALOG (object);

  switch (property_id)
    {
      case PROP_ACTION_ID:
        dialog->action_id = g_value_dup_string (value);
        break;

      case PROP_VENDOR:
        dialog->vendor = g_value_dup_string (value);
        break;

      case PROP_VENDOR_URL:
        dialog->vendor_url = g_value_dup_string (value);
        break;

      case PROP_ICON_NAME:
        dialog->icon_name = g_value_dup_string (value);
        break;

      case PROP_MESSAGE:
        dialog->message = g_value_dup_string (value);
        break;

      case PROP_DETAILS:
        dialog->details = g_value_dup_object (value);
        break;

      case PROP_USERS:
        dialog->users = g_value_dup_boxed (value);
        break;

      case PROP_SELECTED_USER:
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
flashback_polkit_dialog_dispose (GObject *object)
{
  FlashbackPolkitDialog *dialog;

  dialog = FLASHBACK_POLKIT_DIALOG (object);

  g_clear_object (&dialog->details);
  g_clear_object (&dialog->users_store);

  G_OBJECT_CLASS (flashback_polkit_dialog_parent_class)->dispose (object);
}

static void
flashback_polkit_dialog_finalize (GObject *object)
{
  FlashbackPolkitDialog *dialog;

  dialog = FLASHBACK_POLKIT_DIALOG (object);

  g_free (dialog->action_id);
  g_free (dialog->vendor);
  g_free (dialog->vendor_url);
  g_free (dialog->icon_name);
  g_free (dialog->message);

  g_strfreev (dialog->users);

  g_free (dialog->selected_user);

  G_OBJECT_CLASS (flashback_polkit_dialog_parent_class)->finalize (object);
}

static void
flashback_polkit_dialog_class_init (FlashbackPolkitDialogClass *dialog_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;
  const gchar *resource;

  object_class = G_OBJECT_CLASS (dialog_class);
  widget_class = GTK_WIDGET_CLASS (dialog_class);

  object_class->constructed = flashback_polkit_dialog_constructed;
  object_class->get_property = flashback_polkit_dialog_get_property;
  object_class->set_property = flashback_polkit_dialog_set_property;
  object_class->dispose = flashback_polkit_dialog_dispose;
  object_class->finalize = flashback_polkit_dialog_finalize;

  /**
   * FlashbackPolkitDialog:action-id
   */
  properties[PROP_ACTION_ID] =
    g_param_spec_string ("action-id", "action-id", "action-id",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * FlashbackPolkitDialog:vendor
   */
  properties[PROP_VENDOR] =
    g_param_spec_string ("vendor", "vendor", "vendor",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * FlashbackPolkitDialog:vendor-url
   */
  properties[PROP_VENDOR_URL] =
    g_param_spec_string ("vendor-url", "vendor-url", "vendor-url",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * FlashbackPolkitDialog:icon-name
   */
  properties[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", "icon-name", "icon-name",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * FlashbackPolkitDialog:message
   */
  properties[PROP_MESSAGE] =
    g_param_spec_string ("message", "message", "message",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * FlashbackPolkitDialog:details
   */
  properties[PROP_DETAILS] =
    g_param_spec_object ("details", "details", "details",
                         POLKIT_TYPE_DETAILS,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * FlashbackPolkitDialog:users
   */
  properties[PROP_USERS] =
    g_param_spec_boxed ("users", "users", "users",
                        G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);

  /**
   * FlashbackPolkitDialog:selected-user
   */
  properties[PROP_SELECTED_USER] =
    g_param_spec_string ("selected-user", "selected-user", "selected-user",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /**
   * FlashbackPolkitDialog::close
   */
  signals[SIGNAL_CLOSE] =
    g_signal_new ("close",
                  G_OBJECT_CLASS_TYPE (dialog_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  binding_set = gtk_binding_set_by_class (dialog_class);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);

  resource = "/org/gnome/gnome-flashback/flashback-polkit-dialog.ui";
  gtk_widget_class_set_template_from_resource (widget_class, resource);

  gtk_widget_class_bind_template_child (widget_class, FlashbackPolkitDialog, image);
  gtk_widget_class_bind_template_child (widget_class, FlashbackPolkitDialog, main_message_label);
  gtk_widget_class_bind_template_child (widget_class, FlashbackPolkitDialog, secondary_message_label);
  gtk_widget_class_bind_template_child (widget_class, FlashbackPolkitDialog, users_combobox);
  gtk_widget_class_bind_template_child (widget_class, FlashbackPolkitDialog, entry_box);
  gtk_widget_class_bind_template_child (widget_class, FlashbackPolkitDialog, prompt_label);
  gtk_widget_class_bind_template_child (widget_class, FlashbackPolkitDialog, password_entry);
  gtk_widget_class_bind_template_child (widget_class, FlashbackPolkitDialog, info_label);
  gtk_widget_class_bind_template_child (widget_class, FlashbackPolkitDialog, details_grid);
  gtk_widget_class_bind_template_child (widget_class, FlashbackPolkitDialog, auth_button);

  gtk_widget_class_bind_template_callback (widget_class, cancel_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, auth_button_clicked_cb);
}

static void
flashback_polkit_dialog_close (FlashbackPolkitDialog *dialog,
                               gpointer               user_data)
{
  dialog->response = GTK_RESPONSE_CANCEL;

  if (dialog->is_running == TRUE)
    gtk_main_quit ();

  gtk_window_close (GTK_WINDOW (dialog));
}

static gboolean
delete_event_cb (FlashbackPolkitDialog *dialog,
                 GdkEvent              *event,
                 gpointer               user_data)
{
  if (!dialog->is_running)
    return FALSE;

  dialog->response = GTK_RESPONSE_DELETE_EVENT;
  gtk_main_quit ();

  return TRUE;
}

static void
flashback_polkit_dialog_init (FlashbackPolkitDialog *dialog)
{
  GtkWindow *window;
  GtkWidget *widget;

  widget = GTK_WIDGET (dialog);
  window = GTK_WINDOW (dialog);

  gtk_widget_init_template (widget);

  gtk_window_set_icon_name (window, "dialog-password");
  gtk_window_set_keep_above (window, TRUE);

  g_signal_connect (dialog, "close",
                    G_CALLBACK (flashback_polkit_dialog_close),
                    NULL);

  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (delete_event_cb),
                    NULL);
}

/**
 * flashback_polkit_dialog_new:
 * @action_id:
 * @vendor:
 * @vendor_url:
 * @icon_name:
 * @message_markup:
 * @details:
 * @users:
 *
 * Creates a new #FlashbackPolkitDialog.
 *
 * Returns: (transfer full): a newly created #FlashbackPolkitDialog.
 **/
GtkWidget *
flashback_polkit_dialog_new (const gchar    *action_id,
                             const gchar    *vendor,
                             const gchar    *vendor_url,
                             const gchar    *icon_name,
                             const gchar    *message_markup,
                             PolkitDetails  *details,
                             gchar         **users)
{
  return g_object_new (FLASHBACK_TYPE_POLKIT_DIALOG,
                       "action-id", action_id,
                       "vendor", vendor,
                       "vendor-url", vendor_url,
                       "icon-name", icon_name,
                       "message", message_markup,
                       "details", details,
                       "users", users,
                       NULL);
}

/**
 * flashback_polkit_dialog_get_selected_user:
 * @dialog: a #FlashbackPolkitDialog
 *
 * Gets the currently selected user.
 *
 * Returns: The currently selected user (free with g_free()) or %NULL if no
 *     user is currently selected.
 **/
gchar *
flashback_polkit_dialog_get_selected_user (FlashbackPolkitDialog *dialog)
{
  return g_strdup (dialog->selected_user);
}

/**
 * flashback_polkit_dialog_run_until_user_is_selected:
 * @dialog: a #FlashbackPolkitDialog
 *
 * Runs @dialog in a recursive main loop until a user have been selected.
 *
 * If there is only one element in the the users array (which is set upon
 * construction) or an user has already been selected, this function returns
 * immediately with the return value %TRUE.
 *
 * Returns: %TRUE if a user is selected or %FALSE if the dialog was cancelled.
 **/
gboolean
flashback_polkit_dialog_run_until_user_is_selected (FlashbackPolkitDialog *dialog)
{
  if (dialog->selected_user != NULL)
    return TRUE;

  dialog->is_running = TRUE;

  gtk_main ();

  dialog->is_running = FALSE;

  if (dialog->response == RESPONSE_USER_SELECTED)
    return TRUE;

  return FALSE;
}

/**
 * flashback_polkit_dialog_run_until_response_for_prompt:
 * @dialog: a #FlashbackPolkitDialog
 * @prompt: the prompt to present the user with
 * @echo_chars: whether characters should be echoed in the password entry box
 * @was_cancelled: set to %TRUE if the dialog was cancelled
 * @new_user_selected: set to %TRUE if another user was selected
 *
 * Runs @dialog in a recursive main loop until a response to @prompt have been
 * obtained from the user.
 *
 * Returns: The response (free with g_free()) or %NULL if one of
 *     @was_cancelled or @new_user_selected has been set to %TRUE.
 **/
gchar *
flashback_polkit_dialog_run_until_response_for_prompt (FlashbackPolkitDialog *dialog,
                                                       const gchar           *prompt,
                                                       gboolean               echo_chars,
                                                       gboolean              *was_cancelled,
                                                       gboolean              *new_user_selected)
{
  gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->prompt_label), prompt);
  gtk_entry_set_visibility (GTK_ENTRY (dialog->password_entry), echo_chars);
  gtk_entry_set_text (GTK_ENTRY (dialog->password_entry), "");
  gtk_widget_grab_focus (dialog->password_entry);

  if (was_cancelled != NULL)
    *was_cancelled = FALSE;

  if (new_user_selected != NULL)
    *new_user_selected = FALSE;

  dialog->is_running = TRUE;

  gtk_widget_set_no_show_all (dialog->entry_box, FALSE);
  gtk_widget_show_all (dialog->entry_box);

  gtk_main ();

  gtk_widget_hide (dialog->entry_box);
  gtk_widget_set_no_show_all (dialog->entry_box, TRUE);

  dialog->is_running = FALSE;

  if (dialog->response == GTK_RESPONSE_OK)
    {
      return g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->password_entry)));
    }
  else if (dialog->response == RESPONSE_USER_SELECTED)
    {
      if (new_user_selected != NULL)
        *new_user_selected = TRUE;
    }
  else
    {
      if (was_cancelled != NULL)
        *was_cancelled = TRUE;
    }

  return NULL;
}

/**
 * flashback_polkit_dialog_cancel:
 * @dialog: a #FlashbackPolkitDialog
 *
 * Cancels the dialog if it is currenlty running.
 *
 * Returns: %TRUE if the dialog was running.
 **/
gboolean
flashback_polkit_dialog_cancel (FlashbackPolkitDialog *dialog)
{
  if (!dialog->is_running)
    return FALSE;

  dialog->response = GTK_RESPONSE_CANCEL;
  gtk_main_quit ();

  return TRUE;
}

/**
 * flashback_polkit_dialog_indicate_error:
 * @dialog: a #FlashbackPolkitDialog
 *
 * Call this function to indicate an authentication error; typically shakes
 * the window.
 **/
void
flashback_polkit_dialog_indicate_error (FlashbackPolkitDialog *dialog)
{
  GtkWindow *window;
  gint x;
  gint y;
  gint n;

  window = GTK_WINDOW (dialog);

  gtk_window_get_position (window, &x, &y);

  for (n = 0; n < 10; n++)
    {
      gtk_window_move (window, x + (n % 2 == 0 ? -15 : 15), y);

      while (gtk_events_pending ())
        gtk_main_iteration ();

      g_usleep (10000);
    }

  gtk_window_move (window, x, y);
}

void
flashback_polkit_dialog_set_info_message (FlashbackPolkitDialog *dialog,
                                          const gchar           *info_markup)
{
  gtk_label_set_markup (GTK_LABEL (dialog->info_label), info_markup);
}

void
flashback_polkit_dialog_present (FlashbackPolkitDialog *dialog)
{
  GtkWidget *widget;
  GdkWindow *window;
  guint32 server_time;

  widget = GTK_WIDGET (dialog);

  gtk_widget_show_all (widget);

  window = gtk_widget_get_window (widget);

  server_time = GDK_CURRENT_TIME;
  if (window != NULL)
    server_time = gdk_x11_get_server_time (window);

  gtk_window_present_with_time (GTK_WINDOW (dialog), server_time);
}
