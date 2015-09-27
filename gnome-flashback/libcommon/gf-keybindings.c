/*
 * Copyright (C) 2014 - 2015 Alberts Muktupāvels
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

#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include "gf-keybindings.h"

struct _GfKeybindings
{
  GObject     parent;

  GHashTable *table;

  Display    *xdisplay;
  Window      xwindow;

  guint       meta_mask;
  guint       super_mask;
  guint       hyper_mask;

  guint       ignored_mask;
};

typedef struct
{
  gchar           *name;

  guint            keyval;
  GdkModifierType  modifiers;

  guint            keycode;
  guint            mask;

  guint            action;
} Keybinding;

enum
{
  SIGNAL_ACCELERATOR_ACTIVATED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfKeybindings, gf_keybindings, G_TYPE_OBJECT)

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

  keybinding->name = g_strdup (name);

  keybinding->keyval = keyval;
  keybinding->modifiers = modifiers;

  keybinding->keycode = keycode;
  keybinding->mask = mask;

  keybinding->action = action;

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
  pkeybinding = g_hash_table_lookup (keybindings->table, paction);
  keybinding = (Keybinding *) pkeybinding;

  return keybinding;
}

static GdkFilterReturn
filter_func (GdkXEvent *xevent,
             GdkEvent  *event,
             gpointer   user_data)
{
  GfKeybindings *keybindings;
  XEvent *ev;
  GList *values;
  GList *l;

  keybindings = GF_KEYBINDINGS (user_data);
  ev = (XEvent *) xevent;

  XAllowEvents (keybindings->xdisplay, AsyncKeyboard, ev->xkey.time);

  if (ev->type != KeyPress)
    return GDK_FILTER_CONTINUE;

  values = g_hash_table_get_values (keybindings->table);

  for (l = values; l; l = l->next)
    {
      Keybinding *keybinding;
      guint state;

      keybinding = (Keybinding *) l->data;
      state = ev->xkey.state & 0xff & ~(keybindings->ignored_mask);

      if (keybinding->keycode == ev->xkey.keycode &&
          keybinding->mask == state)
        {
          XUngrabKeyboard (keybindings->xdisplay, ev->xkey.time);
          g_signal_emit (keybindings, signals[SIGNAL_ACCELERATOR_ACTIVATED],
                         0, keybinding->action);

          break;
        }
    }

  g_list_free (values);

  return GDK_FILTER_CONTINUE;
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

  ignored_mask = 0;
  keycode = keybinding->keycode;
  mask = keybinding->mask;

  while (ignored_mask <= keybindings->ignored_mask)
    {
      if (ignored_mask & ~(keybindings->ignored_mask))
        {
          ++ignored_mask;
          continue;
        }

      gdk_error_trap_push ();

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

      error_code = gdk_error_trap_pop ();
      if (error_code != 0)
        {
          g_debug ("Failed to grab/ ungrab key. Error code - %d", error_code);
        }

      ++ignored_mask;
    }
}

static guint
get_next_action (void)
{
  static guint action;

  return ++action;
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
gf_keybindings_dispose (GObject *object)
{
  GfKeybindings *keybindings;

  keybindings = GF_KEYBINDINGS (object);

  if (keybindings->table != NULL)
    {
      g_hash_table_destroy (keybindings->table);
      keybindings->table = NULL;
    }

  G_OBJECT_CLASS (gf_keybindings_parent_class)->dispose (object);
}

static void
gf_keybindings_finalize (GObject *object)
{
  GfKeybindings *keybindings;

  keybindings = GF_KEYBINDINGS (object);

  gdk_window_remove_filter (NULL, filter_func, keybindings);

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
   * The ::accelerator-activated signal is emitted each time when keybinding
   * is activated by user.
   */
  signals[SIGNAL_ACCELERATOR_ACTIVATED] =
    g_signal_new ("accelerator-activated",
                  G_TYPE_FROM_CLASS (keybindings_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void
gf_keybindings_init (GfKeybindings *keybindings)
{
  GdkDisplay *display;
  guint meta_mask;
  guint super_mask;
  guint hyper_mask;
  guint num_lock_mask;
  guint scroll_lock_mask;

  keybindings->table = g_hash_table_new_full (NULL, NULL, NULL,
                                              keybinding_free);

  display = gdk_display_get_default ();
  keybindings->xdisplay = gdk_x11_display_get_xdisplay (display);
  keybindings->xwindow = XDefaultRootWindow (keybindings->xdisplay);

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
  return g_object_new (GF_TYPE_KEYBINDINGS, NULL);
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

  if (!gtk_accelerator_valid (keyval, modifiers))
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
  g_hash_table_insert (keybindings->table, paction, keybinding);

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
  pkeybinding = g_hash_table_lookup (keybindings->table, paction);
  keybinding = (Keybinding *) pkeybinding;

  if (keybinding == NULL)
    return FALSE;

  change_keygrab (keybindings, FALSE, keybinding);
  g_hash_table_remove (keybindings->table, paction);

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
