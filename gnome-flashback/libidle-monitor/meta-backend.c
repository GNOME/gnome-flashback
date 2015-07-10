/*
 * Copyright (C) 2014 Red Hat
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
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include <gdk/gdkx.h>

#include "config.h"
#include "meta-backend.h"
#include "meta-idle-monitor-xsync.h"

G_DEFINE_TYPE (MetaBackend, meta_backend, G_TYPE_OBJECT);

static void
meta_backend_finalize (GObject *object)
{
  MetaBackend *backend = META_BACKEND (object);
  int i;

  for (i = 0; i <= backend->device_id_max; i++)
    {
      if (backend->device_monitors[i])
        g_object_unref (backend->device_monitors[i]);
    }

  G_OBJECT_CLASS (meta_backend_parent_class)->finalize (object);
}

static void
create_device_monitor (MetaBackend *backend,
                       int          device_id)
{
  g_assert (backend->device_monitors[device_id] == NULL);

  backend->device_monitors[device_id] = g_object_new (META_TYPE_IDLE_MONITOR_XSYNC,
                                                      "device-id", device_id,
                                                      NULL);
  backend->device_id_max = MAX (backend->device_id_max, device_id);
}

static void
destroy_device_monitor (MetaBackend *backend,
                        int          device_id)
{
  g_clear_object (&backend->device_monitors[device_id]);

  if (device_id == backend->device_id_max)
    {
      /* Reset the max device ID */
      int i, new_max = 0;
      for (i = 0; i < backend->device_id_max; i++)
        if (backend->device_monitors[i] != NULL)
          new_max = i;
      backend->device_id_max = new_max;
    }
}

static void
on_device_added (GdkDeviceManager *device_manager,
                 GdkDevice        *device,
                 gpointer          user_data)
{
  MetaBackend *backend = META_BACKEND (user_data);
  int device_id = gdk_x11_device_get_id (device);

  create_device_monitor (backend, device_id);
}

static void
on_device_removed (GdkDeviceManager *device_manager,
                   GdkDevice        *device,
                   gpointer          user_data)
{
  MetaBackend *backend = META_BACKEND (user_data);
  int device_id = gdk_x11_device_get_id (device);

  destroy_device_monitor (backend, device_id);
}

static void
on_device_changed (GdkDeviceManager *device_manager,
                   GdkDevice        *device,
                   gpointer          user_data)
{
    MetaBackend *backend = META_BACKEND (user_data);

	if (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_FLOATING)
		on_device_removed (device_manager, device, backend);
	else
		on_device_added (device_manager, device, backend);
}

static void
meta_backend_class_init (MetaBackendClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_backend_finalize;
}

static void
meta_backend_init (MetaBackend *backend)
{
	GdkDeviceManager *manager;
	GList *devices, *l;

	/* Create the core device monitor. */
	create_device_monitor (backend, 0);

	manager = gdk_display_get_device_manager (gdk_display_get_default ());
	g_signal_connect_object (manager, "device-added", G_CALLBACK (on_device_added), backend, 0);
	g_signal_connect_object (manager, "device-removed", G_CALLBACK (on_device_removed), backend, 0);
	g_signal_connect_object (manager, "device-changed", G_CALLBACK (on_device_changed), backend, 0);

	devices = gdk_device_manager_list_devices (manager, GDK_DEVICE_TYPE_MASTER);
	devices = g_list_concat (devices, gdk_device_manager_list_devices (manager, GDK_DEVICE_TYPE_SLAVE));

	for (l = devices; l != NULL; l = l->next) {
		GdkDevice *device = l->data;
		on_device_added (manager, device, backend);
	}

	g_list_free (devices);
}

MetaBackend *
meta_get_backend (void)
{
  static MetaBackend *backend = NULL;

  if (!backend)
  	backend = g_object_new (META_TYPE_BACKEND, NULL);

  return backend;
}

MetaIdleMonitor *
meta_backend_get_idle_monitor (MetaBackend *backend,
                               int          device_id)
{
  g_return_val_if_fail (device_id >= 0 && device_id < 256, NULL);

  return backend->device_monitors[device_id];
}
