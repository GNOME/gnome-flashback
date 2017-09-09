/*
 * Copyright (C) 2014 Red Hat
 * Copyright (C) 2017 Alberts Muktupāvels
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
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 *
 * Adapted from mutter:
 * - src/backends/x11/meta-backend-x11.c
 */

#include "config.h"

#include <gio/gio.h>

#include "gf-backend-x11-private.h"

typedef struct
{
  GSource    source;
  GPollFD    event_poll_fd;

  GfBackend *backend;
} XEventSource;

typedef struct
{
  Display *xdisplay;

  GSource *source;
} GfBackendX11Private;

static void
initable_iface_init (GInitableIface *initable_iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GfBackendX11, gf_backend_x11, GF_TYPE_BACKEND,
                                  G_ADD_PRIVATE (GfBackendX11)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         initable_iface_init))

static void
handle_host_xevent (GfBackend *backend,
                    XEvent    *event)
{
  GfBackendX11 *x11;
  GfBackendX11Private *priv;

  x11 = GF_BACKEND_X11 (backend);
  priv = gf_backend_x11_get_instance_private (x11);

  XGetEventData (priv->xdisplay, &event->xcookie);

  GF_BACKEND_X11_GET_CLASS (x11)->handle_host_xevent (x11, event);

  XFreeEventData (priv->xdisplay, &event->xcookie);
}

static gboolean
x_event_source_prepare (GSource *source,
                        int     *timeout)
{
  XEventSource *x_source;
  GfBackendX11 *x11;
  GfBackendX11Private *priv;

  x_source = (XEventSource *) source;
  x11 = GF_BACKEND_X11 (x_source->backend);
  priv = gf_backend_x11_get_instance_private (x11);

  *timeout = -1;

  return XPending (priv->xdisplay);
}

static gboolean
x_event_source_check (GSource *source)
{
  XEventSource *x_source;
  GfBackendX11 *x11;
  GfBackendX11Private *priv;

  x_source = (XEventSource *) source;
  x11 = GF_BACKEND_X11 (x_source->backend);
  priv = gf_backend_x11_get_instance_private (x11);

  return XPending (priv->xdisplay);
}

static gboolean
x_event_source_dispatch (GSource     *source,
                         GSourceFunc  callback,
                         gpointer     user_data)
{
  XEventSource *x_source;
  GfBackendX11 *x11;
  GfBackendX11Private *priv;

  x_source = (XEventSource *) source;
  x11 = GF_BACKEND_X11 (x_source->backend);
  priv = gf_backend_x11_get_instance_private (x11);

  while (XPending (priv->xdisplay))
    {
      XEvent event;

      XNextEvent (priv->xdisplay, &event);

      handle_host_xevent (x_source->backend, &event);
    }

  return TRUE;
}

static GSourceFuncs x_event_funcs =
  {
    x_event_source_prepare,
    x_event_source_check,
    x_event_source_dispatch,
  };

static GSource *
x_event_source_new (GfBackend *backend)
{
  GfBackendX11 *x11;
  GfBackendX11Private *priv;
  GSource *source;
  XEventSource *x_source;

  x11 = GF_BACKEND_X11 (backend);
  priv = gf_backend_x11_get_instance_private (x11);

  source = g_source_new (&x_event_funcs, sizeof (XEventSource));

  x_source = (XEventSource *) source;
  x_source->event_poll_fd.fd = ConnectionNumber (priv->xdisplay);
  x_source->event_poll_fd.events = G_IO_IN;
  x_source->backend = backend;

  g_source_add_poll (source, &x_source->event_poll_fd);
  g_source_attach (source, NULL);

  return source;
}

static gboolean
gf_backend_x11_initable_init (GInitable     *initable,
                              GCancellable  *cancellable,
                              GError       **error)
{
  GfBackendX11 *x11;
  GfBackendX11Private *priv;
  GInitableIface *parent_iface;
  const gchar *display;

  x11 = GF_BACKEND_X11 (initable);
  priv = gf_backend_x11_get_instance_private (x11);
  parent_iface = g_type_interface_peek_parent (G_INITABLE_GET_IFACE (x11));

  display = g_getenv ("DISPLAY");
  if (!display)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Unable to open display, DISPLAY not set");

      return FALSE;
    }

  priv->xdisplay = XOpenDisplay (display);
  if (!priv->xdisplay)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Unable to open display '%s'", display);

      return FALSE;
    }

  return parent_iface->init (initable, cancellable, error);
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = gf_backend_x11_initable_init;
}

static void
gf_backend_x11_finalize (GObject *object)
{
  GfBackendX11 *x11;
  GfBackendX11Private *priv;

  x11 = GF_BACKEND_X11 (object);
  priv = gf_backend_x11_get_instance_private (x11);

  if (priv->source != NULL)
    {
      g_source_unref (priv->source);
      priv->source = NULL;
    }

  if (priv->xdisplay != NULL)
    {
      XCloseDisplay (priv->xdisplay);
      priv->xdisplay = NULL;
    }

  G_OBJECT_CLASS (gf_backend_x11_parent_class)->finalize (object);
}

static void
gf_backend_x11_post_init (GfBackend *backend)
{
  GfBackendX11 *x11;
  GfBackendX11Private *priv;

  x11 = GF_BACKEND_X11 (backend);
  priv = gf_backend_x11_get_instance_private (x11);

  priv->source = x_event_source_new (backend);

  GF_BACKEND_CLASS (gf_backend_x11_parent_class)->post_init (backend);
}

static void
gf_backend_x11_class_init (GfBackendX11Class *x11_class)
{
  GObjectClass *object_class;
  GfBackendClass *backend_class;

  object_class = G_OBJECT_CLASS (x11_class);
  backend_class = GF_BACKEND_CLASS (x11_class);

  object_class->finalize = gf_backend_x11_finalize;

  backend_class->post_init = gf_backend_x11_post_init;
}

static void
gf_backend_x11_init (GfBackendX11 *x11)
{
}

Display *
gf_backend_x11_get_xdisplay (GfBackendX11 *x11)
{
  GfBackendX11Private *priv;

  priv = gf_backend_x11_get_instance_private (x11);

  return priv->xdisplay;
}
