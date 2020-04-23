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
#include "gf-user-image.h"

#include "dbus/gf-accounts-gen.h"
#include "dbus/gf-accounts-user-gen.h"

struct _GfUserImage
{
  GtkImage           parent;

  GCancellable      *cancellable;

  GfAccountsGen     *accounts;
  GfAccountsUserGen *user;
};

G_DEFINE_TYPE (GfUserImage, gf_user_image, GTK_TYPE_IMAGE)

static void
rounded_rect_path (cairo_t           *cr,
                   cairo_rectangle_t *rect,
                   double             radius)
{
  double x;
  double y;
  double w;
  double h;
  double degrees;

  x = rect->x;
  y = rect->y;
  w = rect->width;
  h = rect->height;
  degrees = G_PI / 180.0;

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + w - radius, y + radius, radius, -90 * degrees, 0 * degrees);
  cairo_arc (cr, x + w - radius, y + h - radius, radius, 0 * degrees, 90 * degrees);
  cairo_arc (cr, x + radius, y + h - radius, radius, 90 * degrees, 180 * degrees);
  cairo_arc (cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);
}

static cairo_surface_t *
surface_from_pixbuf (GdkPixbuf *pixbuf,
                     int        scale)
{
  int width;
  int height;
  cairo_rectangle_t rect;
  cairo_surface_t *surface;
  cairo_t *cr;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  rect = (cairo_rectangle_t) { 0, 0, width, height };
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);

  rounded_rect_path (cr, &rect, 8 * scale);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_fill (cr);

  cairo_destroy (cr);

  cairo_surface_set_device_scale (surface, scale, scale);

  return surface;
}

static void
load_icon (GfUserImage *self,
           const char  *icon_file)
{
  int scale;
  int icon_size;
  GError *error;
  GdkPixbuf *pixbuf;
  cairo_surface_t *surface;

  scale = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  icon_size = 64 * scale;

  error = NULL;
  pixbuf = gdk_pixbuf_new_from_file_at_size (icon_file,
                                             icon_size,
                                             icon_size,
                                             &error);

  if (error != NULL)
    {
      gtk_widget_hide (GTK_WIDGET (self));

      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  surface = surface_from_pixbuf (pixbuf, scale);
  g_object_unref (pixbuf);

  gtk_image_set_from_surface (GTK_IMAGE (self), surface);
  cairo_surface_destroy (surface);

  gtk_widget_show (GTK_WIDGET (self));
}

static void
user_ready_cb (GObject      *object,
               GAsyncResult *res,
               gpointer      user_data)
{
  GError *error;
  GfAccountsUserGen *user;
  GfUserImage *self;
  const char *icon_file;

  error = NULL;
  user = gf_accounts_user_gen_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_USER_IMAGE (user_data);
  self->user = user;

  icon_file = gf_accounts_user_gen_get_icon_file (user);

  if (icon_file == NULL)
    {
      g_debug ("User does not have icon file!");
      gtk_widget_hide (GTK_WIDGET (self));
      return;
    }

  if (!g_file_test (icon_file, G_FILE_TEST_EXISTS))
    {
      g_debug ("User icon '%s' does not exist!", icon_file);
      gtk_widget_hide (GTK_WIDGET (self));
      return;
    }

  load_icon (self, icon_file);
}

static void
find_user_by_name_cb (GObject      *object,
                      GAsyncResult *res,
                      gpointer      user_data)

{
  GError *error;
  char *user;
  GfUserImage *self;

  error = NULL;
  user = NULL;

  gf_accounts_gen_call_find_user_by_name_finish (GF_ACCOUNTS_GEN (object),
                                                 &user,
                                                 res,
                                                 &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_USER_IMAGE (user_data);

  gf_accounts_user_gen_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          "org.freedesktop.Accounts",
                                          user,
                                          self->cancellable,
                                          user_ready_cb,
                                          self);

  g_free (user);
}

static void
accounts_ready_cb (GObject      *object,
                   GAsyncResult *res,
                   gpointer      user_data)

{
  GError *error;
  GfAccountsGen *accounts;
  GfUserImage *self;

  error = NULL;
  accounts = gf_accounts_gen_proxy_new_for_bus_finish (res, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);

      g_error_free (error);
      return;
    }

  self = GF_USER_IMAGE (user_data);
  self->accounts = accounts;

  gf_accounts_gen_call_find_user_by_name (accounts,
                                          g_get_user_name (),
                                          self->cancellable,
                                          find_user_by_name_cb,
                                          self);
}

static void
gf_user_image_dispose (GObject *object)
{
  GfUserImage *self;

  self = GF_USER_IMAGE (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->accounts);
  g_clear_object (&self->user);

  G_OBJECT_CLASS (gf_user_image_parent_class)->dispose (object);
}

static void
gf_user_image_class_init (GfUserImageClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_user_image_dispose;
}

static void
gf_user_image_init (GfUserImage *self)
{
  self->cancellable = g_cancellable_new ();

  gf_accounts_gen_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                     G_DBUS_PROXY_FLAGS_NONE,
                                     "org.freedesktop.Accounts",
                                     "/org/freedesktop/Accounts",
                                     self->cancellable,
                                     accounts_ready_cb,
                                     self);
}

GtkWidget *
gf_user_image_new (void)
{
  return g_object_new (GF_TYPE_USER_IMAGE,
                       NULL);
}
