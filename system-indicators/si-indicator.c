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
#include "si-indicator.h"

#include <libgnome-panel/gp-image-menu-item.h>
#include <libgnome-panel/gp-utils.h>

typedef struct
{
  GpApplet  *applet;

  GtkWidget *menu_item;

  GtkWidget *image;

  char      *filename;
} SiIndicatorPrivate;

enum
{
  PROP_0,

  PROP_APPLET,

  LAST_PROP
};

static GParamSpec *indicator_properties[LAST_PROP] = { NULL };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (SiIndicator, si_indicator, G_TYPE_OBJECT)

static cairo_surface_t *
surface_from_pixbuf (GdkPixbuf *pixbuf,
                     int        scale)
{
  int width;
  int height;
  cairo_surface_t *surface;
  cairo_t *cr;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);

  cairo_surface_set_device_scale (surface, scale, scale);

  return surface;
}

static void
update_icon (SiIndicator *self)
{
  SiIndicatorPrivate *priv;
  guint icon_size;

  priv = si_indicator_get_instance_private (self);
  icon_size = gp_applet_get_panel_icon_size (priv->applet);

  if (priv->filename != NULL)
    {
      int scale;
      GError *error;
      GdkPixbuf *pixbuf;
      cairo_surface_t *surface;

      scale = gtk_widget_get_scale_factor (priv->image);
      icon_size *= scale;

      error = NULL;
      pixbuf = gdk_pixbuf_new_from_file_at_size (priv->filename,
                                                 icon_size,
                                                 icon_size,
                                                 &error);

      if (error != NULL)
        {
          g_warning ("%s", error->message);
          g_error_free (error);

          si_indicator_set_icon_name (self, "image-missing");

          return;
        }

      surface = surface_from_pixbuf (pixbuf, scale);
      g_object_unref (pixbuf);

      gtk_image_set_from_surface (GTK_IMAGE (priv->image), surface);
      cairo_surface_destroy (surface);
    }
  else
    {
      gtk_image_set_pixel_size (GTK_IMAGE (priv->image), icon_size);
    }
}

static void
panel_icon_size_cb (GpApplet    *applet,
                    GParamSpec  *pspec,
                    SiIndicator *self)
{
  update_icon (self);
}

static void
scale_factor_cb (GtkImage    *image,
                 GParamSpec  *pspec,
                 SiIndicator *self)
{
  update_icon (self);
}

static void
si_indicator_constructed (GObject *object)
{
  SiIndicator *self;
  SiIndicatorPrivate *priv;

  self = SI_INDICATOR (object);
  priv = si_indicator_get_instance_private (self);

  G_OBJECT_CLASS (si_indicator_parent_class)->constructed (object);

  g_signal_connect_object (priv->applet,
                           "notify::panel-icon-size",
                           G_CALLBACK (panel_icon_size_cb),
                           self,
                           0);
}

static void
si_indicator_dispose (GObject *object)
{
  SiIndicator *self;
  SiIndicatorPrivate *priv;

  self = SI_INDICATOR (object);
  priv = si_indicator_get_instance_private (self);

  g_clear_pointer (&priv->menu_item, gtk_widget_destroy);

  G_OBJECT_CLASS (si_indicator_parent_class)->dispose (object);
}

static void
si_indicator_finalize (GObject *object)
{
  SiIndicator *self;
  SiIndicatorPrivate *priv;

  self = SI_INDICATOR (object);
  priv = si_indicator_get_instance_private (self);

  g_clear_pointer (&priv->filename, g_free);

  G_OBJECT_CLASS (si_indicator_parent_class)->finalize (object);
}

static void
si_indicator_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  SiIndicator *self;
  SiIndicatorPrivate *priv;

  self = SI_INDICATOR (object);
  priv = si_indicator_get_instance_private (self);

  switch (property_id)
    {
      case PROP_APPLET:
        g_assert (priv->applet == NULL);
        priv->applet = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  indicator_properties[PROP_APPLET] =
    g_param_spec_object ("applet",
                         "applet",
                         "applet",
                         GP_TYPE_APPLET,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     indicator_properties);
}

static void
si_indicator_class_init (SiIndicatorClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = si_indicator_constructed;
  object_class->dispose = si_indicator_dispose;
  object_class->finalize = si_indicator_finalize;
  object_class->set_property = si_indicator_set_property;

  install_properties (object_class);
}

static void
si_indicator_init (SiIndicator *self)
{
  SiIndicatorPrivate *priv;

  priv = si_indicator_get_instance_private (self);

  priv->menu_item = gp_image_menu_item_new ();
  g_object_ref_sink (priv->menu_item);

  priv->image = gtk_image_new ();
  gp_image_menu_item_set_image (GP_IMAGE_MENU_ITEM (priv->menu_item),
                                priv->image);

  g_signal_connect (priv->image,
                    "notify::scale-factor",
                    G_CALLBACK (scale_factor_cb),
                    self);

  gp_add_text_color_class (priv->image);
}

GpApplet *
si_indicator_get_applet (SiIndicator *self)
{
  SiIndicatorPrivate *priv;

  priv = si_indicator_get_instance_private (self);

  return priv->applet;
}

GtkWidget *
si_indicator_get_menu_item (SiIndicator *self)
{
  SiIndicatorPrivate *priv;

  priv = si_indicator_get_instance_private (self);

  return priv->menu_item;
}

void
si_indicator_set_icon_name (SiIndicator *self,
                            const char  *icon_name)
{
  SiIndicatorPrivate *priv;

  priv = si_indicator_get_instance_private (self);

  g_clear_pointer (&priv->filename, g_free);
  gtk_image_clear (GTK_IMAGE (priv->image));

  if (icon_name == NULL)
    {
      gtk_widget_hide (priv->image);
      return;
    }

  gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
                                icon_name,
                                GTK_ICON_SIZE_MENU);

  update_icon (self);

  gtk_widget_show (priv->image);
}

void
si_indicator_set_icon_filename (SiIndicator *self,
                                const char  *filename)
{
  SiIndicatorPrivate *priv;

  priv = si_indicator_get_instance_private (self);

  g_clear_pointer (&priv->filename, g_free);
  gtk_image_clear (GTK_IMAGE (priv->image));

  if (filename == NULL)
    {
      gtk_widget_hide (priv->image);
      return;
    }

  priv->filename = g_strdup (filename);
  update_icon (self);

  gtk_widget_show (priv->image);
}

void
si_indicator_set_icon (SiIndicator *self,
                       GIcon       *icon)
{
  SiIndicatorPrivate *priv;

  priv = si_indicator_get_instance_private (self);

  g_clear_pointer (&priv->filename, g_free);
  gtk_image_clear (GTK_IMAGE (priv->image));

  if (icon == NULL)
    {
      gtk_widget_hide (priv->image);
      return;
    }

  gtk_image_set_from_gicon (GTK_IMAGE (priv->image), icon, GTK_ICON_SIZE_MENU);
  update_icon (self);

  gtk_widget_show (priv->image);
}
