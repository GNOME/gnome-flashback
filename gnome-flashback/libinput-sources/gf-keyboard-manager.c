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
 */

#include "config.h"

#include <glib.h>
#include <gdk/gdkx.h>
#include <libgnome-desktop/gnome-languages.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>

#include "gf-keyboard-manager.h"

#define DEFAULT_XKB_RULES_FILE "evdev"
#define DEFAULT_XKB_MODEL "pc105+inet"

#define DEFAULT_LOCALE "en_US"
#define DEFAULT_LAYOUT "us"
#define DEFAULT_VARIANT ""

/* The XKB protocol doesn't allow for more than 4 layouts in a keymap. */
#define MAX_LAYOUTS_PER_GROUP 4

typedef struct _LayoutInfo LayoutInfo;
struct _LayoutInfo
{
  gchar       *id;
  gchar       *layout;
  gchar       *variant;

  LayoutInfo **group;
  guint        index;
};

struct _GfKeyboardManager
{
  GObject       parent;

  Display      *xdisplay;

  gint          xkb_event_base;
  gint          xkb_error_base;

  GnomeXkbInfo *xkb_info;

  LayoutInfo   *locale;

  GHashTable   *layout_infos;
  LayoutInfo   *current;

  gchar        *layouts;
  gchar        *variants;
  gchar        *options;

  guint         locked_group;
};

G_DEFINE_TYPE (GfKeyboardManager, gf_keyboard_manager, G_TYPE_OBJECT)

static LayoutInfo *
layout_info_new (const gchar *id,
                 const gchar *layout,
                 const gchar *variant)
{
  LayoutInfo *layout_info;

  layout_info = g_new0 (LayoutInfo, 1);

  layout_info->id = g_strdup (id);
  layout_info->layout = g_strdup (layout);
  layout_info->variant = g_strdup (variant);

  layout_info->group = NULL;

  return layout_info;
}

static void
layout_info_free (gpointer data)
{
  LayoutInfo *layout_info;

  layout_info = (LayoutInfo *) data;

  g_free (layout_info->id);
  g_free (layout_info->layout);
  g_free (layout_info->variant);

  if (layout_info->index == 0)
    g_free (layout_info->group);

  g_free (layout_info);
}

static void
upload_xkb_description (Display              *xdisplay,
                        const gchar          *rules_file_path,
                        XkbComponentNamesRec *component_names,
                        XkbRF_VarDefsRec     *var_defs)
{
  guint want_mask;
  guint need_mask;
  XkbDescRec *xkb_desc;
  gchar *rules_file;

  want_mask = XkbGBN_AllComponentsMask;
  need_mask = XkbGBN_AllComponentsMask & (~XkbGBN_GeometryMask);
  xkb_desc = XkbGetKeyboardByName (xdisplay, XkbUseCoreKbd, component_names,
                                   want_mask, need_mask, True);

  if (xkb_desc == NULL)
    {
      g_warning ("Couldn't upload new XKB keyboard description");
      return;
    }

  XkbFreeKeyboard (xkb_desc, 0, True);

  rules_file = g_path_get_basename (rules_file_path);

  if (!XkbRF_SetNamesProp (xdisplay, rules_file, var_defs))
    g_warning ("Couldn't update the XKB root window property");

  g_free (rules_file);
}

static void
get_xkbrf_var_defs (Display           *xdisplay,
                    const char        *layouts,
                    const char        *variants,
                    const char        *options,
                    gchar            **rules,
                    XkbRF_VarDefsRec **var_defs)
{
  gchar *tmp;
  XkbRF_VarDefsRec *defs;

  defs = g_new0 (XkbRF_VarDefsRec, 1);

  tmp = NULL;
  if (!XkbRF_GetNamesProp (xdisplay, &tmp, defs) || !tmp)
    {
      tmp = g_strdup (DEFAULT_XKB_RULES_FILE);

      defs->model = g_strdup (DEFAULT_XKB_MODEL);
      defs->layout = NULL;
      defs->variant = NULL;
      defs->options = NULL;
    }

  g_free (defs->layout);
  defs->layout = g_strdup (layouts);

  g_free (defs->variant);
  defs->variant = g_strdup (variants);

  g_free (defs->options);
  defs->options = g_strdup (options);

  if (tmp[0] == '/')
    *rules = g_strdup (tmp);
  else
    *rules = g_build_filename (XKB_BASE, "rules", tmp, NULL);
  g_free (tmp);

  *var_defs = defs;
}

static void
xkbrf_var_defs_free (XkbRF_VarDefsRec *var_defs)
{
  g_free (var_defs->model);
  g_free (var_defs->layout);
  g_free (var_defs->variant);
  g_free (var_defs->options);

  g_free (var_defs);
}

static void
xkb_component_names_free (XkbComponentNamesRec *xkb_comp_names)
{
  g_free (xkb_comp_names->keymap);
  g_free (xkb_comp_names->keycodes);
  g_free (xkb_comp_names->types);
  g_free (xkb_comp_names->compat);
  g_free (xkb_comp_names->symbols);
  g_free (xkb_comp_names->geometry);

  g_free (xkb_comp_names);
}

static void
set_keymap (GfKeyboardManager *manager)
{
  gchar *rules_file_path;
  XkbRF_VarDefsRec *xkb_var_defs;
  XkbRF_RulesRec *xkb_rules;

  if (manager->xkb_event_base == -1)
    return;

  if (!manager->layouts || !manager->variants || !manager->options)
    return;

  xkb_var_defs = NULL;
  get_xkbrf_var_defs (manager->xdisplay,
                      manager->layouts, manager->variants, manager->options,
                      &rules_file_path, &xkb_var_defs);

  xkb_rules = XkbRF_Load (rules_file_path, NULL, True, True);

  if (xkb_rules != NULL)
    {
      XkbComponentNamesRec *xkb_comp_names;

      xkb_comp_names = g_new0 (XkbComponentNamesRec, 1);
      XkbRF_GetComponents (xkb_rules, xkb_var_defs, xkb_comp_names);

      upload_xkb_description (manager->xdisplay, rules_file_path,
                              xkb_comp_names, xkb_var_defs);

      xkb_component_names_free (xkb_comp_names);
      XkbRF_Free (xkb_rules, True);
    }
  else
    g_warning ("Couldn't load XKB rules");

  g_free (rules_file_path);
  xkbrf_var_defs_free (xkb_var_defs);
}

static gchar *
get_layouts_string (GfKeyboardManager  *manager,
                    LayoutInfo        **group)
{
  gchar *layouts;
  gint i;
  gchar *tmp;

  if (group[0] == NULL)
    return g_strdup ("");

  layouts = g_strdup (group[0]->layout);
  for (i = 1; i < (MAX_LAYOUTS_PER_GROUP - 1); i++)
    {
      if (group[i] == NULL)
        continue;

      tmp = g_strdup_printf ("%s,%s", layouts, group[i]->layout);
      g_free (layouts);

      layouts = tmp;
    }

  if (manager->locale != NULL)
    {
      tmp = g_strdup_printf ("%s,%s", layouts, manager->locale->layout);
      g_free (layouts);

      layouts = tmp;
    }

  return layouts;
}

static gchar *
get_variants_string (GfKeyboardManager  *manager,
                     LayoutInfo        **group)
{
  gchar *variants;
  gint i;
  gchar *tmp;

  if (group[0] == NULL)
    return g_strdup ("");

  variants = g_strdup (group[0]->variant);
  for (i = 1; i < (MAX_LAYOUTS_PER_GROUP - 1); i++)
    {
      if (group[i] == NULL)
        continue;

      tmp = g_strdup_printf ("%s,%s", variants, group[i]->variant);
      g_free (variants);

      variants = tmp;
    }

  if (manager->locale != NULL)
    {
      tmp = g_strdup_printf ("%s,%s", variants, manager->locale->variant);
      g_free (variants);

      variants = tmp;
    }

  return variants;
}

static void
apply_layout_group (GfKeyboardManager  *manager,
                    LayoutInfo        **group)
{
  g_free (manager->layouts);
  manager->layouts = get_layouts_string (manager, group);

  g_free (manager->variants);
  manager->variants = get_variants_string (manager, group);

  set_keymap (manager);
}

static void
apply_layout_index (GfKeyboardManager *manager,
                    guint              index)
{
  if (manager->xkb_event_base == -1)
    return;

  manager->locked_group = index;
  XkbLockGroup (manager->xdisplay, XkbUseCoreKbd, index);
}

static void
device_added_cb (GdkSeat   *seat,
                 GdkDevice *device,
                 gpointer   user_data)
{
  GfKeyboardManager *manager;

  manager = GF_KEYBOARD_MANAGER (user_data);

  if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
    return;

  set_keymap (manager);
}

static void
get_locale_layout_info (GfKeyboardManager *manager)
{
  const gchar *locale;
  const gchar *type;
  const gchar *id;
  const gchar *layout;
  const gchar *variant;

  locale = g_get_language_names ()[0];
  if (g_strrstr (locale, "_") == NULL)
    locale = DEFAULT_LOCALE;

  if (!gnome_get_input_source_from_locale (locale, &type, &id))
    gnome_get_input_source_from_locale (DEFAULT_LOCALE, &type, &id);

  if (gnome_xkb_info_get_layout_info (manager->xkb_info, id, NULL, NULL,
                                      &layout, &variant))
    {
      manager->locale = layout_info_new (id, layout, variant);
    }
  else
    {
      manager->locale = layout_info_new (id, DEFAULT_LAYOUT, DEFAULT_VARIANT);
    }
}

static GdkFilterReturn
filter_func (GdkXEvent *xevent,
             GdkEvent  *event,
             gpointer   user_data)
{
  GfKeyboardManager *manager;
  XEvent *ev;
  XkbEvent *xkb_ev;

  manager = GF_KEYBOARD_MANAGER (user_data);
  ev = (XEvent *) xevent;

  if (ev->type != manager->xkb_event_base)
    return GDK_FILTER_CONTINUE;

  xkb_ev = (XkbEvent *) ev;

  if (xkb_ev->any.xkb_type == XkbStateNotify &&
      xkb_ev->state.changed & XkbGroupLockMask)
    {
      if ((gint) manager->locked_group != xkb_ev->state.locked_group)
        apply_layout_index (manager, manager->locked_group);
    }

  return GDK_FILTER_CONTINUE;
}

static void
gf_keyboard_manager_dispose (GObject *object)
{
  GfKeyboardManager *manager;

  manager = GF_KEYBOARD_MANAGER (object);

  g_clear_object (&manager->xkb_info);

  G_OBJECT_CLASS (gf_keyboard_manager_parent_class)->dispose (object);
}

static void
gf_keyboard_manager_finalize (GObject *object)
{
  GfKeyboardManager *manager;

  manager = GF_KEYBOARD_MANAGER (object);

  gdk_window_remove_filter (NULL, filter_func, manager);

  if (manager->locale != 0)
    {
      layout_info_free (manager->locale);
      manager->locale = NULL;
    }

  if (manager->layout_infos != NULL)
    {
      g_hash_table_destroy (manager->layout_infos);
      manager->layout_infos = NULL;
    }

  g_free (manager->layouts);
  g_free (manager->variants);
  g_free (manager->options);

  G_OBJECT_CLASS (gf_keyboard_manager_parent_class)->finalize (object);
}

static void
gf_keyboard_manager_class_init (GfKeyboardManagerClass *manager_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (manager_class);

  object_class->dispose = gf_keyboard_manager_dispose;
  object_class->finalize = gf_keyboard_manager_finalize;
}

static void
gf_keyboard_manager_init (GfKeyboardManager *manager)
{
  GdkDisplay *display;
  GdkSeat *seat;
  gint xkb_opcode;
  gint xkb_major;
  gint xkb_minor;

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);

  manager->xdisplay = gdk_x11_display_get_xdisplay (display);
  manager->xkb_info = gnome_xkb_info_new ();
  manager->options = g_strdup ("");

  xkb_major = XkbMajorVersion;
  xkb_minor = XkbMinorVersion;

  if (!XkbQueryExtension (manager->xdisplay, &xkb_opcode,
                          &manager->xkb_event_base, &manager->xkb_error_base,
                          &xkb_major, &xkb_minor))
    {
      manager->xkb_event_base = -1;

      g_warning ("X server doesn't have the XKB extension, version %d.%d or "
                 "newer", XkbMajorVersion, XkbMinorVersion);
    }

  g_signal_connect_object (seat, "device-added",
                           G_CALLBACK (device_added_cb), manager,
                           G_CONNECT_AFTER);

  get_locale_layout_info (manager);

  gdk_window_add_filter (NULL, filter_func, manager);
}

/**
 * gf_keyboard_manager_new:
 *
 * Creates a new #GfKeyboardManager.
 *
 * Returns: (transfer full): a newly created #GfKeyboardManager.
 */
GfKeyboardManager *
gf_keyboard_manager_new (void)
{
  return g_object_new (GF_TYPE_KEYBOARD_MANAGER, NULL);
}

GnomeXkbInfo *
gf_keyboard_manager_get_xkb_info (GfKeyboardManager *manager)
{
  return manager->xkb_info;
}

/**
 * gf_keyboard_manager_set_xkb_options:
 * @manager: a #GfKeyboardManager
 * @options: a %NULL-terminated array of xkb options
 */
void
gf_keyboard_manager_set_xkb_options (GfKeyboardManager  *manager,
                                     gchar             **options)
{
  g_free (manager->options);
  manager->options = g_strjoinv (",", options);
}

/**
 * gf_keyboard_manager_set_user_layouts:
 * @manager: a #GfKeyboardManager
 * @ids: a %NULL-terminated array of input source ids
 */
void
gf_keyboard_manager_set_user_layouts (GfKeyboardManager  *manager,
                                      gchar             **ids)
{
  gint i;
  gint j;
  const gchar *layout;
  const gchar *variant;
  LayoutInfo **group;

  manager->current = NULL;

  if (manager->layout_infos != NULL)
    g_hash_table_destroy (manager->layout_infos);

  manager->layout_infos = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free, layout_info_free);

  for (i = 0, j = 0; ids[i] != NULL; i++)
    {
      if (gnome_xkb_info_get_layout_info (manager->xkb_info, ids[i],
                                          NULL, NULL, &layout, &variant))
        {
          LayoutInfo *info;
          guint index;

          if (g_hash_table_contains (manager->layout_infos, ids[i]))
            continue;

          info = layout_info_new (ids[i], layout, variant);

          /*
           * We need to leave one slot on each group free so that we
           * can add a layout containing the symbols for the language
           * used in UI strings to ensure that toolkits can handle
           * mnemonics like Alt+Ф even if the user is actually typing
           * in a different layout.
           */
          index = j % (MAX_LAYOUTS_PER_GROUP - 1);

          if (index == 0)
            group = g_new0 (LayoutInfo *, (MAX_LAYOUTS_PER_GROUP - 1));

          group[index] = info;

          info->group = group;
          info->index = index;

          g_hash_table_insert (manager->layout_infos, g_strdup (ids[i]), info);

          ++j;
        }
    }
}

/**
 * gf_keyboard_manager_apply:
 * @manager: a #GfKeyboardManager
 * @id: the xkb_layout + xkb_variant or xkb_layout if a XKB variant isn't
 *     needed
 */
void
gf_keyboard_manager_apply (GfKeyboardManager *manager,
                           const gchar       *id)
{
  LayoutInfo *info;

  info = (LayoutInfo *) g_hash_table_lookup (manager->layout_infos, id);

  if (info == NULL)
    return;

  if (manager->current != NULL && manager->current->group == info->group)
    {
      if (manager->current->index != info->index)
        apply_layout_index (manager, info->index);
    }
  else
    {
      apply_layout_group (manager, info->group);
      apply_layout_index (manager, info->index);
    }

  manager->current = info;
}

/**
 * gf_keyboard_manager_reapply:
 * @manager: a #GfKeyboardManager
 */
void
gf_keyboard_manager_reapply (GfKeyboardManager *manager)
{
  if (manager->current == NULL)
    return;

  apply_layout_group (manager, manager->current->group);
  apply_layout_index (manager, manager->current->index);
}

/**
 * gf_keyboard_manager_grab:
 * @manager: a #GfKeyboardManager
 * @timestamp: the timestamp of the user interaction (typically a button or
 *     key press event) which triggered this call
 *
 * Returns: %TRUE if grab was successful, %FALSE otherwise
 */
gboolean
gf_keyboard_manager_grab (GfKeyboardManager *manager,
                          guint32            timestamp)
{
  GdkDisplay *display;
  GdkScreen *screen;
  GdkWindow *root;
  Window xroot;
  gint status;
  gint error;

  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);
  root = gdk_screen_get_root_window (screen);
  xroot = gdk_x11_window_get_xid (root);

  gdk_x11_display_error_trap_push (display);
  status = XGrabKeyboard (manager->xdisplay, xroot, False,
                          GrabModeAsync, GrabModeSync,
                          timestamp);
  error = gdk_x11_display_error_trap_pop (display);

  if (error != 0)
    return FALSE;

  if (status != 0)
    return FALSE;

  return TRUE;
}

/**
 * gf_keyboard_manager_ungrab:
 * @manager: a #GfKeyboardManager
 * @timestamp: the timestamp of the user interaction (typically a button or
 *     key press event) which triggered this call
 */
void
gf_keyboard_manager_ungrab (GfKeyboardManager *manager,
                            guint32            timestamp)
{
  XUngrabKeyboard (manager->xdisplay, timestamp);
}

/**
 * gf_keyboard_manager_get_default_layout:
 * @manager: a #GfKeyboardManager
 */
const gchar *
gf_keyboard_manager_get_default_layout (GfKeyboardManager *manager)
{
  return DEFAULT_LAYOUT;
}
