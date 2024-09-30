/*
 * Copyright (C) 2024 Alberts Muktupāvels
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
#include "gf-ui-scaling.h"

#include "backends/gf-monitor-manager.h"
#include "backends/gf-settings.h"
#include "dbus/gf-dbus-x11.h"

struct _GfUiScaling
{
  GObject    parent;

  GfBackend *backend;

  GfDBusX11 *x11;
  guint      bus_name_id;
};

enum
{
  PROP_0,

  PROP_BACKEND,

  LAST_PROP
};

static GParamSpec *ui_scaling_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (GfUiScaling, gf_ui_scaling, G_TYPE_OBJECT)

static void
update_ui_scaling_factor (GfUiScaling *self)
{
  GfSettings *settings;
  int ui_scaling_factor;

  settings = gf_backend_get_settings (self->backend);
  ui_scaling_factor = gf_settings_get_ui_scaling_factor (settings);

  gf_dbus_x11_set_ui_scaling_factor (self->x11, ui_scaling_factor);
}

static void
ui_scaling_factor_changed_cb (GfSettings  *settings,
                              GfUiScaling *self)
{
  update_ui_scaling_factor (self);
}

static void
bus_acquired_cb (GDBusConnection *connection,
                 const char      *name,
                 gpointer         user_data)
{
  GfUiScaling *self;
  GDBusInterfaceSkeleton *skeleton;
  GError *error;
  gboolean exported;

  self = GF_UI_SCALING (user_data);
  skeleton = G_DBUS_INTERFACE_SKELETON (self->x11);

  error = NULL;
  exported = g_dbus_interface_skeleton_export (skeleton,
                                               connection,
                                               "/org/gnome/Mutter/X11",
                                               &error);

  if (!exported)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
}

static void
name_lost_cb (GDBusConnection *connection,
              const char      *name,
              gpointer         user_data)
{
}

static void
gf_ui_scaling_constructed (GObject *object)
{
  GfUiScaling *self;

  self = GF_UI_SCALING (object);

  G_OBJECT_CLASS (gf_ui_scaling_parent_class)->constructed (object);

  self->x11 = gf_dbus_x11_skeleton_new ();

  self->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                      "org.gnome.Mutter.X11",
                                      G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                      G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                      bus_acquired_cb,
                                      name_acquired_cb,
                                      name_lost_cb,
                                      self,
                                      NULL);

  g_signal_connect (gf_backend_get_settings (self->backend),
                    "ui-scaling-factor-changed",
                    G_CALLBACK (ui_scaling_factor_changed_cb),
                    self);

  update_ui_scaling_factor (self);
}

static void
gf_ui_scaling_dispose (GObject *object)
{
  GfUiScaling *self;

  self = GF_UI_SCALING (object);

  self->backend = NULL;

  if (self->bus_name_id != 0)
    {
      g_bus_unown_name (self->bus_name_id);
      self->bus_name_id = 0;
    }

  g_clear_object (&self->x11);

  G_OBJECT_CLASS (gf_ui_scaling_parent_class)->dispose (object);
}

static void
gf_ui_scaling_get_property (GObject      *object,
                            unsigned int  property_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
  GfUiScaling *self;

  self = GF_UI_SCALING (object);

  switch (property_id)
    {
      case PROP_BACKEND:
        g_value_set_object (value, self->backend);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_ui_scaling_set_property (GObject      *object,
                            unsigned int  property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GfUiScaling *self;

  self = GF_UI_SCALING (object);

  switch (property_id)
    {
      case PROP_BACKEND:
        self->backend = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  ui_scaling_properties[PROP_BACKEND] =
    g_param_spec_object ("backend",
                         "GfBackend",
                         "GfBackend",
                         GF_TYPE_BACKEND,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     LAST_PROP,
                                     ui_scaling_properties);
}

static void
gf_ui_scaling_class_init (GfUiScalingClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = gf_ui_scaling_constructed;
  object_class->dispose = gf_ui_scaling_dispose;
  object_class->get_property = gf_ui_scaling_get_property;
  object_class->set_property = gf_ui_scaling_set_property;

  install_properties (object_class);
}

static void
gf_ui_scaling_init (GfUiScaling *self)
{
}

GfUiScaling *
gf_ui_scaling_new (GfBackend *backend)
{
  return g_object_new (GF_TYPE_UI_SCALING,
                       "backend", backend,
                       NULL);
}
