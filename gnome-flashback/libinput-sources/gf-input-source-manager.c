/*
 * Copyright (C) 2015 Alberts Muktupāvels
 * Copyright (C) 2015 Sebastian Geiger
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
#include "gf-input-source-manager.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <libcommon/gf-keybindings.h>
#include <X11/Xatom.h>

#include "gf-ibus-manager.h"
#include "gf-input-source-ibus.h"
#include "gf-input-source-popup.h"
#include "gf-input-source-settings.h"
#include "gf-input-source-xkb.h"
#include "gf-keyboard-manager.h"

#define DESKTOP_WM_KEYBINDINGS_SCHEMA "org.gnome.desktop.wm.keybindings"

#define KEY_SWITCH_INPUT_SOURCE "switch-input-source"
#define KEY_SWITCH_INPUT_SOURCE_BACKWARD "switch-input-source-backward"

typedef struct _SourceInfo SourceInfo;
struct _SourceInfo
{
  gchar *type;

  gchar *id;
};

typedef struct
{
  GHashTable    *input_sources;
  GfInputSource *current_source;
} SourceWindow;

struct _GfInputSourceManager
{
  GObject                parent;

  GSettings             *wm_keybindings;
  GfKeybindings         *keybindings;
  guint                  switch_source_action;
  guint                  switch_source_backward_action;

  GfInputSourceSettings *settings;

  GfKeyboardManager     *keyboard_manager;

  GfIBusManager         *ibus_manager;
  gboolean               ibus_ready;
  gboolean               disable_ibus;
  gboolean               reloading;

  GHashTable            *input_sources;
  GHashTable            *ibus_sources;

  GList                 *mru_sources;
  GList                 *mru_sources_backup;

  GtkWidget             *popup;

  GfInputSource         *current_source;

  gboolean               sources_per_window;
  gboolean               event_filter_added;

  GHashTable            *source_windows;
};

enum
{
  SIGNAL_SOURCES_CHANGED,
  SIGNAL_CURRENT_SOURCE_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum
{
  PROP_0,

  PROP_IBUS_MANAGER,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfInputSourceManager, gf_input_source_manager, G_TYPE_OBJECT)

static SourceWindow *
source_window_new (void)
{
  SourceWindow *window;

  window = g_new0 (SourceWindow, 1);

  return window;
}

static void
source_window_free (gpointer user_data)
{
  SourceWindow *window;

  window = user_data;

  g_clear_pointer (&window->input_sources, g_hash_table_unref);
  g_clear_object (&window->current_source);
  g_free (window);
}

static Window
get_active_window (void)
{
  GdkDisplay *display;
  Display *xdisplay;
  Atom _net_active_window;
  int status;
  Atom actual_type;
  int actual_format;
  unsigned long n_items;
  unsigned long bytes_after;
  unsigned char *prop;
  Window window;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  _net_active_window = XInternAtom (xdisplay, "_NET_ACTIVE_WINDOW", True);
  if (_net_active_window == None)
    return None;

  gdk_x11_display_error_trap_push (display);

  prop = NULL;
  status = XGetWindowProperty (xdisplay,
                               DefaultRootWindow (xdisplay),
                               _net_active_window,
                               0,
                               G_MAXLONG,
                               False,
                               XA_WINDOW,
                               &actual_type,
                               &actual_format,
                               &n_items,
                               &bytes_after,
                               &prop);

  gdk_x11_display_error_trap_pop_ignored (display);

  if (status != Success ||
      actual_type != XA_WINDOW)
    {
      XFree (prop);

      return None;
    }

  window = *(Window *) prop;
  XFree (prop);

  return window;
}

static SourceWindow *
get_current_window (GfInputSourceManager *manager)
{
  Window active_window;
  SourceWindow *window;

  active_window = get_active_window ();
  if (active_window == None)
    return NULL;

  window = g_hash_table_lookup (manager->source_windows,
                                GUINT_TO_POINTER (active_window));

  if (window == NULL)
    {
      window = source_window_new ();
      g_hash_table_insert (manager->source_windows,
                           GUINT_TO_POINTER (active_window),
                           window);
    }

  return window;
}

static gint
compare_indexes (gconstpointer a,
                 gconstpointer b)
{
  return GPOINTER_TO_UINT (a) - GPOINTER_TO_UINT (b);
}

static gboolean
compare_sources (GfInputSource *source1,
                 GfInputSource *source2)
{
  const gchar *type1;
  const gchar *type2;
  const gchar *id1;
  const gchar *id2;

  type1 = gf_input_source_get_source_type (source1);
  type2 = gf_input_source_get_source_type (source2);

  id1 = gf_input_source_get_id (source1);
  id2 = gf_input_source_get_id (source2);

  if (g_strcmp0 (type1, type2) == 0 && g_strcmp0 (id1, id2) == 0)
    return TRUE;

  return FALSE;
}

static GfInputSource *
get_new_input_source (GfInputSourceManager *manager,
                      GfInputSource        *current)
{
  GList *keys;
  gpointer index;
  GfInputSource *input_source;

  if (g_hash_table_size (manager->input_sources) == 0)
    return NULL;

  input_source = NULL;

  if (current != NULL)
    {
      GList *sources;
      GList *l;

      sources = g_hash_table_get_values (manager->input_sources);

      for (l = sources; l != NULL; l = g_list_next (l))
        {
          GfInputSource *source;

          source = l->data;

          if (compare_sources (current, source))
            {
              input_source = source;
              break;
            }
        }

      g_list_free (sources);

      if (input_source != NULL)
        return g_object_ref (input_source);
    }

  keys = g_hash_table_get_keys (manager->input_sources);
  keys = g_list_sort (keys, compare_indexes);

  index = g_list_nth_data (keys, 0);
  input_source = g_hash_table_lookup (manager->input_sources, index);
  g_list_free (keys);

  return g_object_ref (input_source);
}

static void
set_per_window_input_source (GfInputSourceManager *manager)
{
  SourceWindow *window;

  window = get_current_window (manager);
  if (window == NULL)
    return;

  if (window->input_sources != manager->input_sources)
    {
      GfInputSource *old;

      g_clear_pointer (&window->input_sources, g_hash_table_unref);
      window->input_sources = g_hash_table_ref (manager->input_sources);

      old = window->current_source;
      window->current_source = get_new_input_source (manager, old);
      g_clear_object (&old);
    }

  if (window->current_source != NULL)
    gf_input_source_activate (window->current_source, FALSE);
}

static GdkFilterReturn
event_filter_cb (GdkXEvent *xevent,
                 GdkEvent  *event,
                 gpointer   user_data)
{
  XEvent *e;

  e = (XEvent *) xevent;

  if (e->type == PropertyNotify)
    {
      XPropertyEvent *p;

      p = (XPropertyEvent *) e;

      if (p->atom == XInternAtom (p->display, "_NET_ACTIVE_WINDOW", True))
        set_per_window_input_source (GF_INPUT_SOURCE_MANAGER (user_data));
    }

  return GDK_FILTER_CONTINUE;
}

static void
change_per_window_source (GfInputSourceManager *manager)
{
  SourceWindow *window;

  if (!manager->sources_per_window)
    return;

  window = get_current_window (manager);
  if (window == NULL)
    return;

  g_clear_pointer (&window->input_sources, g_hash_table_unref);
  g_clear_object (&window->current_source);

  window->input_sources = g_hash_table_ref (manager->input_sources);

  if (manager->current_source != NULL)
    window->current_source = g_object_ref (manager->current_source);
}

static gint
compare_sources_by_index (gconstpointer a,
                          gconstpointer b)
{
  GfInputSource *source1;
  GfInputSource *source2;
  guint index1;
  guint index2;

  source1 = (GfInputSource *) a;
  source2 = (GfInputSource *) b;

  index1 = gf_input_source_get_index (source1);
  index2 = gf_input_source_get_index (source2);

  return index1 - index2;
}

static gchar *
get_symbol_from_char_code (gunichar code)
{
  gchar buffer[6];
  gint length;

  length = g_unichar_to_utf8 (code, buffer);
  buffer[length] = '\0';

  return g_strdup_printf ("%s", buffer);
}

static SourceInfo *
source_info_new (const gchar *type,
                 const gchar *id)
{
  SourceInfo *info;

  info = g_new0 (SourceInfo, 1);

  info->type = g_strdup (type);
  info->id = g_strdup (id);

  return info;
}

static void
source_info_free (gpointer data)
{
  SourceInfo *info;

  info = (SourceInfo *) data;

  g_free (info->type);
  g_free (info->id);

  g_free (info);
}

static void
switch_input_changed_cb (GSettings *settings,
                         gchar     *key,
                         gpointer   user_data)
{
  GfInputSourceManager *manager;
  guint action;
  gchar **keybindings;
  gint i;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);
  action = manager->switch_source_action;

  if (action != 0)
    {
      gf_keybindings_ungrab (manager->keybindings, action);
      action = 0;
    }

  keybindings = g_settings_get_strv (settings, KEY_SWITCH_INPUT_SOURCE);

  /* There might be multiple keybindings set, but we will grab only one. */
  for (i = 0; keybindings[i] != NULL; i++)
    {
      action = gf_keybindings_grab (manager->keybindings, keybindings[i]);

      if (action != 0)
        break;
    }

  g_strfreev (keybindings);

  manager->switch_source_action = action;
}

static void
switch_input_backward_changed_cb (GSettings *settings,
                                  gchar     *key,
                                  gpointer   user_data)
{
  GfInputSourceManager *manager;
  guint action;
  gchar **keybindings;
  gint i;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);
  action = manager->switch_source_backward_action;

  if (action != 0)
    {
      gf_keybindings_ungrab (manager->keybindings, action);
      action = 0;
    }

  keybindings = g_settings_get_strv (settings, KEY_SWITCH_INPUT_SOURCE_BACKWARD);

  /* There might be multiple keybindings set, but we will grab only one. */
  for (i = 0; keybindings[i] != NULL; i++)
    {
      action = gf_keybindings_grab (manager->keybindings, keybindings[i]);

      if (action != 0)
        break;
    }

  g_strfreev (keybindings);

  manager->switch_source_backward_action = action;
}

static void
fade_finished_cb (GfInputSourcePopup *popup,
                  gpointer            user_data)
{
  GfInputSourceManager *manager;
  GtkWidget *widget;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);
  widget = GTK_WIDGET (popup);

  gtk_widget_destroy (widget);
  manager->popup = NULL;
}

static gboolean
modifiers_accelerator_activated_cb (GfKeybindings    *keybindings,
                                    GfKeybindingType  keybinding_type,
                                    gpointer          user_data)
{
  GfInputSourceManager *manager;
  guint size;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);

  size = g_hash_table_size (manager->input_sources);
  if (size == 0)
    {
      gf_keyboard_manager_ungrab (manager->keyboard_manager, GDK_CURRENT_TIME);
      return TRUE;
    }

  if (keybinding_type == GF_KEYBINDING_ISO_NEXT_GROUP)
    {
      gf_input_source_manager_activate_next_source (manager);
    }
  else if (keybinding_type == GF_KEYBINDING_ISO_FIRST_GROUP)
    {
      GfInputSource *first_source;

      first_source = g_hash_table_lookup (manager->input_sources,
                                          GUINT_TO_POINTER (0));

      gf_input_source_activate (first_source, TRUE);
    }
  else if (keybinding_type == GF_KEYBINDING_ISO_LAST_GROUP)
    {
      GList *keys;
      GfInputSource *last_source;

      keys = g_hash_table_get_keys (manager->input_sources);
      keys = g_list_sort (keys, compare_indexes);

      last_source = g_hash_table_lookup (manager->input_sources,
                                         g_list_nth_data (keys, size - 1));

      gf_input_source_activate (last_source, TRUE);
      g_list_free (keys);
    }

  return TRUE;
}

static void
accelerator_activated_cb (GfKeybindings *keybindings,
                          guint          action,
                          const gchar   *device_node,
                          guint          device_id,
                          guint          timestamp,
                          gpointer       user_data)
{
  GfInputSourceManager *manager;
  gboolean backward;
  guint keyval;
  guint modifiers;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);

  if (action != manager->switch_source_action &&
      action != manager->switch_source_backward_action)
    return;

  if (g_list_length (manager->mru_sources) < 2)
    return;

  if (manager->popup != NULL)
    return;

  backward = action == manager->switch_source_backward_action;
  keyval = gf_keybindings_get_keyval (manager->keybindings, action);
  modifiers = gf_keybindings_get_modifiers (manager->keybindings, action);

  manager->popup = gf_input_source_popup_new (manager->mru_sources, backward,
                                              keyval, modifiers);

  g_signal_connect (manager->popup, "fade-finished",
                    G_CALLBACK (fade_finished_cb), manager);

  gtk_widget_show (manager->popup);
}

static char *
get_iso_group_from_xkb_options (gchar **xkb_options)
{
  gchar **p;
  char *option;

  option = NULL;

  for (p = xkb_options; p && *p; ++p)
    {
      if (!g_str_has_prefix (*p, "grp:"))
        continue;

      option = g_strdup (*p + 4);
      break;
    }

  return option;
}

static void
keybindings_init (GfInputSourceManager *manager)
{
  gchar **options;
  char *iso_group;

  manager->wm_keybindings = g_settings_new (DESKTOP_WM_KEYBINDINGS_SCHEMA);
  manager->keybindings = gf_keybindings_new ();

  g_signal_connect (manager->wm_keybindings,
                    "changed::" KEY_SWITCH_INPUT_SOURCE,
                    G_CALLBACK (switch_input_changed_cb),
                    manager);

  g_signal_connect (manager->wm_keybindings,
                    "changed::" KEY_SWITCH_INPUT_SOURCE_BACKWARD,
                    G_CALLBACK (switch_input_backward_changed_cb),
                    manager);

  options = gf_input_source_settings_get_xkb_options (manager->settings);
  iso_group = get_iso_group_from_xkb_options (options);
  g_strfreev (options);

  gf_keybindings_grab_iso_group (manager->keybindings, iso_group);
  g_free (iso_group);

  g_signal_connect (manager->keybindings, "accelerator-activated",
                    G_CALLBACK (accelerator_activated_cb), manager);
  g_signal_connect (manager->keybindings, "modifiers-accelerator-activated",
                    G_CALLBACK (modifiers_accelerator_activated_cb), manager);

  switch_input_changed_cb (manager->wm_keybindings, NULL, manager);
  switch_input_backward_changed_cb (manager->wm_keybindings, NULL, manager);
}

static GList *
get_source_info_list (GfInputSourceManager *manager)
{
  GVariant *sources;
  GnomeXkbInfo *xkb_info;
  GList *list;
  gsize size;
  gsize i;

  sources = gf_input_source_settings_get_sources (manager->settings);
  xkb_info = gf_keyboard_manager_get_xkb_info (manager->keyboard_manager);

  list = NULL;
  size = g_variant_n_children (sources);

  for (i = 0; i < size; i++)
    {
      const gchar *type;
      const gchar *id;
      SourceInfo *info;

      g_variant_get_child (sources, i, "(&s&s)", &type, &id);
      info = NULL;

      if (g_strcmp0 (type, INPUT_SOURCE_TYPE_XKB) == 0)
        {
          gboolean exists;

          exists = gnome_xkb_info_get_layout_info (xkb_info, id, NULL,
                                                   NULL, NULL, NULL);

          if (exists)
            info = source_info_new (type, id);
        }
      else if (g_strcmp0 (type, INPUT_SOURCE_TYPE_IBUS) == 0)
        {
          IBusEngineDesc *engine_desc;

          if (manager->disable_ibus)
            continue;

          engine_desc = gf_ibus_manager_get_engine_desc (manager->ibus_manager,
                                                         id);

          if (engine_desc == NULL)
            continue;

          info = source_info_new (type, id);
        }

      if (info != NULL)
        list = g_list_append (list, info);
    }

  g_variant_unref (sources);

  if (list == NULL)
    {
      const gchar *type;
      const gchar *id;
      SourceInfo *info;

      type = INPUT_SOURCE_TYPE_XKB;
      id = gf_keyboard_manager_get_default_layout (manager->keyboard_manager);

      info = source_info_new (type, id);
      list = g_list_append (list, info);
    }

  return list;
}

static void
current_input_source_changed (GfInputSourceManager *manager,
                              GfInputSource        *new_source)
{
  GfInputSource *old_source;
  GList *l;

  old_source = manager->current_source;
  manager->current_source = new_source;

  g_signal_emit (manager, signals[SIGNAL_CURRENT_SOURCE_CHANGED],
                 0, old_source);

  for (l = manager->mru_sources; l != NULL; l = g_list_next (l))
    {
      GfInputSource *source;

      source = GF_INPUT_SOURCE (l->data);

      if (compare_sources (source, new_source))
        {
          manager->mru_sources = g_list_remove_link (manager->mru_sources, l);
          manager->mru_sources = g_list_concat (l, manager->mru_sources);

          break;
        }
    }

  change_per_window_source (manager);
}

static void
engine_set_cb (GfIBusManager *manager,
               gpointer       user_data)
{
  GfInputSourceManager *source_manager;

  source_manager = GF_INPUT_SOURCE_MANAGER (user_data);

  if (source_manager->reloading)
    return;

  gf_keyboard_manager_ungrab (source_manager->keyboard_manager,
                              GDK_CURRENT_TIME);
}

static void
save_mru_list (GfInputSourceManager *manager)
{
  GVariantBuilder builder;
  GList *l;
  GVariant *variant;

  /* If IBus is not ready we don't have a full picture of all the available
   * sources, so don't update the setting.
   */
  if (!manager->ibus_ready)
    return;

  /*If IBus is temporarily disabled, don't update the setting. */
  if (manager->disable_ibus)
    return;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));

  for (l = manager->mru_sources; l != NULL; l = g_list_next (l))
    {
      GfInputSource *source;
      const gchar *type;
      const gchar *id;

      source = GF_INPUT_SOURCE (l->data);
      type = gf_input_source_get_source_type (source);
      id = gf_input_source_get_id (source);

      g_variant_builder_add (&builder, "(ss)", type, id);
    }

  variant = g_variant_builder_end (&builder);
  gf_input_source_settings_set_mru_sources (manager->settings, variant);
}

static void
activate_cb (GfInputSource        *source,
             gboolean              interactive,
             GfInputSourceManager *manager)
{
  const gchar *xkb_id;
  const gchar *type;
  const gchar *engine;

  xkb_id = gf_input_source_get_xkb_id (source);

  /*
   * The focus changes during grab/ungrab may trick the client into hiding
   * UI containing the currently focused entry. So grab/ungrab are not called
   * when 'set-content-type' signal is received. E.g. Focusing on a password
   * entry in a popup in Xorg Firefox will emit 'set-content-type' signal.
   *
   * https://gitlab.gnome.org/GNOME/gnome-shell/issues/391
   * https://gitlab.gnome.org/GNOME/gnome-flashback/issues/15
   */
  if (!manager->reloading)
    gf_keyboard_manager_grab (manager->keyboard_manager, GDK_CURRENT_TIME);

  gf_keyboard_manager_apply (manager->keyboard_manager, xkb_id);

  if (manager->ibus_manager == NULL)
    {
      current_input_source_changed (manager, source);
      return;
    }

  type = gf_input_source_get_source_type (source);

  /*
   * All the "xkb:..." IBus engines simply "echo" back symbols, despite their
   * naming implying differently, so we always set one in order for XIM
   * applications to work given that we set XMODIFIERS=@im=ibus in the first
   * place so that they can work without restarting when/if the user adds an
   * IBus input source.
   */
  if (g_strcmp0 (type, INPUT_SOURCE_TYPE_IBUS) == 0)
    engine = gf_input_source_get_id (source);
  else
    engine = "xkb:us::eng";

  gf_ibus_manager_set_engine (manager->ibus_manager, engine);
  current_input_source_changed (manager, source);

  if (interactive)
    save_mru_list (manager);
}

static gchar **
get_ibus_engine_list (GHashTable *sources)
{
  GList *list;
  guint size;
  guint i;
  gchar **engines;
  GList *l;

  list = g_hash_table_get_keys (sources);
  size = g_hash_table_size (sources);
  i = 0;

  engines = g_new0 (gchar *, size + 1);

  for (l = list; l != NULL; l = g_list_next (l))
    {
      const gchar *engine;

      engine = (const gchar *) l->data;
      engines[i++] = g_strdup (engine);
    }

  engines[i] = NULL;

  g_list_free (list);

  return engines;
}

static void
sources_by_name_add (GHashTable    *sources,
                     GfInputSource *source)
{
  const gchar *short_name;
  GList *list;

  short_name = gf_input_source_get_short_name (source);
  list = g_hash_table_lookup (sources, short_name);

  if (list == NULL)
    g_hash_table_insert (sources, g_strdup (short_name), list);

  list = g_list_append (list, source);
  g_hash_table_replace (sources, g_strdup (short_name), list);
}

static void
sources_by_name_update (GHashTable *input_sources,
                        GHashTable *sources_by_name)
{
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init (&iter, input_sources);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      GfInputSource *source;
      const gchar *short_name;
      GList *list;

      source = (GfInputSource *) value;
      short_name = gf_input_source_get_short_name (source);

      list = g_hash_table_lookup (sources_by_name, short_name);
      if (list == NULL)
        continue;

      if (g_list_length (list) > 1)
        {
          guint index;
          gchar *symbol;
          gchar *new_name;

          index = g_list_index (list, source);

          symbol = get_symbol_from_char_code (0x2080 + index + 1);
          new_name = g_strdup_printf ("%s%s", short_name, symbol);
          g_free (symbol);

          gf_input_source_set_short_name (source, new_name);
          g_free (new_name);
        }
    }
}

static gboolean
sources_by_name_free (gpointer key,
                      gpointer value,
                      gpointer user_data)
{
  GList *list;

  list = (GList *) value;

  g_list_free (list);

  return TRUE;
}

static void
init_mru_sources (GfInputSourceManager *manager,
                  GList                *sources)
{
  GVariant *mru_sources;
  GList *list;
  gsize size;
  gsize i;

  mru_sources = gf_input_source_settings_get_mru_sources (manager->settings);

  list = NULL;
  size = g_variant_n_children (mru_sources);

  for (i = 0; i < size; i++)
    {
      const gchar *mru_type;
      const gchar *mru_id;
      guint j;

      g_variant_get_child (mru_sources, i, "(&s&s)", &mru_type, &mru_id);

      for (j = 0; j < g_list_length (sources); j++)
        {
          GfInputSource *source;
          const gchar *type;
          const gchar *id;

          source = g_list_nth_data (sources, j);
          type = gf_input_source_get_source_type (source);
          id = gf_input_source_get_id (source);

          if (g_strcmp0 (mru_type, type) == 0 && g_strcmp0 (mru_id, id) == 0)
            {
              list = g_list_append (list, source);
              break;
            }
        }
    }

  g_variant_unref (mru_sources);

  g_assert (manager->mru_sources == NULL);
  manager->mru_sources = list;
}

static void
update_mru_sources_list (GfInputSourceManager *manager)
{
  GList *sources;
  GList *mru_sources;
  GList *l1;
  GList *l2;

  if (!manager->disable_ibus && manager->mru_sources_backup != NULL)
    {
      if (manager->mru_sources != NULL)
        g_list_free (manager->mru_sources);

      manager->mru_sources = manager->mru_sources_backup;
      manager->mru_sources_backup = NULL;
    }

  sources = g_hash_table_get_values (manager->input_sources);
  sources = g_list_sort (sources, compare_sources_by_index);

  /* Initialize from settings when we have no MRU sources list */
  if (manager->mru_sources == NULL)
    init_mru_sources (manager, sources);

  mru_sources = NULL;
  for (l1 = manager->mru_sources; l1 != NULL; l1 = g_list_next (l1))
    {
      for (l2 = sources; l2 != NULL; l2 = g_list_next (l2))
        {
          GfInputSource *source1;
          GfInputSource *source2;

          source1 = (GfInputSource *) l1->data;
          source2 = (GfInputSource *) l2->data;

          if (!compare_sources (source1, source2))
            continue;

          sources = g_list_remove_link (sources, l2);
          mru_sources = g_list_concat (mru_sources, l2);
          break;
        }
    }

  mru_sources = g_list_concat (mru_sources, sources);

  if (manager->mru_sources != NULL)
    g_list_free (manager->mru_sources);
  manager->mru_sources = mru_sources;

  if (manager->mru_sources != NULL)
    {
      GfInputSource *source;

      source = (GfInputSource *) g_list_nth_data (manager->mru_sources, 0);

      gf_input_source_activate (source, FALSE);
    }
}

static void
sources_changed_cb (GfInputSourceSettings *settings,
                    gpointer               user_data)
{
  GfInputSourceManager *manager;
  GList *source_infos;
  GList *l;
  GHashTable *sources_by_name;
  guint length;
  gchar **ids;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);
  source_infos = get_source_info_list (manager);

  if (manager->input_sources != NULL)
    g_hash_table_unref (manager->input_sources);
  manager->input_sources = g_hash_table_new_full (NULL, NULL, NULL,
                                                  g_object_unref);

  if (manager->ibus_sources != NULL)
    g_hash_table_destroy (manager->ibus_sources);
  manager->ibus_sources = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free, g_object_unref);

  g_clear_pointer (&manager->mru_sources, g_list_free);
  g_clear_pointer (&manager->mru_sources_backup, g_list_free);

  manager->current_source = NULL;

  sources_by_name = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free, NULL);

  length = g_list_length (source_infos);
  ids = g_new0 (gchar *, length + 1);

  for (l = source_infos; l != NULL; l = g_list_next (l))
    {
      SourceInfo *info;
      gint position;
      GfInputSource *source;

      info = (SourceInfo *) l->data;
      position = g_list_position (source_infos, l);

      if (g_strcmp0 (info->type, INPUT_SOURCE_TYPE_IBUS) == 0)
        {
          source = gf_input_source_ibus_new (manager->ibus_manager,
                                             info->id,
                                             position);
        }
      else
        {
          GnomeXkbInfo *xkb_info;

          xkb_info = gf_keyboard_manager_get_xkb_info (manager->keyboard_manager);

          source = gf_input_source_xkb_new (xkb_info, info->id, position);
        }

      g_signal_connect (source, "activate",
                        G_CALLBACK (activate_cb), manager);

      g_hash_table_insert (manager->input_sources, GINT_TO_POINTER (position),
                           g_object_ref (source));

      if (g_strcmp0 (info->type, INPUT_SOURCE_TYPE_IBUS) == 0)
        g_hash_table_insert (manager->ibus_sources, g_strdup (info->id),
                             g_object_ref (source));

      sources_by_name_add (sources_by_name, source);

      ids[position] = g_strdup (gf_input_source_get_xkb_id (source));
      g_object_unref (source);
    }

  ids[length] = NULL;

  g_list_free_full (source_infos, source_info_free);

  sources_by_name_update (manager->input_sources, sources_by_name);
  g_hash_table_foreach_remove (sources_by_name, sources_by_name_free, NULL);
  g_hash_table_destroy (sources_by_name);

  g_signal_emit (manager, signals[SIGNAL_SOURCES_CHANGED], 0);

  gf_keyboard_manager_set_user_layouts (manager->keyboard_manager, ids);
  g_strfreev (ids);

  update_mru_sources_list (manager);

  /*
   * All IBus engines are preloaded here to reduce the launching time when
   * users switch the input sources.
   */
  ids = get_ibus_engine_list (manager->ibus_sources);
  gf_ibus_manager_preload_engines (manager->ibus_manager, ids);
  g_strfreev (ids);
}

static void
xkb_options_changed_cb (GfInputSourceSettings *settings,
                        gpointer               user_data)
{
  GfInputSourceManager *manager;
  gchar **options;
  char *iso_group;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);

  options = gf_input_source_settings_get_xkb_options (manager->settings);

  iso_group = get_iso_group_from_xkb_options (options);
  gf_keybindings_grab_iso_group (manager->keybindings, iso_group);
  g_free (iso_group);

  gf_keyboard_manager_set_xkb_options (manager->keyboard_manager, options);
  g_strfreev (options);

  gf_keyboard_manager_reapply (manager->keyboard_manager);
}

static void
per_window_changed_cb (GfInputSourceSettings *settings,
                       gpointer               user_data)
{
  GfInputSourceManager *manager;
  gboolean per_window;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);
  per_window = gf_input_source_settings_get_per_window (settings);

  manager->sources_per_window = per_window;

  if (manager->sources_per_window && !manager->event_filter_added)
    {
      g_assert (manager->source_windows == NULL);
      manager->source_windows = g_hash_table_new_full (NULL, NULL, NULL,
                                                       source_window_free);

      gdk_window_add_filter (NULL, event_filter_cb, manager);
      manager->event_filter_added = TRUE;
    }
  else if (!manager->sources_per_window && manager->event_filter_added)
    {
      gdk_window_remove_filter (NULL, event_filter_cb, manager);
      manager->event_filter_added = FALSE;

      g_hash_table_destroy (manager->source_windows);
      manager->source_windows = NULL;
    }
}

static void
input_source_settings_init (GfInputSourceManager *manager)
{
  manager->settings = gf_input_source_settings_new ();

  g_signal_connect (manager->settings, "sources-changed",
                    G_CALLBACK (sources_changed_cb), manager);
  g_signal_connect (manager->settings, "xkb-options-changed",
                    G_CALLBACK (xkb_options_changed_cb), manager);
  g_signal_connect (manager->settings, "per-window-changed",
                    G_CALLBACK (per_window_changed_cb), manager);

  per_window_changed_cb (manager->settings, manager);
}

static void
ready_cb (GfIBusManager *ibus_manager,
          gboolean       ready,
          gpointer       user_data)
{
  GfInputSourceManager *manager;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);

  if (manager->ibus_ready == ready)
    return;

  manager->ibus_ready = ready;

  if (manager->mru_sources != NULL)
    {
      g_list_free (manager->mru_sources);
      manager->mru_sources = NULL;
    }

  sources_changed_cb (manager->settings, manager);
}

static void
properties_registered_cb (GfIBusManager *ibus_manager,
                          const gchar   *engine_name,
                          IBusPropList  *prop_list,
                          gpointer       user_data)
{
  GfInputSourceManager *manager;
  GfInputSource *source;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);
  source = (GfInputSource *) g_hash_table_lookup (manager->ibus_sources,
                                                  engine_name);

  if (!source || !GF_IS_INPUT_SOURCE_IBUS (source))
    return;

  gf_input_source_ibus_set_properties (GF_INPUT_SOURCE_IBUS (source),
                                       prop_list);

  if (compare_sources (source, manager->current_source))
    g_signal_emit (manager, signals[SIGNAL_CURRENT_SOURCE_CHANGED], 0, NULL);
}

static gboolean
update_sub_property (IBusPropList *prop_list,
                     IBusProperty *prop)
{
  IBusProperty *p;
  guint index;

  if (!prop_list)
    return FALSE;

  index = 0;
  while ((p = ibus_prop_list_get (prop_list, index)) != NULL)
    {
      const gchar *p_key;
      IBusPropType p_type;
      const gchar *prop_key;
      IBusPropType prop_type;

      p_key = ibus_property_get_key (p);
      p_type = ibus_property_get_prop_type (p);
      prop_key = ibus_property_get_key (prop);
      prop_type = ibus_property_get_prop_type (prop);

      if (g_strcmp0 (p_key, prop_key) == 0 && p_type == prop_type)
        {
          ibus_prop_list_update_property (prop_list, prop);
          return TRUE;
        }
      else if (p_type == PROP_TYPE_MENU)
        {
          IBusPropList *sub_props;

          sub_props = ibus_property_get_sub_props (p);
          if (update_sub_property (sub_props, prop))
            return TRUE;
        }

      index++;
    }

  return FALSE;
}

static void
property_updated_cb (GfIBusManager *ibus_manager,
                     const gchar   *engine_name,
                     IBusProperty  *property,
                     gpointer       user_data)
{
  GfInputSourceManager *manager;
  GfInputSource *source;
  IBusPropList *prop_list;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);
  source = (GfInputSource *) g_hash_table_lookup (manager->ibus_sources,
                                                  engine_name);

  if (!source || !GF_IS_INPUT_SOURCE_IBUS (source))
    return;

  prop_list = gf_input_source_ibus_get_properties (GF_INPUT_SOURCE_IBUS (source));

  if (!update_sub_property (prop_list, property))
    return;

  if (compare_sources (source, manager->current_source))
    g_signal_emit (manager, signals[SIGNAL_CURRENT_SOURCE_CHANGED], 0, NULL);
}

static void
set_content_type_cb (GfIBusManager *ibus_manager,
                     guint          purpose,
                     guint          hints,
                     gpointer       user_data)
{
  GfInputSourceManager *manager;

  manager = GF_INPUT_SOURCE_MANAGER (user_data);

  if (purpose == IBUS_INPUT_PURPOSE_PASSWORD)
    {
      GList *keys1;
      GList *keys2;

      keys1 = g_hash_table_get_keys (manager->input_sources);
      keys2 = g_hash_table_get_keys (manager->ibus_sources);

      if (g_list_length (keys1) == g_list_length (keys2))
        {
          g_list_free (keys1);
          g_list_free (keys2);

          return;
        }

      g_list_free (keys1);
      g_list_free (keys2);

      if (manager->disable_ibus)
        return;

      manager->disable_ibus = TRUE;
      manager->mru_sources_backup = g_list_copy (manager->mru_sources);
    }
  else
    {
      if (!manager->disable_ibus)
        return;

      manager->disable_ibus = FALSE;
    }

  gf_input_source_manager_reload (manager);
}

static void
gf_input_source_manager_constructed (GObject *object)
{
  GfInputSourceManager *manager;

  manager = GF_INPUT_SOURCE_MANAGER (object);

  G_OBJECT_CLASS (gf_input_source_manager_parent_class)->constructed (object);

  g_signal_connect (manager->ibus_manager, "ready",
                    G_CALLBACK (ready_cb), manager);
  g_signal_connect (manager->ibus_manager, "properties-registered",
                    G_CALLBACK (properties_registered_cb), manager);
  g_signal_connect (manager->ibus_manager, "property-updated",
                    G_CALLBACK (property_updated_cb), manager);
  g_signal_connect (manager->ibus_manager, "set-content-type",
                    G_CALLBACK (set_content_type_cb), manager);

  g_signal_connect (manager->ibus_manager, "engine-set",
                    G_CALLBACK (engine_set_cb), manager);
}

static void
gf_input_source_manager_dispose (GObject *object)
{
  GfInputSourceManager *manager;

  manager = GF_INPUT_SOURCE_MANAGER (object);

  g_clear_object (&manager->wm_keybindings);
  g_clear_object (&manager->keybindings);

  g_clear_object (&manager->settings);

  g_clear_object (&manager->keyboard_manager);

  if (manager->input_sources != NULL)
    {
      g_hash_table_destroy (manager->input_sources);
      manager->input_sources = NULL;
    }

  if (manager->ibus_sources != NULL)
    {
      g_hash_table_destroy (manager->ibus_sources);
      manager->ibus_sources = NULL;
    }

  if (manager->mru_sources != NULL)
    {
      g_list_free (manager->mru_sources);
      manager->mru_sources = NULL;
    }

  if (manager->mru_sources_backup != NULL)
    {
      g_list_free (manager->mru_sources_backup);
      manager->mru_sources_backup = NULL;
    }

  if (manager->popup != NULL)
    {
      gtk_widget_destroy (manager->popup);
      manager->popup = NULL;
    }

  if (manager->event_filter_added)
    {
      gdk_window_remove_filter (NULL, event_filter_cb, manager);
      manager->event_filter_added = FALSE;
    }

  if (manager->source_windows != NULL)
    {
      g_hash_table_destroy (manager->source_windows);
      manager->source_windows = NULL;
    }

  G_OBJECT_CLASS (gf_input_source_manager_parent_class)->dispose (object);
}

static void
gf_input_source_manager_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GfInputSourceManager *manager;

  manager = GF_INPUT_SOURCE_MANAGER (object);

  switch (property_id)
    {
      case PROP_IBUS_MANAGER:
        manager->ibus_manager = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_input_source_manager_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GfInputSourceManager *manager;

  manager = GF_INPUT_SOURCE_MANAGER (object);

  switch (property_id)
    {
      case PROP_IBUS_MANAGER:
        g_value_set_object (value, manager->ibus_manager);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_input_source_manager_class_init (GfInputSourceManagerClass *manager_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (manager_class);

  object_class->constructed = gf_input_source_manager_constructed;
  object_class->dispose = gf_input_source_manager_dispose;
  object_class->get_property = gf_input_source_manager_get_property;
  object_class->set_property = gf_input_source_manager_set_property;

  signals[SIGNAL_SOURCES_CHANGED] =
    g_signal_new ("sources-changed", G_TYPE_FROM_CLASS (manager_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals[SIGNAL_CURRENT_SOURCE_CHANGED] =
    g_signal_new ("current-source-changed", G_TYPE_FROM_CLASS (manager_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
                  GF_TYPE_INPUT_SOURCE);

  properties[PROP_IBUS_MANAGER] =
    g_param_spec_object ("ibus-manager", "IBus Manager",
                         "An instance of GfIBusManager",
                         GF_TYPE_IBUS_MANAGER,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gf_input_source_manager_init (GfInputSourceManager *manager)
{
  manager->keyboard_manager = gf_keyboard_manager_new ();

  input_source_settings_init (manager);
  keybindings_init (manager);
}

GfInputSourceManager *
gf_input_source_manager_new (GfIBusManager *ibus_manager)
{
  return g_object_new (GF_TYPE_INPUT_SOURCE_MANAGER,
                       "ibus-manager", ibus_manager,
                       NULL);
}

void
gf_input_source_manager_reload (GfInputSourceManager *manager)
{
  gchar **options;

  manager->reloading = TRUE;

  options = gf_input_source_settings_get_xkb_options (manager->settings);

  gf_keyboard_manager_set_xkb_options (manager->keyboard_manager, options);
  g_strfreev (options);

  sources_changed_cb (manager->settings, manager);

  manager->reloading = FALSE;
}

GfInputSource *
gf_input_source_manager_get_current_source (GfInputSourceManager *manager)
{
  return manager->current_source;
}

GList *
gf_input_source_manager_get_input_sources (GfInputSourceManager *manager)
{
  GList *sources;

  sources = g_hash_table_get_values (manager->input_sources);
  sources = g_list_sort (sources, compare_sources_by_index);

  return sources;
}

void
gf_input_source_manager_activate_next_source (GfInputSourceManager *manager)
{
  guint size;
  GList *keys;
  GfInputSource *current_source;
  guint next_index;
  GfInputSource *next_source;

  size = g_hash_table_size (manager->input_sources);
  if (size == 0)
    return;

  keys = g_hash_table_get_keys (manager->input_sources);
  keys = g_list_sort (keys, compare_indexes);

  current_source = manager->current_source;

  if (current_source == NULL)
    current_source = g_hash_table_lookup (manager->input_sources,
                                          GUINT_TO_POINTER (0));

  next_index = gf_input_source_get_index (current_source) + 1;
  if (next_index > GPOINTER_TO_UINT (g_list_nth_data (keys, size - 1)))
    next_index = 0;

  next_source = g_hash_table_lookup (manager->input_sources,
                                     GUINT_TO_POINTER (next_index));

  gf_input_source_activate (next_source, TRUE);
  g_list_free (keys);
}
