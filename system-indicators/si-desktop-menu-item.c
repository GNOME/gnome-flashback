/*
 * Copyright (C) 2019 Alberts Muktupāvels
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
#include "si-desktop-menu-item.h"

#include <glib/gi18n-lib.h>
#include <gio/gdesktopappinfo.h>

struct _SiDesktopMenuItem
{
  GtkMenuItem      parent;

  char            *desktop_id;
  GDesktopAppInfo *app_info;
};

enum
{
  PROP_0,

  PROP_DESKTOP_ID,

  LAST_PROP
};

static GParamSpec *item_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (SiDesktopMenuItem, si_desktop_menu_item, GTK_TYPE_MENU_ITEM)

static void
response_cb (GtkDialog         *dialog,
             gint               response_id,
             SiDesktopMenuItem *self)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
show_error_message (SiDesktopMenuItem *self,
                    const char        *message)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_USE_HEADER_BAR,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s",
                                   message);

  g_signal_connect (dialog, "response", G_CALLBACK (response_cb), self);
  gtk_widget_show (dialog);
}

static void
activate_cb (SiDesktopMenuItem *self,
             gpointer           user_data)
{
  char *message;
  GdkDisplay *display;
  GdkAppLaunchContext *context;
  GError *error;
  const char *label;

  if (self->app_info == NULL)
    {
      message = g_strdup_printf (_("Desktop file “%s” is missing!"),
                                 self->desktop_id);

      show_error_message (self, message);
      g_free (message);
      return;
    }

  display = gdk_display_get_default ();
  context = gdk_display_get_app_launch_context (display);

  error = NULL;
  g_app_info_launch (G_APP_INFO (self->app_info),
                     NULL,
                     G_APP_LAUNCH_CONTEXT (context),
                     &error);

  g_object_unref (context);

  if (error == NULL)
    return;

  label = gtk_menu_item_get_label (GTK_MENU_ITEM (self));
  message = g_strdup_printf (_("Failed to start “%s”: %s"), label, error->message);
  g_error_free (error);

  show_error_message (self, message);
  g_free (message);
}

static void
si_desktop_menu_item_constructed (GObject *object)
{
  SiDesktopMenuItem *self;

  self = SI_DESKTOP_MENU_ITEM (object);

  G_OBJECT_CLASS (si_desktop_menu_item_parent_class)->constructed (object);

  self->app_info = g_desktop_app_info_new (self->desktop_id);
}

static void
si_desktop_menu_item_dispose (GObject *object)
{
  SiDesktopMenuItem *self;

  self = SI_DESKTOP_MENU_ITEM (object);

  g_clear_object (&self->app_info);

  G_OBJECT_CLASS (si_desktop_menu_item_parent_class)->dispose (object);
}

static void
si_desktop_menu_item_finalize (GObject *object)
{
  SiDesktopMenuItem *self;

  self = SI_DESKTOP_MENU_ITEM (object);

  g_clear_pointer (&self->desktop_id, g_free);

  G_OBJECT_CLASS (si_desktop_menu_item_parent_class)->finalize (object);
}

static void
si_desktop_menu_item_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  SiDesktopMenuItem *self;

  self = SI_DESKTOP_MENU_ITEM (object);

  switch (property_id)
    {
      case PROP_DESKTOP_ID:
        g_assert (self->desktop_id == NULL);
        self->desktop_id = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  item_properties[PROP_DESKTOP_ID] =
    g_param_spec_string ("desktop-id",
                         "desktop-id",
                         "desktop-id",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, item_properties);
}

static void
si_desktop_menu_item_class_init (SiDesktopMenuItemClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = si_desktop_menu_item_constructed;
  object_class->dispose = si_desktop_menu_item_dispose;
  object_class->finalize = si_desktop_menu_item_finalize;
  object_class->set_property = si_desktop_menu_item_set_property;

  install_properties (object_class);
}

static void
si_desktop_menu_item_init (SiDesktopMenuItem *self)
{
  g_signal_connect (self, "activate", G_CALLBACK (activate_cb), NULL);
}

GtkWidget *
si_desktop_menu_item_new (const char *label,
                          const char *desktop_id)
{
  return g_object_new (SI_TYPE_DESKTOP_MENU_ITEM,
                       "desktop-id", desktop_id,
                       "label", label,
                       NULL);
}
