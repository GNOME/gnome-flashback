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

struct _FlashbackKeyBindingsPrivate {
	GHashTable *table;

	Display    *xdisplay;
	Window      xwindow;

	guint       ignored_modifier_mask;
};

enum {
	BINDING_ACTIVATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
	const gchar *name;
	guint        action;
	guint        keyval;
	guint        keycode;
	guint        modifiers;
} KeyBinding;

static guint MetaMask = 0;
static guint SuperMask = 0;
static guint HyperMask = 0;
static guint NumLockMask = 0;
static guint ScrollLockMask = 0;

G_DEFINE_TYPE_WITH_PRIVATE (FlashbackKeyBindings, flashback_key_bindings, G_TYPE_OBJECT)

static gboolean
devirtualize_modifiers (GdkModifierType  modifiers,
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
get_real_modifiers (GdkModifierType  modifiers,
                    guint           *mask)
{
	gboolean devirtualized;

	devirtualized = TRUE;
	*mask = 0;

	devirtualized &= devirtualize_modifiers (modifiers, GDK_SHIFT_MASK, ShiftMask, mask);
	devirtualized &= devirtualize_modifiers (modifiers, GDK_CONTROL_MASK, ControlMask, mask);
	devirtualized &= devirtualize_modifiers (modifiers, GDK_MOD1_MASK, Mod1Mask, mask);
	devirtualized &= devirtualize_modifiers (modifiers, GDK_META_MASK, MetaMask, mask);
	devirtualized &= devirtualize_modifiers (modifiers, GDK_HYPER_MASK, HyperMask, mask);
	devirtualized &= devirtualize_modifiers (modifiers, GDK_SUPER_MASK, SuperMask, mask);
	devirtualized &= devirtualize_modifiers (modifiers, GDK_MOD2_MASK, Mod2Mask, mask);
	devirtualized &= devirtualize_modifiers (modifiers, GDK_MOD3_MASK, Mod3Mask, mask);
	devirtualized &= devirtualize_modifiers (modifiers, GDK_MOD4_MASK, Mod4Mask, mask);
	devirtualized &= devirtualize_modifiers (modifiers, GDK_MOD5_MASK, Mod5Mask, mask);

	return devirtualized;
}

static GVariant *
build_parameters (guint device_id,
                  guint timestamp,
                  guint action_mode)
{
  GVariantBuilder *builder;
  GVariant *parameters;

  builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (builder, "{sv}", "device-id", g_variant_new_uint32 (device_id));
  g_variant_builder_add (builder, "{sv}", "timestamp", g_variant_new_uint32 (timestamp));
  g_variant_builder_add (builder, "{sv}", "action-mode", g_variant_new_uint32 (action_mode));

  parameters = g_variant_new ("a{sv}", builder);
  g_variant_builder_unref (builder);

  return parameters;
}

static GdkFilterReturn
filter_func (GdkXEvent *xevent,
             GdkEvent  *event,
             gpointer   user_data)
{
  FlashbackKeyBindings *bindings;
  XEvent *ev;

  bindings = FLASHBACK_KEY_BINDINGS (user_data);
  ev = xevent;

  XAllowEvents (bindings->priv->xdisplay, AsyncKeyboard, ev->xkey.time);

  if (ev->type == KeyPress)
    {
      GList *values, *l;

      values = g_hash_table_get_values (bindings->priv->table);

      for (l = values; l; l = l->next)
        {
          KeyBinding *binding = l->data;

          if (binding->keycode == ev->xkey.keycode &&
              binding->modifiers == (ev->xkey.state & 0xff & ~(bindings->priv->ignored_modifier_mask)))
            {
              GVariant *parameters;

              parameters = build_parameters (0, 0, 0);

              XUngrabKeyboard (bindings->priv->xdisplay, ev->xkey.time);
              g_signal_emit (bindings, signals[BINDING_ACTIVATED], 0,
                             binding->action, parameters);

              break;
            }
        }

      g_list_free (values);
    }

  return GDK_FILTER_CONTINUE;
}

static void
flashback_key_bindings_change_keygrab (FlashbackKeyBindings *bindings,
                                       gboolean              grab,
                                       gint                  keyval,
                                       guint                 keycode,
                                       guint                 modifiers)
{
	guint ignored_mask;

	gdk_error_trap_push ();

	ignored_mask = 0;
	while (ignored_mask <= bindings->priv->ignored_modifier_mask) {
		if (ignored_mask & ~(bindings->priv->ignored_modifier_mask)) {
			++ignored_mask;
			continue;
		}

		if (grab)
			XGrabKey (bindings->priv->xdisplay, keycode,
			          modifiers | ignored_mask,
			          bindings->priv->xwindow,
			          True,
			          GrabModeAsync, GrabModeSync);
		else
			XUngrabKey (bindings->priv->xdisplay, keycode,
			            modifiers | ignored_mask,
			            bindings->priv->xwindow);

		++ignored_mask;
	}

	gdk_error_trap_pop_ignored ();
}

static void
flashback_key_bindings_finalize (GObject *object)
{
	FlashbackKeyBindings *bindings;

	bindings = FLASHBACK_KEY_BINDINGS (object);

	gdk_window_remove_filter (NULL, filter_func, bindings);

	if (bindings->priv->table) {
		g_hash_table_destroy (bindings->priv->table);
		bindings->priv->table = NULL;
	}

	G_OBJECT_CLASS (flashback_key_bindings_parent_class)->finalize (object);
}

static void
flashback_key_bindings_init (FlashbackKeyBindings *bindings)
{
	FlashbackKeyBindingsPrivate *priv;

	bindings->priv = flashback_key_bindings_get_instance_private (bindings);
	priv = bindings->priv;

	priv->xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	priv->xwindow = XDefaultRootWindow (priv->xdisplay);
	priv->table = g_hash_table_new_full (NULL, NULL, NULL, g_free);

	MetaMask = XkbKeysymToModifiers (priv->xdisplay, XK_Meta_L);
	if (MetaMask == 0)
		MetaMask = XkbKeysymToModifiers (priv->xdisplay, XK_Meta_R);

	SuperMask = XkbKeysymToModifiers (priv->xdisplay, XK_Super_L);
	if (SuperMask == 0)
		SuperMask = XkbKeysymToModifiers (priv->xdisplay, XK_Super_R);

	HyperMask = XkbKeysymToModifiers (priv->xdisplay, XK_Hyper_L);
	if (HyperMask == 0)
		HyperMask = XkbKeysymToModifiers (priv->xdisplay, XK_Hyper_R);

	NumLockMask = XkbKeysymToModifiers (priv->xdisplay, XK_Num_Lock);
	ScrollLockMask = XkbKeysymToModifiers (priv->xdisplay, XK_Scroll_Lock);

	priv->ignored_modifier_mask = NumLockMask | ScrollLockMask | LockMask;

	gdk_window_add_filter (NULL, filter_func, bindings);
}

static void
flashback_key_bindings_class_init (FlashbackKeyBindingsClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = flashback_key_bindings_finalize;

	signals[BINDING_ACTIVATED] =
		g_signal_new ("binding-activated",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (FlashbackKeyBindingsClass, binding_activated),
		              NULL, NULL, NULL,
		              G_TYPE_NONE,
		              2,
		              G_TYPE_UINT,
		              G_TYPE_VARIANT);
}

FlashbackKeyBindings *
flashback_key_bindings_new (void)
{
	return g_object_new (FLASHBACK_TYPE_KEY_BINDINGS, NULL);
}

guint
flashback_key_bindings_grab (FlashbackKeyBindings *bindings,
                             const gchar          *accelerator)
{
	KeyBinding *binding;
	guint keyval;
	GdkModifierType modifiers;
	guint real_modifiers;
	guint keycode;
	static guint next_action = 0;

	gtk_accelerator_parse (accelerator, &keyval, &modifiers);
	if (!gtk_accelerator_valid (keyval, modifiers)) {
		return 0;
	}

	if (keyval == 0)
		return 0;

	keycode = XKeysymToKeycode (bindings->priv->xdisplay, keyval);
	if (keycode == 0)
		return 0;

	if (!get_real_modifiers (modifiers, &real_modifiers))
		return 0;

	flashback_key_bindings_change_keygrab (bindings,
	                                       TRUE,
	                                       keyval,
	                                       keycode,
	                                       real_modifiers);

	binding = g_new0 (KeyBinding, 1);

	binding->name = accelerator;
	binding->action = ++next_action;
	binding->keyval = keyval;
	binding->keycode = keycode;
	binding->modifiers = real_modifiers;

	g_hash_table_insert (bindings->priv->table, GUINT_TO_POINTER (binding->action), binding);

	return binding->action;
}

gboolean
flashback_key_bindings_ungrab (FlashbackKeyBindings *bindings,
                               guint                 action)
{
	KeyBinding *binding;

	binding = (KeyBinding *) g_hash_table_lookup (bindings->priv->table,
	                                              GUINT_TO_POINTER (action));

	if (binding == NULL)
		return FALSE;

	flashback_key_bindings_change_keygrab (bindings,
	                                       FALSE,
	                                       binding->keyval,
	                                       binding->keycode,
	                                       binding->modifiers);

	g_hash_table_remove (bindings->priv->table,
	                     GUINT_TO_POINTER (action));

	return TRUE;
}
