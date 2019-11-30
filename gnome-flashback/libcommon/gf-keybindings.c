/*
 * Copyright (C) 2014 - 2019 Alberts Muktupāvels
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
#include "gf-keybindings.h"

#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include "gf-common-enum-types.h"

#define DESKTOP_INPUT_SOURCES_SCHEMA "org.gnome.desktop.input-sources"

#define KEY_XKB_OPTIONS "xkb-options"

struct _GfKeybindings
{
  GObject     parent;

  GHashTable *keybindings;
  GHashTable *iso_group_keybindings;

  gchar      *iso_group;

  Display    *xdisplay;
  Window      xwindow;

  gint        xkb_event_base;
  gint        xkb_error_base;

  guint       meta_mask;
  guint       super_mask;
  guint       hyper_mask;

  guint       ignored_mask;
};

typedef struct
{
  GfKeybindingType  type;

  gchar            *name;

  guint             keyval;
  GdkModifierType   modifiers;

  guint             keycode;
  guint             mask;

  guint             action;
} Keybinding;

enum
{
  SIGNAL_ACCELERATOR_ACTIVATED,
  SIGNAL_MODIFIERS_ACCELERATOR_ACTIVATED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfKeybindings, gf_keybindings, G_TYPE_OBJECT)

static gboolean
is_valid_accelerator (guint           keyval,
                      GdkModifierType modifiers)
{
  /* Unlike gtk_accelerator_valid(), we want to allow Tab when combined
   * with some modifiers (Alt+Tab and friends)
   */
  return gtk_accelerator_valid (keyval, modifiers) ||
         (keyval == GDK_KEY_Tab && modifiers != 0);
}

static gboolean
devirtualize_modifier (GdkModifierType  modifiers,
                       GdkModifierType  gdk_mask,
                       unsigned int     real_mask,
                       unsigned int    *mask)
{
  if (modifiers & gdk_mask)
    {
      if (real_mask == 0)
        return FALSE;

       *mask |= real_mask;
    }

  return TRUE;
}

static gboolean
devirtualize_modifiers (GfKeybindings   *keybindings,
                        GdkModifierType  modifiers,
                        guint           *mask)
{
  gboolean devirtualized;

  devirtualized = TRUE;
  *mask = 0;

  devirtualized &= devirtualize_modifier (modifiers, GDK_SHIFT_MASK,
                                          ShiftMask, mask);

  devirtualized &= devirtualize_modifier (modifiers, GDK_CONTROL_MASK,
                                          ControlMask, mask);

  devirtualized &= devirtualize_modifier (modifiers, GDK_MOD1_MASK,
                                          Mod1Mask, mask);

  devirtualized &= devirtualize_modifier (modifiers, GDK_META_MASK,
                                          keybindings->meta_mask, mask);

  devirtualized &= devirtualize_modifier (modifiers, GDK_HYPER_MASK,
                                          keybindings->hyper_mask, mask);

  devirtualized &= devirtualize_modifier (modifiers, GDK_SUPER_MASK,
                                          keybindings->super_mask, mask);

  devirtualized &= devirtualize_modifier (modifiers, GDK_MOD2_MASK,
                                          Mod2Mask, mask);

  devirtualized &= devirtualize_modifier (modifiers, GDK_MOD3_MASK,
                                          Mod3Mask, mask);

  devirtualized &= devirtualize_modifier (modifiers, GDK_MOD4_MASK,
                                          Mod4Mask, mask);

  devirtualized &= devirtualize_modifier (modifiers, GDK_MOD5_MASK,
                                          Mod5Mask, mask);

  return devirtualized;
}

static void
change_keygrab (GfKeybindings *keybindings,
                gboolean       grab,
                Keybinding    *keybinding)
{
  guint ignored_mask;
  guint keycode;
  guint mask;
  gint error_code;
  GdkDisplay *display;

  ignored_mask = 0;
  keycode = keybinding->keycode;
  mask = keybinding->mask;
  display = gdk_display_get_default ();

  if (keycode == 0)
    return;

  while (ignored_mask <= keybindings->ignored_mask)
    {
      if (ignored_mask & ~(keybindings->ignored_mask))
        {
          ++ignored_mask;
          continue;
        }

      gdk_x11_display_error_trap_push (display);

      if (grab)
        {
          XGrabKey (keybindings->xdisplay, keycode, mask | ignored_mask,
                    keybindings->xwindow, True, GrabModeAsync, GrabModeSync);
        }
      else
        {
          XUngrabKey (keybindings->xdisplay, keycode, mask | ignored_mask,
                      keybindings->xwindow);
        }

      error_code = gdk_x11_display_error_trap_pop (display);
      if (error_code != 0)
        {
          g_debug ("Failed to grab/ ungrab key. Error code - %d", error_code);
        }

      ++ignored_mask;
    }
}

static Keybinding *
keybinding_new (const gchar     *name,
                guint            keyval,
                GdkModifierType  modifiers,
                guint            keycode,
                guint            mask,
                guint            action)
{
  Keybinding *keybinding;

  keybinding = g_new0 (Keybinding, 1);
  keybinding->type = GF_KEYBINDING_NONE;

  keybinding->name = g_strdup (name);

  keybinding->keyval = keyval;
  keybinding->modifiers = modifiers;

  keybinding->keycode = keycode;
  keybinding->mask = mask;

  keybinding->action = action;

  return keybinding;
}

static Keybinding *
iso_next_group_keybinding_new (guint keycode,
                               guint mask)
{
  Keybinding *keybinding;

  keybinding = g_new0 (Keybinding, 1);
  keybinding->type = GF_KEYBINDING_ISO_NEXT_GROUP;

  keybinding->keycode = keycode;
  keybinding->mask = mask;

  return keybinding;
}

static Keybinding *
iso_first_group_keybinding_new (guint keycode,
                                guint mask)
{
  Keybinding *keybinding;

  keybinding = g_new0 (Keybinding, 1);
  keybinding->type = GF_KEYBINDING_ISO_FIRST_GROUP;

  keybinding->keycode = keycode;
  keybinding->mask = mask;

  return keybinding;
}

static Keybinding *
iso_last_group_keybinding_new (guint keycode,
                               guint mask)
{
  Keybinding *keybinding;

  keybinding = g_new0 (Keybinding, 1);
  keybinding->type = GF_KEYBINDING_ISO_LAST_GROUP;

  keybinding->keycode = keycode;
  keybinding->mask = mask;

  return keybinding;
}

static void
keybinding_free (gpointer data)
{
  Keybinding *keybinding;

  keybinding = (Keybinding *) data;

  g_free (keybinding->name);

  g_free (keybinding);
}

static Keybinding *
get_keybinding (GfKeybindings *keybindings,
                guint          action)
{
  gpointer paction;
  gpointer pkeybinding;
  Keybinding *keybinding;

  paction = GUINT_TO_POINTER (action);
  pkeybinding = g_hash_table_lookup (keybindings->keybindings, paction);
  keybinding = (Keybinding *) pkeybinding;

  return keybinding;
}

static gint
get_keycodes_for_keysym (GfKeybindings *keybindings,
                         gint           keysym,
                         gint         **keycodes)
{
  GArray *retval;
  gint min_keycodes;
  gint max_keycodes;
  gint keysyms_per_keycode;
  KeySym *keymap;
  gint n_keycodes;
  gint keycode;

  retval = g_array_new (FALSE, FALSE, sizeof (gint));

  XDisplayKeycodes (keybindings->xdisplay, &min_keycodes, &max_keycodes);

  keymap = XGetKeyboardMapping (keybindings->xdisplay, min_keycodes,
                                max_keycodes - min_keycodes + 1,
                                &keysyms_per_keycode);

  keycode = min_keycodes;
  while (keycode <= max_keycodes)
    {
      const KeySym *syms;
      gint i;

      syms = keymap + (keycode - min_keycodes) * keysyms_per_keycode;
      i = 0;

      while (i < keysyms_per_keycode)
        {
          if (syms[i] == (uint) keysym)
            g_array_append_val (retval, keycode);

          ++i;
        }

      ++keycode;
    }

  XFree (keymap);

  n_keycodes = retval->len;
  *keycodes = (gint *) g_array_free (retval, n_keycodes == 0 ? TRUE : FALSE);

  return n_keycodes;
}

static void
reload_iso_group_keybindings (GfKeybindings *keybindings)
{
  gint *keycodes;
  gint n_keycodes;
  gint i;

  g_hash_table_remove_all (keybindings->iso_group_keybindings);

  if (keybindings->iso_group == NULL)
    return;

  if (g_strcmp0 (keybindings->iso_group, "toggle") == 0 ||
      g_strcmp0 (keybindings->iso_group, "lalt_toggle") == 0 ||
      g_strcmp0 (keybindings->iso_group, "lwin_toggle") == 0 ||
      g_strcmp0 (keybindings->iso_group, "rwin_toggle") == 0 ||
      g_strcmp0 (keybindings->iso_group, "lshift_toggle") == 0 ||
      g_strcmp0 (keybindings->iso_group, "rshift_toggle") == 0 ||
      g_strcmp0 (keybindings->iso_group, "lctrl_toggle") == 0 ||
      g_strcmp0 (keybindings->iso_group, "rctrl_toggle") == 0 ||
      g_strcmp0 (keybindings->iso_group, "sclk_toggle") == 0 ||
      g_strcmp0 (keybindings->iso_group, "menu_toggle") == 0 ||
      g_strcmp0 (keybindings->iso_group, "caps_toggle") == 0)
    {
      n_keycodes = get_keycodes_for_keysym (keybindings, XK_ISO_Next_Group,
                                            &keycodes);

      for (i = 0; i < n_keycodes; i++)
        {
          Keybinding *keybinding;

          keybinding = iso_next_group_keybinding_new (keycodes[i], 0);
          g_hash_table_insert (keybindings->iso_group_keybindings,
                               GINT_TO_POINTER (i), keybinding);
        }

      g_free (keycodes);
    }
  else if (g_strcmp0 (keybindings->iso_group, "shift_caps_toggle") == 0 ||
           g_strcmp0 (keybindings->iso_group, "shifts_toggle") == 0)
    {
      n_keycodes = get_keycodes_for_keysym (keybindings, XK_ISO_Next_Group,
                                            &keycodes);

      for (i = 0; i < n_keycodes; i++)
        {
          Keybinding *keybinding;

          keybinding = iso_next_group_keybinding_new (keycodes[i], ShiftMask);
          g_hash_table_insert (keybindings->iso_group_keybindings,
                               GINT_TO_POINTER (i), keybinding);
        }

      g_free (keycodes);
    }
  else if (g_strcmp0 (keybindings->iso_group, "alt_caps_toggle") == 0 ||
           g_strcmp0 (keybindings->iso_group, "alt_space_toggle") == 0)
    {
      n_keycodes = get_keycodes_for_keysym (keybindings, XK_ISO_Next_Group,
                                            &keycodes);

      for (i = 0; i < n_keycodes; i++)
        {
          Keybinding *keybinding;

          keybinding = iso_next_group_keybinding_new (keycodes[i], Mod1Mask);
          g_hash_table_insert (keybindings->iso_group_keybindings,
                               GINT_TO_POINTER (i), keybinding);
        }

      g_free (keycodes);
    }
  else if (g_strcmp0 (keybindings->iso_group, "ctrl_shift_toggle") == 0 ||
           g_strcmp0 (keybindings->iso_group, "lctrl_lshift_toggle") == 0 ||
           g_strcmp0 (keybindings->iso_group, "rctrl_rshift_toggle") == 0)
    {
      n_keycodes = get_keycodes_for_keysym (keybindings, XK_ISO_Next_Group,
                                            &keycodes);

      for (i = 0; i < n_keycodes; i++)
        {
          Keybinding *keybinding;

          keybinding = iso_next_group_keybinding_new (keycodes[i], ShiftMask);
          g_hash_table_insert (keybindings->iso_group_keybindings,
                               GINT_TO_POINTER (i), keybinding);

          keybinding = iso_next_group_keybinding_new (keycodes[i], ControlMask);
          g_hash_table_insert (keybindings->iso_group_keybindings,
                               GINT_TO_POINTER (i + n_keycodes), keybinding);
        }

      g_free (keycodes);
    }
  else if (g_strcmp0 (keybindings->iso_group, "ctrl_alt_toggle") == 0)
    {
      n_keycodes = get_keycodes_for_keysym (keybindings, XK_ISO_Next_Group,
                                            &keycodes);

      for (i = 0; i < n_keycodes; i++)
        {
          Keybinding *keybinding;

          keybinding = iso_next_group_keybinding_new (keycodes[i], Mod1Mask);
          g_hash_table_insert (keybindings->iso_group_keybindings,
                               GINT_TO_POINTER (i), keybinding);

          keybinding = iso_next_group_keybinding_new (keycodes[i], ControlMask);
          g_hash_table_insert (keybindings->iso_group_keybindings,
                               GINT_TO_POINTER (i + n_keycodes), keybinding);
        }

      g_free (keycodes);
    }
  else if (g_strcmp0 (keybindings->iso_group, "alt_shift_toggle") == 0 ||
           g_strcmp0 (keybindings->iso_group, "lalt_lshift_toggle") == 0)
    {
      n_keycodes = get_keycodes_for_keysym (keybindings, XK_ISO_Next_Group,
                                            &keycodes);

      for (i = 0; i < n_keycodes; i++)
        {
          Keybinding *keybinding;

          keybinding = iso_next_group_keybinding_new (keycodes[i], Mod1Mask);
          g_hash_table_insert (keybindings->iso_group_keybindings,
                               GINT_TO_POINTER (i), keybinding);

          keybinding = iso_next_group_keybinding_new (keycodes[i], ShiftMask);
          g_hash_table_insert (keybindings->iso_group_keybindings,
                               GINT_TO_POINTER (i + n_keycodes), keybinding);
        }

      g_free (keycodes);
    }
  /* Caps Lock to first layout; Shift+Caps Lock to last layout */
  else if (g_strcmp0 (keybindings->iso_group, "shift_caps_switch") == 0)
    {
      n_keycodes = get_keycodes_for_keysym (keybindings, XK_ISO_First_Group,
                                            &keycodes);

      for (i = 0; i < n_keycodes; i++)
        {
          Keybinding *keybinding;

          keybinding = iso_first_group_keybinding_new (keycodes[i], 0);
          g_hash_table_insert (keybindings->iso_group_keybindings,
                               GINT_TO_POINTER (i), keybinding);
        }

      g_free (keycodes);
      n_keycodes = get_keycodes_for_keysym (keybindings, XK_ISO_Last_Group,
                                            &keycodes);

      for (i = 0; i < n_keycodes; i++)
        {
          Keybinding *keybinding;

          keybinding = iso_last_group_keybinding_new (keycodes[i], ShiftMask);
          g_hash_table_insert (keybindings->iso_group_keybindings,
                               GINT_TO_POINTER (i + n_keycodes), keybinding);
        }

      g_free (keycodes);
    }
}

static void
ungrab_keybindings (GfKeybindings *keybindings)
{
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init (&iter, keybindings->iso_group_keybindings);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      Keybinding *keybinding;

      keybinding = (Keybinding *) value;

      change_keygrab (keybindings, FALSE, keybinding);
    }

  g_hash_table_iter_init (&iter, keybindings->keybindings);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      Keybinding *keybinding;

      keybinding = (Keybinding *) value;

      change_keygrab (keybindings, FALSE, keybinding);
    }
}

static void
reload_modmap (GfKeybindings *keybindings)
{
  guint meta_mask;
  guint super_mask;
  guint hyper_mask;
  guint num_lock_mask;
  guint scroll_lock_mask;

  meta_mask = XkbKeysymToModifiers (keybindings->xdisplay, XK_Meta_L);
  if (meta_mask == 0)
    meta_mask = XkbKeysymToModifiers (keybindings->xdisplay, XK_Meta_R);

  super_mask = XkbKeysymToModifiers (keybindings->xdisplay, XK_Super_L);
  if (super_mask == 0)
    super_mask = XkbKeysymToModifiers (keybindings->xdisplay, XK_Super_R);

  hyper_mask = XkbKeysymToModifiers (keybindings->xdisplay, XK_Hyper_L);
  if (hyper_mask == 0)
    hyper_mask = XkbKeysymToModifiers (keybindings->xdisplay, XK_Hyper_R);

  num_lock_mask = XkbKeysymToModifiers (keybindings->xdisplay, XK_Num_Lock);
  scroll_lock_mask = XkbKeysymToModifiers (keybindings->xdisplay,
                                           XK_Scroll_Lock);

  keybindings->meta_mask = meta_mask;
  keybindings->super_mask = super_mask;
  keybindings->hyper_mask = hyper_mask;
  keybindings->ignored_mask = num_lock_mask | scroll_lock_mask | LockMask;
}

static void
reload_keybindings (GfKeybindings *keybindings)
{
  GHashTableIter iter;
  gpointer value;

  reload_iso_group_keybindings (keybindings);

  g_hash_table_iter_init (&iter, keybindings->keybindings);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      Keybinding *keybinding;
      guint keyval;
      GdkModifierType modifiers;
      guint keycode;
      guint mask;

      keybinding = (Keybinding *) value;

      gtk_accelerator_parse (keybinding->name, &keyval, &modifiers);

      if (is_valid_accelerator (keyval, modifiers))
        {
          keycode = XKeysymToKeycode (keybindings->xdisplay, keyval);

          if (!devirtualize_modifiers (keybindings, modifiers, &mask))
            mask = 0;
        }
      else
        {
          keyval = 0;
          modifiers = 0;
          keycode = 0;
          mask = 0;
        }

      keybinding->keyval = keyval;
      keybinding->modifiers = modifiers;
      keybinding->keycode = keycode;
      keybinding->mask = mask;
    }
}

static void
regrab_keybindings (GfKeybindings *keybindings)
{
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init (&iter, keybindings->iso_group_keybindings);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      Keybinding *keybinding;

      keybinding = (Keybinding *) value;

      change_keygrab (keybindings, TRUE, keybinding);
    }

  g_hash_table_iter_init (&iter, keybindings->keybindings);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      Keybinding *keybinding;

      keybinding = (Keybinding *) value;

      change_keygrab (keybindings, TRUE, keybinding);
    }
}

static gboolean
process_iso_group (GfKeybindings *keybindings,
                   XEvent        *event)
{
  gboolean processed;
  guint state;
  GList *values;
  GList *l;

  if (event->type == KeyRelease)
    return FALSE;

  processed = FALSE;
  state = event->xkey.state & 0xff & ~(keybindings->ignored_mask);
  values = g_hash_table_get_values (keybindings->iso_group_keybindings);

  for (l = values; l; l = l->next)
    {
      Keybinding *keybinding;

      keybinding = (Keybinding *) l->data;

      if (keybinding->keycode == event->xkey.keycode &&
          keybinding->mask == state)
        {
          gboolean freeze;

          g_signal_emit (keybindings,
                         signals[SIGNAL_MODIFIERS_ACCELERATOR_ACTIVATED],
                         0,
                         keybinding->type,
                         &freeze);

          if (!freeze)
            XUngrabKeyboard (keybindings->xdisplay, event->xkey.time);

          processed = TRUE;
          break;
        }
    }

  g_list_free (values);

  return processed;
}

static gboolean
process_event (GfKeybindings *keybindings,
               XEvent        *event)
{
  gboolean processed;
  guint state;
  GList *values;
  GList *l;

  if (event->type == KeyRelease)
    return FALSE;

  processed = FALSE;
  state = event->xkey.state & 0xff & ~(keybindings->ignored_mask);
  values = g_hash_table_get_values (keybindings->keybindings);

  for (l = values; l; l = l->next)
    {
      Keybinding *keybinding;

      keybinding = (Keybinding *) l->data;

      if (keybinding->keycode == event->xkey.keycode &&
          keybinding->mask == state)
        {
          XUngrabKeyboard (keybindings->xdisplay, event->xkey.time);
          g_signal_emit (keybindings, signals[SIGNAL_ACCELERATOR_ACTIVATED],
                         0, keybinding->action, NULL, 0, event->xkey.time);

          processed = TRUE;
          break;
        }
    }

  g_list_free (values);

  return processed;
}

static GdkFilterReturn
filter_func (GdkXEvent *xevent,
             GdkEvent  *event,
             gpointer   user_data)
{
  GfKeybindings *keybindings;
  XEvent *ev;

  keybindings = GF_KEYBINDINGS (user_data);
  ev = (XEvent *) xevent;

  if (ev->type == keybindings->xkb_event_base)
    {
      XkbEvent *xkb_ev;

      xkb_ev = (XkbEvent *) ev;

      switch (xkb_ev->any.xkb_type)
        {
        case XkbNewKeyboardNotify:
        case XkbMapNotify:
          ungrab_keybindings (keybindings);
          reload_modmap (keybindings);
          reload_keybindings (keybindings);
          regrab_keybindings (keybindings);
          break;
        default:
          break;
        }
    }

  if (ev->type != KeyPress && ev->type != KeyRelease)
    return GDK_FILTER_CONTINUE;

  if (process_iso_group (keybindings, ev))
    return GDK_FILTER_REMOVE;

  XAllowEvents (keybindings->xdisplay, AsyncKeyboard, ev->xkey.time);

  if (process_event (keybindings, ev))
    return GDK_FILTER_REMOVE;

  return GDK_FILTER_CONTINUE;
}

static guint
get_next_action (void)
{
  static guint action;

  return ++action;
}

static void
gf_keybindings_dispose (GObject *object)
{
  GfKeybindings *keybindings;

  keybindings = GF_KEYBINDINGS (object);

  if (keybindings->keybindings != NULL)
    {
      g_hash_table_destroy (keybindings->keybindings);
      keybindings->keybindings = NULL;
    }

  if (keybindings->iso_group_keybindings != NULL)
    {
      g_hash_table_destroy (keybindings->iso_group_keybindings);
      keybindings->iso_group_keybindings = NULL;
    }

  G_OBJECT_CLASS (gf_keybindings_parent_class)->dispose (object);
}

static void
gf_keybindings_finalize (GObject *object)
{
  GfKeybindings *keybindings;

  keybindings = GF_KEYBINDINGS (object);

  gdk_window_remove_filter (NULL, filter_func, keybindings);

  g_free (keybindings->iso_group);

  G_OBJECT_CLASS (gf_keybindings_parent_class)->finalize (object);
}

static void
gf_keybindings_class_init (GfKeybindingsClass *keybindings_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (keybindings_class);

  object_class->dispose = gf_keybindings_dispose;
  object_class->finalize = gf_keybindings_finalize;

  /**
   * GfKeybindings::accelerator-activated:
   * @keybindings: the object on which the signal is emitted
   * @action: keybinding action from gf_keybindings_grab
   *
   * The ::accelerator-activated signal will be emitted when a keybinding is
   * activated.
   */
  signals[SIGNAL_ACCELERATOR_ACTIVATED] =
    g_signal_new ("accelerator-activated",
                  G_TYPE_FROM_CLASS (keybindings_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 4, G_TYPE_UINT,
                  G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT);

  /**
   * GfKeybindings::modifiers-accelerator-activated:
   * @keybindings: the object on which the signal is emitted
   *
   * The ::modifiers-accelerator-activated signal will be emitted when a
   * special modifiers-only keybinding is activated.
   */
  signals[SIGNAL_MODIFIERS_ACCELERATOR_ACTIVATED] =
    g_signal_new ("modifiers-accelerator-activated",
                  G_TYPE_FROM_CLASS (keybindings_class), G_SIGNAL_RUN_LAST,
                  0, g_signal_accumulator_first_wins, NULL, NULL,
                  G_TYPE_BOOLEAN, 1, GF_TYPE_KEYBINDING_TYPE);
}

static void
gf_keybindings_init (GfKeybindings *keybindings)
{
  GdkDisplay *display;
  gint xkb_opcode;
  gint xkb_major;
  gint xkb_minor;

  keybindings->keybindings = g_hash_table_new_full (NULL, NULL, NULL,
                                                    keybinding_free);
  keybindings->iso_group_keybindings = g_hash_table_new_full (NULL, NULL, NULL,
                                                              keybinding_free);

  display = gdk_display_get_default ();
  xkb_major = XkbMajorVersion;
  xkb_minor = XkbMinorVersion;

  keybindings->xdisplay = gdk_x11_display_get_xdisplay (display);
  keybindings->xwindow = XDefaultRootWindow (keybindings->xdisplay);

  if (!XkbQueryExtension (keybindings->xdisplay, &xkb_opcode,
                          &keybindings->xkb_event_base,
                          &keybindings->xkb_error_base,
                          &xkb_major, &xkb_minor))
    {
      keybindings->xkb_event_base = -1;

      g_warning ("X server doesn't have the XKB extension, version %d.%d or "
                 "newer", XkbMajorVersion, XkbMinorVersion);
    }
  else
    {
      XkbSelectEvents (keybindings->xdisplay, XkbUseCoreKbd,
                       XkbNewKeyboardNotifyMask | XkbMapNotifyMask,
                       XkbNewKeyboardNotifyMask | XkbMapNotifyMask);
    }

  reload_modmap (keybindings);

  gdk_window_add_filter (NULL, filter_func, keybindings);
}

/**
 * gf_keybindings_new:
 *
 * Creates a new #GfKeybindings.
 *
 * Returns: (transfer full): a newly created #GfKeybindings.
 */
GfKeybindings *
gf_keybindings_new (void)
{
  return g_object_new (GF_TYPE_KEYBINDINGS,
                       NULL);
}

void
gf_keybindings_grab_iso_group (GfKeybindings *keybindings,
                               const gchar   *iso_group)
{
  if (g_strcmp0 (iso_group, keybindings->iso_group) == 0)
    return;

  g_free (keybindings->iso_group);
  keybindings->iso_group = g_strdup (iso_group);

  reload_iso_group_keybindings (keybindings);
}

/**
 * gf_keybindings_grab:
 * @keybindings: a #GfKeybindings
 * @accelerator: a string representing an accelerator
 *
 * Add keybinding.
 *
 * Returns: the keybinding action if the keybinding was added successfully,
 *          otherwise 0.
 */
guint
gf_keybindings_grab (GfKeybindings *keybindings,
                     const gchar   *accelerator)
{
  guint keyval;
  GdkModifierType modifiers;
  guint keycode;
  guint mask;
  guint action;
  gpointer paction;
  Keybinding *keybinding;

  gtk_accelerator_parse (accelerator, &keyval, &modifiers);

  if (!is_valid_accelerator (keyval, modifiers))
    return 0;

  if (keyval == 0)
    return 0;

  keycode = XKeysymToKeycode (keybindings->xdisplay, keyval);

  if (keycode == 0)
    return 0;

  if (!devirtualize_modifiers (keybindings, modifiers, &mask))
    return 0;

  action = get_next_action();
  paction = GUINT_TO_POINTER (action);
  keybinding = keybinding_new (accelerator, keyval, modifiers,
                               keycode, mask, action);

  change_keygrab (keybindings, TRUE, keybinding);
  g_hash_table_insert (keybindings->keybindings, paction, keybinding);

  return action;
}

/**
 * gf_keybindings_ungrab:
 * @keybindings: a #GfKeybindings
 * @action: a keybinding action
 *
 * Remove keybinding.
 *
 * Returns: %TRUE if the keybinding was removed successfully
 */
gboolean
gf_keybindings_ungrab (GfKeybindings *keybindings,
                       guint          action)
{
  gpointer paction;
  gpointer pkeybinding;
  Keybinding *keybinding;

  paction = GUINT_TO_POINTER (action);
  pkeybinding = g_hash_table_lookup (keybindings->keybindings, paction);
  keybinding = (Keybinding *) pkeybinding;

  if (keybinding == NULL)
    return FALSE;

  change_keygrab (keybindings, FALSE, keybinding);
  g_hash_table_remove (keybindings->keybindings, paction);

  return TRUE;
}

guint
gf_keybindings_get_keyval (GfKeybindings *keybindings,
                           guint          action)
{
  Keybinding *keybinding;

  keybinding = get_keybinding (keybindings, action);

  if (keybinding == NULL)
    return 0;

  return keybinding->keyval;
}

GdkModifierType
gf_keybindings_get_modifiers (GfKeybindings *keybindings,
                              guint          action)
{
  Keybinding *keybinding;

  keybinding = get_keybinding (keybindings, action);

  if (keybinding == NULL)
    return 0;

  return keybinding->modifiers;
}
