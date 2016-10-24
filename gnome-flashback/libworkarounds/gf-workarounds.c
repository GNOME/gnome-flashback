/*
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
 * Based on code from Owen Taylor, Copyright(C) 2001, 2007 Red Hat, Inc.
 *
 * https://git.gnome.org/browse/gtk+/tree/gdk/x11/xsettings-client.c
 * https://git.gnome.org/browse/gnome-settings-daemon/tree/plugins/xsettings/xsettings-manager.c
 */

#include "config.h"

#include <string.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/Xmd.h>

#include "gf-workarounds.h"

#define BYTES_LEFT(buffer) ((buffer)->data + (buffer)->len - (buffer)->pos)
#define RETURN_IF_FAIL_BYTES(buffer, n_bytes) if (BYTES_LEFT (buffer) < (n_bytes)) return FALSE;
#define XSETTINGS_PAD(n,m) ((n + m - 1) & (~(m-1)))

struct _GfWorkarounds
{
  GObject      parent;

  GSettings   *g_settings;
  GtkSettings *gtk_settings;

  gboolean     fix_app_menu;
  gchar       *fix_button_layout;

  guint        idle_id;
  guint        timeout_id;

  Display     *xdisplay;

  Atom         selection_atom;
  Atom         xsettings_atom;

  Window       manager_window;

  GHashTable  *xsettings;
  CARD32       serial;
};

G_DEFINE_TYPE (GfWorkarounds, gf_workarounds, G_TYPE_OBJECT)

static void add_workarounds (GfWorkarounds *workarounds);

typedef enum
{
  XSETTINGS_TYPE_INT    = 0,
  XSETTINGS_TYPE_STRING = 1,
  XSETTINGS_TYPE_COLOR  = 2
} XSettingsType;

typedef struct
{
  gchar         *name;
  XSettingsType  type;
  GValue        *value;
  gulong         last_change_serial;
} XSettingsSetting;

typedef struct
{
  gchar   byte_order;
  gulong  len;
  guchar *data;
  guchar *pos;
} XSettingsBuffer;

static gboolean
fetch_card16 (XSettingsBuffer *buffer,
              CARD16          *result)
{
  CARD16 x;

  RETURN_IF_FAIL_BYTES (buffer, 2);

  x = *(CARD16 *)buffer->pos;
  buffer->pos += 2;

  if (buffer->byte_order == MSBFirst)
    *result = GUINT16_FROM_BE (x);
  else
    *result = GUINT16_FROM_LE (x);

  return TRUE;
}

static gboolean
fetch_card32 (XSettingsBuffer *buffer,
              CARD32          *result)
{
  CARD32 x;

  RETURN_IF_FAIL_BYTES (buffer, 4);

  x = *(CARD32 *)buffer->pos;
  buffer->pos += 4;

  if (buffer->byte_order == MSBFirst)
    *result = GUINT32_FROM_BE (x);
  else
    *result = GUINT32_FROM_LE (x);

  return TRUE;
}

static gboolean
fetch_card8 (XSettingsBuffer *buffer,
             CARD8           *result)
{
  RETURN_IF_FAIL_BYTES (buffer, 1);

  *result = *(CARD8 *)buffer->pos;
  buffer->pos += 1;

  return TRUE;
}

static gboolean
fetch_string (XSettingsBuffer  *buffer,
              guint             length,
              gchar           **result)
{
  guint pad_len;

  pad_len = XSETTINGS_PAD (length, 4);
  if (pad_len < length)
    return FALSE;

  RETURN_IF_FAIL_BYTES (buffer, pad_len);

  *result = g_strndup ((gchar *) buffer->pos, length);
  buffer->pos += pad_len;

  return TRUE;
}

static void
free_gvalue (gpointer user_data)
{
  GValue *value;

  value = (GValue *) user_data;

  g_value_unset (value);
  g_free (value);
}

static void
free_xsetting (gpointer data)
{
  XSettingsSetting *setting;

  setting = (XSettingsSetting *) data;

  g_free (setting->name);
  free_gvalue (setting->value);
  g_free (setting);
}

static GHashTable *
parse_settings (GfWorkarounds *workarounds,
                guchar        *data,
                gulong         n_items)
{
  XSettingsBuffer buffer;
  GHashTable *settings;
  CARD32 n_entries;
  CARD32 i;
  GValue *value;
  gchar *x_name;
  gulong last_change_serial;
  XSettingsSetting *setting;

  buffer.pos = buffer.data = data;
  buffer.len = n_items;

  if (!fetch_card8 (&buffer, (guchar *)&buffer.byte_order))
    return NULL;

  if (buffer.byte_order != MSBFirst && buffer.byte_order != LSBFirst)
    return NULL;

  buffer.pos += 3;

  if (!fetch_card32 (&buffer, &workarounds->serial) ||
      !fetch_card32 (&buffer, &n_entries))
    return NULL;

  settings = NULL;
  value = NULL;
  x_name = NULL;

  for (i = 0; i < n_entries; i++)
    {
      CARD8 type;
      CARD16 name_len;
      CARD32 v_int;

      if (!fetch_card8 (&buffer, &type))
        goto out;

      buffer.pos += 1;

      if (!fetch_card16 (&buffer, &name_len))
        goto out;

      if (!fetch_string (&buffer, name_len, &x_name) || !fetch_card32 (&buffer, &v_int))
        goto out;

      last_change_serial = (gulong) v_int;

      switch (type)
        {
          case XSETTINGS_TYPE_INT:
            if (!fetch_card32 (&buffer, &v_int))
              goto out;

            value = g_new0 (GValue, 1);
            g_value_init (value, G_TYPE_INT);
            g_value_set_int (value, (gint32) v_int);
            break;

          case XSETTINGS_TYPE_STRING:
            {
              gchar *s;

              if (!fetch_card32 (&buffer, &v_int) || !fetch_string (&buffer, v_int, &s))
                goto out;

              value = g_new0 (GValue, 1);
              g_value_init (value, G_TYPE_STRING);
              g_value_take_string (value, s);
            }
            break;

          case XSETTINGS_TYPE_COLOR:
            /* GNOME Settings Daemon does not export settings with color type. */
            g_free (x_name);
            x_name = NULL;
            break;

          default:
            /* Unknown type */
            g_free (x_name);
            x_name = NULL;
            break;
        }

      if (settings == NULL)
        settings = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          g_free, free_xsetting);

      if (g_hash_table_lookup (settings, x_name) != NULL)
        goto out;

      if (x_name != NULL)
        {
          setting = g_new0 (XSettingsSetting, 1);

          setting->name = g_strdup (x_name);
          setting->type = type;
          setting->value = value;
          setting->last_change_serial = last_change_serial;

          g_hash_table_insert (settings, g_strdup (x_name), setting);

          g_free (x_name);
          x_name = NULL;
          value = NULL;
        }
    }

  return settings;

out:

  if (value)
    free_gvalue (value);

  if (settings)
    g_hash_table_unref (settings);

  g_free (x_name);

  return NULL;
}

static gboolean
read_settings (GfWorkarounds *workarounds)
{
  GdkDisplay *display;
  gint result;
  Atom type;
  gint format;
  gulong n_items;
  gulong bytes_after;
  guchar *data;

  display = gdk_x11_lookup_xdisplay (workarounds->xdisplay);

  gdk_x11_display_error_trap_push (display);
  result = XGetWindowProperty (workarounds->xdisplay, workarounds->manager_window,
                               workarounds->xsettings_atom, 0, LONG_MAX,
                               False, workarounds->xsettings_atom,
                               &type, &format, &n_items, &bytes_after, &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (result == Success && type != None)
    {
      if (type == workarounds->xsettings_atom && format == 8)
        workarounds->xsettings = parse_settings (workarounds, data, n_items);

      XFree (data);
    }

  if (workarounds->xsettings)
    return TRUE;

  return FALSE;
}

static gchar
get_byte_order (void)
{
  CARD32 myint = 0x01020304;
  return (*(gchar *)&myint == 1) ? MSBFirst : LSBFirst;
}

static void
align_string (GString *string,
              gint     alignment)
{
  while ((string->len % alignment) != 0)
    g_string_append_c (string, '\0');
}

static void
setting_store (XSettingsSetting *setting,
               GString          *buffer)
{
  guint16 len16;

  g_string_append_c (buffer, setting->type);
  g_string_append_c (buffer, 0);

  len16 = strlen (setting->name);
  g_string_append_len (buffer, (gchar *) &len16, 2);
  g_string_append (buffer, setting->name);
  align_string (buffer, 4);

  g_string_append_len (buffer, (gchar *) &setting->last_change_serial, 4);

  if (setting->type == XSETTINGS_TYPE_INT)
    {
      gint value;

      value = g_value_get_int (setting->value);

      g_string_append_len (buffer, (gchar *) &value, 4);
    }
  else if (setting->type == XSETTINGS_TYPE_STRING)
    {
      const gchar *string;
      guint32 len32;

      string = g_value_get_string (setting->value);
      len32 = strlen (string);
      g_string_append_len (buffer, (gchar *) &len32, 4);
      g_string_append (buffer, string);
      align_string (buffer, 4);
    }
  else if (setting->type == XSETTINGS_TYPE_COLOR)
    {
      /* GNOME Settings Daemon does not export settings with color type. */
    }
}

static void
write_settings (GfWorkarounds *workarounds)
{
  GString *buffer;
  GHashTableIter iter;
  int n_settings;
  gpointer value;

  n_settings = g_hash_table_size (workarounds->xsettings);

  buffer = g_string_new (NULL);
  g_string_append_c (buffer, get_byte_order ());
  g_string_append_c (buffer, '\0');
  g_string_append_c (buffer, '\0');
  g_string_append_c (buffer, '\0');

  g_string_append_len (buffer, (gchar *) &workarounds->serial, 4);
  g_string_append_len (buffer, (gchar *) &n_settings, 4);

  g_hash_table_iter_init (&iter, workarounds->xsettings);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    setting_store (value, buffer);

  XChangeProperty (workarounds->xdisplay, workarounds->manager_window,
                   workarounds->xsettings_atom, workarounds->xsettings_atom,
                   8, PropModeReplace, (guchar *) buffer->str, buffer->len);

  g_string_free (buffer, TRUE);
  g_hash_table_unref (workarounds->xsettings);
  workarounds->xsettings = NULL;
}

static void
apply_app_menu_workaround (GfWorkarounds *workarounds)
{
  const gchar *key;
  XSettingsSetting *old;
  XSettingsSetting *setting;

  key = "Gtk/ShellShowsAppMenu";
  old = g_hash_table_lookup (workarounds->xsettings, key);

  setting = g_new0 (XSettingsSetting, 1);
  setting->name = g_strdup (key);
  setting->type = XSETTINGS_TYPE_INT;
  setting->value = g_new0 (GValue, 1);
  setting->last_change_serial = 0;

  g_value_init (setting->value, G_TYPE_INT);
  g_value_set_int (setting->value, 0);

  if (old != NULL)
    setting->last_change_serial = old->last_change_serial;

  g_hash_table_insert (workarounds->xsettings, g_strdup (key), setting);
}

static void
apply_button_layout_workaround (GfWorkarounds *workarounds)
{
  const gchar *key;
  XSettingsSetting *old;
  XSettingsSetting *setting;

  key = "Gtk/DecorationLayout";
  old = g_hash_table_lookup (workarounds->xsettings, key);

  setting = g_new0 (XSettingsSetting, 1);
  setting->name = g_strdup (key);
  setting->type = XSETTINGS_TYPE_STRING;
  setting->value = g_new0 (GValue, 1);
  setting->last_change_serial = 0;

  g_value_init (setting->value, G_TYPE_STRING);
  g_value_set_string (setting->value, workarounds->fix_button_layout);

  if (old != NULL)
    setting->last_change_serial = old->last_change_serial;

  g_hash_table_insert (workarounds->xsettings, g_strdup (key), setting);
}

static gboolean
apply_workarounds (GfWorkarounds *workarounds)
{
  gboolean gtk_shell_shows_app_menu;
  gchar *gtk_decoration_layout;
  gboolean need_workarounds;

  g_object_get (workarounds->gtk_settings,
                "gtk-shell-shows-app-menu", &gtk_shell_shows_app_menu,
                NULL);

  g_object_get (workarounds->gtk_settings,
                "gtk-decoration-layout", &gtk_decoration_layout,
                NULL);

  need_workarounds = gtk_shell_shows_app_menu;
  if (g_strcmp0 (gtk_decoration_layout, workarounds->fix_button_layout) != 0)
    need_workarounds = TRUE;

  g_free (gtk_decoration_layout);

  if (!need_workarounds)
    return TRUE;

  workarounds->manager_window = XGetSelectionOwner (workarounds->xdisplay,
                                                    workarounds->selection_atom);

  if (workarounds->manager_window == None)
    return FALSE;

  if (!read_settings (workarounds))
    return FALSE;

  if (workarounds->fix_app_menu)
    apply_app_menu_workaround (workarounds);

  if (g_strcmp0 (workarounds->fix_button_layout, "") != 0)
    apply_button_layout_workaround (workarounds);

  write_settings (workarounds);

  return TRUE;
}

static gboolean
try_again (gpointer user_data)
{
  GfWorkarounds *workarounds;

  workarounds = GF_WORKAROUNDS (user_data);

  add_workarounds (workarounds);

  workarounds->timeout_id = 0;
  return G_SOURCE_REMOVE;
}

static gboolean
add_workarounds_real (gpointer user_data)
{
  GfWorkarounds *workarounds;
  gboolean fix_app_menu;
  gchar *fix_button_layout;

  workarounds = GF_WORKAROUNDS (user_data);

  fix_app_menu = g_settings_get_boolean (workarounds->g_settings,
                                         "fix-app-menu");
  fix_button_layout = g_settings_get_string (workarounds->g_settings,
                                             "fix-button-layout");

  g_free (workarounds->fix_button_layout);

  workarounds->fix_app_menu = fix_app_menu;
  workarounds->fix_button_layout = fix_button_layout;

  if (!fix_app_menu && g_strcmp0 (fix_button_layout, "") == 0)
    {
      workarounds->idle_id = 0;
      return G_SOURCE_REMOVE;
    }

  if (!apply_workarounds (workarounds))
    {
      if (workarounds->timeout_id > 0)
        g_source_remove (workarounds->timeout_id);

      workarounds->timeout_id = g_timeout_add (100, try_again, workarounds);
      g_source_set_name_by_id (workarounds->timeout_id,
                               "[gnome-flashback] try_again");
    }

  workarounds->idle_id = 0;
  return G_SOURCE_REMOVE;
}

static void
add_workarounds (GfWorkarounds *workarounds)
{
  if (workarounds->idle_id > 0)
    g_source_remove (workarounds->idle_id);

  workarounds->idle_id = g_idle_add (add_workarounds_real, workarounds);
  g_source_set_name_by_id (workarounds->idle_id,
                           "[gnome-flashback] add_workarounds_real");
}

static void
remove_workarounds (void)
{
  GSettings *settings;
  GVariant *overrides;

  settings = g_settings_new ("org.gnome.settings-daemon.plugins.xsettings");

  overrides = g_settings_get_value (settings, "overrides");
  g_settings_set_value (settings, "overrides", overrides);

  g_variant_unref (overrides);
  g_object_unref (settings);
}

static void
g_settings_changed (GSettings   *settings,
                    const gchar *key,
                    gpointer     user_data)
{
  GfWorkarounds *workarounds;
  gboolean fix_app_menu;
  gchar *fix_button_layout;
  gboolean reset;

  workarounds = GF_WORKAROUNDS (user_data);

  fix_app_menu = g_settings_get_boolean (workarounds->g_settings,
                                         "fix-app-menu");
  fix_button_layout = g_settings_get_string (workarounds->g_settings,
                                             "fix-button-layout");

  reset = FALSE;

  if (workarounds->fix_app_menu && !fix_app_menu)
    reset = TRUE;

  if (g_strcmp0 (workarounds->fix_button_layout, "") != 0 &&
      g_strcmp0 (fix_button_layout, "") == 0)
    reset = TRUE;

  g_free (fix_button_layout);

  if (reset)
    {
      remove_workarounds ();
      return;
    }

  add_workarounds (workarounds);
}

static void
gtk_settings_changed (GtkSettings *settings,
                      GParamSpec  *pspec,
                      gpointer     user_data)
{
  GfWorkarounds *workarounds;

  workarounds = GF_WORKAROUNDS (user_data);

  add_workarounds (workarounds);
}

static void
x11_init (GfWorkarounds *workarounds)
{
  GdkDisplay *display;
  Display *xdisplay;
  GdkScreen *screen;
  gint number;
  gchar *atom;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);
  screen = gdk_display_get_default_screen (display);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  number = gdk_screen_get_number (screen);
  G_GNUC_END_IGNORE_DEPRECATIONS

  atom = g_strdup_printf ("_XSETTINGS_S%d", number);

  workarounds->xdisplay = xdisplay;
  workarounds->selection_atom = XInternAtom (xdisplay, atom, False);
  workarounds->xsettings_atom = XInternAtom (xdisplay, "_XSETTINGS_SETTINGS", False);

  g_free (atom);
}

static void
gf_workarounds_dispose (GObject *object)
{
  GfWorkarounds *workarounds;

  workarounds = GF_WORKAROUNDS (object);

  g_clear_object (&workarounds->g_settings);
  g_signal_handlers_disconnect_by_func (workarounds->gtk_settings,
                                        gtk_settings_changed,
                                        workarounds);

  g_free (workarounds->fix_button_layout);

  if (workarounds->idle_id > 0)
    {
      g_source_remove (workarounds->idle_id);
      workarounds->idle_id = 0;
    }

  if (workarounds->timeout_id > 0)
    {
      g_source_remove (workarounds->timeout_id);
      workarounds->timeout_id = 0;
    }

  if (workarounds->xsettings)
    {
      g_hash_table_unref (workarounds->xsettings);
      workarounds->xsettings = NULL;
    }

  remove_workarounds ();

  G_OBJECT_CLASS (gf_workarounds_parent_class)->dispose (object);
}

static void
gf_workarounds_class_init (GfWorkaroundsClass *workarounds_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (workarounds_class);

  object_class->dispose = gf_workarounds_dispose;
}

static void
gf_workarounds_init (GfWorkarounds *workarounds)
{
  x11_init (workarounds);

  workarounds->g_settings = g_settings_new ("org.gnome.gnome-flashback.workarounds");
  workarounds->gtk_settings = gtk_settings_get_default ();

  g_signal_connect (workarounds->g_settings, "changed",
                    G_CALLBACK (g_settings_changed), workarounds);
  g_signal_connect (workarounds->gtk_settings, "notify::gtk-shell-shows-app-menu",
                    G_CALLBACK (gtk_settings_changed), workarounds);
  g_signal_connect (workarounds->gtk_settings, "notify::gtk-decoration-layout",
                    G_CALLBACK (gtk_settings_changed), workarounds);

  add_workarounds (workarounds);
}

GfWorkarounds *
gf_workarounds_new (void)
{
  return g_object_new (GF_TYPE_WORKAROUNDS, NULL);
}
