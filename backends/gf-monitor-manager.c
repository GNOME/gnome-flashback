/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2017-2019 Alberts Muktupāvels
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
 * Adapted from mutter:
 * - src/backends/meta-monitor-manager.c
 */

#include "config.h"

#include <math.h>
#include <string.h>

#include "gf-crtc-private.h"
#include "gf-logical-monitor-private.h"
#include "gf-monitor-config-manager-private.h"
#include "gf-monitor-manager-private.h"
#include "gf-monitor-normal-private.h"
#include "gf-monitor-private.h"
#include "gf-monitor-spec-private.h"
#include "gf-monitor-tiled-private.h"
#include "gf-monitors-config-private.h"
#include "gf-output-private.h"

#define DEFAULT_DISPLAY_CONFIGURATION_TIMEOUT 20

typedef struct
{
  GfBackend   *backend;

  gboolean     in_init;

  guint        bus_name_id;

  guint        persistent_timeout_id;

  GfPowerSave  power_save_mode;
} GfMonitorManagerPrivate;

typedef gboolean (* MonitorMatchFunc) (GfMonitor *monitor);

enum
{
  PROP_0,

  PROP_BACKEND,

  PROP_PANEL_ORIENTATION_MANAGED,

  LAST_PROP
};

static GParamSpec *manager_properties[LAST_PROP] = { NULL };

enum
{
  MONITORS_CHANGED,
  POWER_SAVE_MODE_CHANGED,
  CONFIRM_DISPLAY_CHANGE,

  LAST_SIGNAL
};

static guint manager_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GfMonitorManager, gf_monitor_manager, G_TYPE_OBJECT)

/* Array index matches GfMonitorTransform */
static gfloat transform_matrices[][6] =
  {
    {  1,  0,  0,  0,  1,  0 }, /* normal */
    {  0, -1,  1,  1,  0,  0 }, /* 90° */
    { -1,  0,  1,  0, -1,  1 }, /* 180° */
    {  0,  1,  0, -1,  0,  1 }, /* 270° */
    { -1,  0,  1,  0,  1,  0 }, /* normal flipped */
    {  0,  1,  0,  1,  0,  0 }, /* 90° flipped */
    {  1,  0,  0,  0, -1,  1 }, /* 180° flipped */
    {  0, -1,  1, -1,  0,  1 }, /* 270° flipped */
  };

static inline void
multiply_matrix (gfloat a[6],
                 gfloat b[6],
                 gfloat res[6])
{
  res[0] = a[0] * b[0] + a[1] * b[3];
  res[1] = a[0] * b[1] + a[1] * b[4];
  res[2] = a[0] * b[2] + a[1] * b[5] + a[2];
  res[3] = a[3] * b[0] + a[4] * b[3];
  res[4] = a[3] * b[1] + a[4] * b[4];
  res[5] = a[3] * b[2] + a[4] * b[5] + a[5];
}

static gboolean
calculate_viewport_matrix (GfMonitorManager *manager,
                           GfLogicalMonitor *logical_monitor,
                           gfloat            viewport[6])
{
  gfloat x, y, width, height;

  x = (gfloat) logical_monitor->rect.x / manager->screen_width;
  y = (gfloat) logical_monitor->rect.y / manager->screen_height;
  width  = (gfloat) logical_monitor->rect.width / manager->screen_width;
  height = (gfloat) logical_monitor->rect.height / manager->screen_height;

  viewport[0] = width;
  viewport[1] = 0.0f;
  viewport[2] = x;
  viewport[3] = 0.0f;
  viewport[4] = height;
  viewport[5] = y;

  return TRUE;
}

static void
power_save_mode_changed (GfMonitorManager *manager,
                         GParamSpec       *pspec,
                         gpointer          user_data)
{
  GfMonitorManagerPrivate *priv;
  GfMonitorManagerClass *manager_class;
  gint mode;

  priv = gf_monitor_manager_get_instance_private (manager);
  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);
  mode = gf_dbus_display_config_get_power_save_mode (manager->display_config);

  if (mode == GF_POWER_SAVE_UNSUPPORTED)
    return;

  /* If DPMS is unsupported, force the property back. */
  if (priv->power_save_mode == GF_POWER_SAVE_UNSUPPORTED)
    {
      gf_dbus_display_config_set_power_save_mode (manager->display_config,
                                                  GF_POWER_SAVE_UNSUPPORTED);
      return;
    }

  if (manager_class->set_power_save_mode)
    manager_class->set_power_save_mode (manager, mode);

  gf_monitor_manager_power_save_mode_changed (manager, mode);
}

static void
gf_monitor_manager_update_monitor_modes_derived (GfMonitorManager *manager)
{
  GList *l;

  for (l = manager->monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;

      gf_monitor_derive_current_mode (monitor);
    }
}

static void
gf_monitor_manager_notify_monitors_changed (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;

  priv = gf_monitor_manager_get_instance_private (manager);

  gf_backend_monitors_changed (priv->backend);

  g_signal_emit (manager, manager_signals[MONITORS_CHANGED], 0);

  gf_dbus_display_config_emit_monitors_changed (manager->display_config);
}

static GfMonitor *
find_monitor (GfMonitorManager *monitor_manager,
              MonitorMatchFunc  match_func)
{
  GList *monitors;
  GList *l;

  monitors = gf_monitor_manager_get_monitors (monitor_manager);
  for (l = monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;

      if (match_func (monitor))
        return monitor;
    }

  return NULL;
}

static GfMonitor *
get_active_monitor (GfMonitorManager *manager)
{
  return find_monitor (manager, gf_monitor_is_active);
}

static gboolean
gf_monitor_manager_is_config_applicable (GfMonitorManager  *manager,
                                         GfMonitorsConfig  *config,
                                         GError           **error)
{
  GList *l;

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config = l->data;
      gfloat scale = logical_monitor_config->scale;
      GList *k;

      for (k = logical_monitor_config->monitor_configs; k; k = k->next)
        {
          GfMonitorConfig *monitor_config = k->data;
          GfMonitorSpec *monitor_spec = monitor_config->monitor_spec;
          GfMonitorModeSpec *mode_spec = monitor_config->mode_spec;
          GfMonitor *monitor;
          GfMonitorMode *monitor_mode;

          monitor = gf_monitor_manager_get_monitor_from_spec (manager, monitor_spec);
          if (!monitor)
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Specified monitor not found");

              return FALSE;
            }

          monitor_mode = gf_monitor_get_mode_from_spec (monitor, mode_spec);
          if (!monitor_mode)
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Specified monitor mode not available");

              return FALSE;
            }

          if (!gf_monitor_manager_is_scale_supported (manager, config->layout_mode,
                                                      monitor, monitor_mode, scale))
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Scale not supported by backend");

              return FALSE;
            }

          if (gf_monitor_is_laptop_panel (monitor) &&
              gf_monitor_manager_is_lid_closed (manager))
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Refusing to activate a closed laptop panel");
              return FALSE;
            }
        }
    }

  return TRUE;
}

static gboolean
gf_monitor_manager_is_config_complete (GfMonitorManager *manager,
                                       GfMonitorsConfig *config)
{
  GfMonitorsConfigKey *current_state_key;
  gboolean is_config_complete;

  current_state_key = gf_create_monitors_config_key_for_current_state (manager);
  if (!current_state_key)
    return FALSE;

  is_config_complete = gf_monitors_config_key_equal (current_state_key, config->key);
  gf_monitors_config_key_free (current_state_key);

  if (!is_config_complete)
    return FALSE;

  return gf_monitor_manager_is_config_applicable (manager, config, NULL);
}

static gboolean
should_use_stored_config (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;

  priv = gf_monitor_manager_get_instance_private (manager);

  return (priv->in_init || !gf_monitor_manager_has_hotplug_mode_update (manager));
}

static gfloat
derive_configured_global_scale (GfMonitorManager *manager,
                                GfMonitorsConfig *config)
{
  GfLogicalMonitorConfig *logical_monitor_config;

  logical_monitor_config = config->logical_monitor_configs->data;

  return logical_monitor_config->scale;
}

static gfloat
calculate_monitor_scale (GfMonitorManager *manager,
                         GfMonitor        *monitor)
{
  GfMonitorMode *monitor_mode;

  monitor_mode = gf_monitor_get_current_mode (monitor);
  return gf_monitor_manager_calculate_monitor_mode_scale (manager, monitor,
                                                          monitor_mode);
}

static gfloat
derive_calculated_global_scale (GfMonitorManager *manager)
{
  GfMonitor *monitor;

  monitor = gf_monitor_manager_get_primary_monitor (manager);

  if (!monitor || !gf_monitor_is_active (monitor))
    monitor = get_active_monitor (manager);

  if (!monitor)
    return 1.0;

  return calculate_monitor_scale (manager, monitor);
}

static GfLogicalMonitor *
logical_monitor_from_layout (GfMonitorManager *manager,
                             GList            *logical_monitors,
                             GfRectangle      *layout)
{
  GList *l;

  for (l = logical_monitors; l; l = l->next)
    {
      GfLogicalMonitor *logical_monitor = l->data;

      if (gf_rectangle_equal (layout, &logical_monitor->rect))
        return logical_monitor;
    }

  return NULL;
}

static gfloat
derive_scale_from_config (GfMonitorManager *manager,
                          GfMonitorsConfig *config,
                          GfRectangle      *layout)
{
  GList *l;

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config = l->data;

      if (gf_rectangle_equal (layout, &logical_monitor_config->layout))
        return logical_monitor_config->scale;
    }

  g_warning ("Missing logical monitor, using scale 1");
  return 1.0;
}

static void
gf_monitor_manager_set_primary_logical_monitor (GfMonitorManager *manager,
                                                GfLogicalMonitor *logical_monitor)
{
  manager->primary_logical_monitor = logical_monitor;
  if (logical_monitor)
    gf_logical_monitor_make_primary (logical_monitor);
}

static void
gf_monitor_manager_rebuild_logical_monitors_derived (GfMonitorManager *manager,
                                                     GfMonitorsConfig *config)
{
  GList *logical_monitors = NULL;
  GList *l;
  gint monitor_number;
  GfLogicalMonitor *primary_logical_monitor = NULL;
  gboolean use_global_scale;
  gfloat global_scale = 0.0;
  GfMonitorManagerCapability capabilities;

  monitor_number = 0;

  capabilities = gf_monitor_manager_get_capabilities (manager);
  use_global_scale = !!(capabilities & GF_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED);

  if (use_global_scale)
    {
      if (config)
        global_scale = derive_configured_global_scale (manager, config);
      else
        global_scale = derive_calculated_global_scale (manager);
    }

  for (l = manager->monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;
      GfLogicalMonitor *logical_monitor;
      GfRectangle layout;

      if (!gf_monitor_is_active (monitor))
        continue;

      gf_monitor_derive_layout (monitor, &layout);
      logical_monitor = logical_monitor_from_layout (manager, logical_monitors,
                                                     &layout);
      if (logical_monitor)
        {
          gf_logical_monitor_add_monitor (logical_monitor, monitor);
        }
      else
        {
          gfloat scale;

          if (use_global_scale)
            scale = global_scale;
          else if (config)
            scale = derive_scale_from_config (manager, config, &layout);
          else
            scale = calculate_monitor_scale (manager, monitor);

          g_assert (scale > 0);

          logical_monitor = gf_logical_monitor_new_derived (manager, monitor,
                                                            &layout, scale,
                                                            monitor_number);

          logical_monitors = g_list_append (logical_monitors, logical_monitor);
          monitor_number++;
        }

      if (gf_monitor_is_primary (monitor))
        primary_logical_monitor = logical_monitor;
    }

  manager->logical_monitors = logical_monitors;

  /*
   * If no monitor was marked as primary, fall back on marking the first
   * logical monitor the primary one.
   */
  if (!primary_logical_monitor && manager->logical_monitors)
    primary_logical_monitor = g_list_first (manager->logical_monitors)->data;

  gf_monitor_manager_set_primary_logical_monitor (manager, primary_logical_monitor);
}

static gboolean
gf_monitor_manager_apply_monitors_config (GfMonitorManager        *manager,
                                          GfMonitorsConfig        *config,
                                          GfMonitorsConfigMethod   method,
                                          GError                 **error)
{
  GfMonitorManagerClass *manager_class;

  g_assert (!config || !(config->flags & GF_MONITORS_CONFIG_FLAG_MIGRATED));

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  if (!manager_class->apply_monitors_config (manager, config, method, error))
    return FALSE;

  switch (method)
    {
      case GF_MONITORS_CONFIG_METHOD_TEMPORARY:
      case GF_MONITORS_CONFIG_METHOD_PERSISTENT:
        gf_monitor_config_manager_set_current (manager->config_manager, config);
        break;

      case GF_MONITORS_CONFIG_METHOD_VERIFY:
      default:
        break;
    }

  return TRUE;
}

static void
orientation_changed (GfOrientationManager *orientation_manager,
                     GfMonitorManager     *manager)
{
  GfMonitorTransform transform;
  GError *error = NULL;
  GfMonitorsConfig *config;

  if (!manager->panel_orientation_managed)
    return;

  switch (gf_orientation_manager_get_orientation (orientation_manager))
    {
      case GF_ORIENTATION_NORMAL:
        transform = GF_MONITOR_TRANSFORM_NORMAL;
        break;
      case GF_ORIENTATION_BOTTOM_UP:
        transform = GF_MONITOR_TRANSFORM_180;
        break;
      case GF_ORIENTATION_LEFT_UP:
        transform = GF_MONITOR_TRANSFORM_90;
        break;
      case GF_ORIENTATION_RIGHT_UP:
        transform = GF_MONITOR_TRANSFORM_270;
        break;

      case GF_ORIENTATION_UNDEFINED:
      default:
        return;
    }

  config = gf_monitor_config_manager_create_for_orientation (manager->config_manager,
                                                             transform);

  if (!config)
    return;

  if (!gf_monitor_manager_apply_monitors_config (manager, config,
                                                 GF_MONITORS_CONFIG_METHOD_TEMPORARY,
                                                 &error))
    {
      g_warning ("Failed to use orientation monitor configuration: %s",
                 error->message);
      g_error_free (error);
    }

  g_object_unref (config);
}

static void
update_panel_orientation_managed (GfMonitorManager *self)
{
  GfMonitorManagerPrivate *priv;
  GfOrientationManager *orientation_manager;
  gboolean panel_orientation_managed;

  priv = gf_monitor_manager_get_instance_private (self);

  orientation_manager = gf_backend_get_orientation_manager (priv->backend);
  panel_orientation_managed = gf_orientation_manager_has_accelerometer (orientation_manager);

  if (self->panel_orientation_managed == panel_orientation_managed)
    return;

  self->panel_orientation_managed = panel_orientation_managed;

  g_object_notify_by_pspec (G_OBJECT (self),
                            manager_properties[PROP_PANEL_ORIENTATION_MANAGED]);

  gf_dbus_display_config_set_panel_orientation_managed (self->display_config,
                                                        panel_orientation_managed);
}

static void
restore_previous_config (GfMonitorManager *manager)
{
  GfMonitorsConfig *previous_config;

  previous_config = gf_monitor_config_manager_pop_previous (manager->config_manager);

  if (previous_config)
    {
      GfMonitorsConfigMethod method;
      GError *error;

      method = GF_MONITORS_CONFIG_METHOD_TEMPORARY;
      error = NULL;

      if (gf_monitor_manager_apply_monitors_config (manager, previous_config,
                                                    method, &error))
        {
          g_object_unref (previous_config);
          return;
        }
      else
        {
          g_object_unref (previous_config);
          g_warning ("Failed to restore previous configuration: %s", error->message);
          g_error_free (error);
        }
    }

  gf_monitor_manager_ensure_configured (manager);
}

static gboolean
save_config_timeout (gpointer user_data)
{
  GfMonitorManager *manager = user_data;
  GfMonitorManagerPrivate *priv;

  priv = gf_monitor_manager_get_instance_private (manager);

  restore_previous_config (manager);
  priv->persistent_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
cancel_persistent_confirmation (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;

  priv = gf_monitor_manager_get_instance_private (manager);

  if (priv->persistent_timeout_id != 0)
    {
      g_source_remove (priv->persistent_timeout_id);
      priv->persistent_timeout_id = 0;
    }
}

static void
confirm_configuration (GfMonitorManager *manager,
                       gboolean          confirmed)
{
  if (confirmed)
    gf_monitor_config_manager_save_current (manager->config_manager);
  else
    restore_previous_config (manager);
}

static void
request_persistent_confirmation (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;
  gint timeout;

  priv = gf_monitor_manager_get_instance_private (manager);
  timeout = gf_monitor_manager_get_display_configuration_timeout ();

  priv->persistent_timeout_id = g_timeout_add_seconds (timeout,
                                                       save_config_timeout,
                                                       manager);

  g_source_set_name_by_id (priv->persistent_timeout_id,
                           "[gnome-flashback] save_config_timeout");

  g_signal_emit (manager, manager_signals[CONFIRM_DISPLAY_CHANGE], 0);
}

static gboolean
find_monitor_mode_scale (GfMonitorManager            *manager,
                         GfLogicalMonitorLayoutMode   layout_mode,
                         GfMonitorConfig             *monitor_config,
                         gfloat                       scale,
                         gfloat                      *out_scale,
                         GError                     **error)
{
  GfMonitorSpec *monitor_spec;
  GfMonitor *monitor;
  GfMonitorModeSpec *monitor_mode_spec;
  GfMonitorMode *monitor_mode;
  gfloat *supported_scales;
  gint n_supported_scales;
  gint i;

  monitor_spec = monitor_config->monitor_spec;
  monitor = gf_monitor_manager_get_monitor_from_spec (manager, monitor_spec);

  if (!monitor)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Monitor not found");

      return FALSE;
    }

  monitor_mode_spec = monitor_config->mode_spec;
  monitor_mode = gf_monitor_get_mode_from_spec (monitor, monitor_mode_spec);

  if (!monitor_mode)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Monitor mode not found");

      return FALSE;
    }

  supported_scales = gf_monitor_manager_calculate_supported_scales (manager, layout_mode,
                                                                    monitor, monitor_mode,
                                                                    &n_supported_scales);

  for (i = 0; i < n_supported_scales; i++)
    {
      gfloat supported_scale = supported_scales[i];

      if (fabsf (supported_scale - scale) < FLT_EPSILON)
        {
          *out_scale = supported_scale;
          g_free (supported_scales);
          return TRUE;
        }
    }

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
               "Scale %g not valid for resolution %dx%d",
               (gdouble) scale,
               monitor_mode_spec->width,
               monitor_mode_spec->height);

  g_free (supported_scales);
  return FALSE;
}

static gboolean
derive_logical_monitor_size (GfMonitorConfig             *monitor_config,
                             gint                        *out_width,
                             gint                        *out_height,
                             gfloat                       scale,
                             GfMonitorTransform           transform,
                             GfLogicalMonitorLayoutMode   layout_mode,
                             GError                     **error)
{
  gint width, height;

  if (gf_monitor_transform_is_rotated (transform))
    {
      width = monitor_config->mode_spec->height;
      height = monitor_config->mode_spec->width;
    }
  else
    {
      width = monitor_config->mode_spec->width;
      height = monitor_config->mode_spec->height;
    }

  switch (layout_mode)
    {
      case GF_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
        width = roundf (width / scale);
        height = roundf (height / scale);
        break;

      case GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
      default:
        break;
    }

  *out_width = width;
  *out_height = height;

  return TRUE;
}

static GfMonitor *
find_monitor_from_connector (GfMonitorManager *manager,
                             gchar            *connector)
{
  GList *monitors;
  GList *l;

  if (!connector)
    return NULL;

  monitors = gf_monitor_manager_get_monitors (manager);
  for (l = monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;
      GfMonitorSpec *monitor_spec = gf_monitor_get_spec (monitor);

      if (g_str_equal (connector, monitor_spec->connector))
        return monitor;
    }

  return NULL;
}

static GfMonitorConfig *
create_monitor_config_from_variant (GfMonitorManager  *manager,
                                    GVariant          *monitor_config_variant,
                                    GError           **error)
{
  gchar *connector;
  gchar *mode_id;
  GVariant *properties_variant;
  GfMonitor *monitor;
  GfMonitorMode *monitor_mode;
  gboolean enable_underscanning;
  gboolean set_underscanning;
  GfMonitorSpec *monitor_spec;
  GfMonitorModeSpec *monitor_mode_spec;
  GfMonitorConfig *monitor_config;

  connector = NULL;
  mode_id = NULL;
  properties_variant = NULL;

  g_variant_get (monitor_config_variant, "(ss@a{sv})",
                 &connector, &mode_id, &properties_variant);

  monitor = find_monitor_from_connector (manager, connector);
  if (!monitor)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid connector '%s' specified", connector);

      g_variant_unref (properties_variant);
      g_free (connector);
      g_free (mode_id);

      return NULL;
    }

  monitor_mode = gf_monitor_get_mode_from_id (monitor, mode_id);
  if (!monitor_mode)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid mode '%s' specified", mode_id);

      g_variant_unref (properties_variant);
      g_free (connector);
      g_free (mode_id);

      return NULL;
    }

  enable_underscanning = FALSE;
  set_underscanning = g_variant_lookup (properties_variant,
                                        "underscanning", "b",
                                        &enable_underscanning);

  g_variant_unref (properties_variant);
  g_free (connector);
  g_free (mode_id);

  if (set_underscanning)
    {
      if (enable_underscanning && !gf_monitor_supports_underscanning (monitor))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Underscanning requested but unsupported");

          return NULL;
        }
    }

  monitor_spec = gf_monitor_spec_clone (gf_monitor_get_spec (monitor));

  monitor_mode_spec = g_new0 (GfMonitorModeSpec, 1);
  *monitor_mode_spec = *gf_monitor_mode_get_spec (monitor_mode);

  monitor_config = g_new0 (GfMonitorConfig, 1);
  *monitor_config = (GfMonitorConfig) {
    .monitor_spec = monitor_spec,
    .mode_spec = monitor_mode_spec,
    .enable_underscanning = enable_underscanning
  };

  return monitor_config;
}

#define MONITOR_CONFIG_FORMAT "(ssa{sv})"
#define MONITOR_CONFIGS_FORMAT "a" MONITOR_CONFIG_FORMAT
#define LOGICAL_MONITOR_CONFIG_FORMAT "(iidub" MONITOR_CONFIGS_FORMAT ")"

static GfLogicalMonitorConfig *
create_logical_monitor_config_from_variant (GfMonitorManager            *manager,
                                            GVariant                    *logical_monitor_config_variant,
                                            GfLogicalMonitorLayoutMode   layout_mode,
                                            GError                      **error)
{
  GfLogicalMonitorConfig *logical_monitor_config;
  gint x, y, width, height;
  gdouble scale_d;
  gfloat scale;
  GfMonitorTransform transform;
  gboolean is_primary;
  GVariantIter *monitor_configs_iter;
  GList *monitor_configs = NULL;
  GfMonitorConfig *monitor_config;

  g_variant_get (logical_monitor_config_variant, LOGICAL_MONITOR_CONFIG_FORMAT,
                 &x, &y, &scale_d, &transform, &is_primary, &monitor_configs_iter);

  scale = (gfloat) scale_d;

  while (TRUE)
    {
      GVariant *monitor_config_variant;

      monitor_config_variant = g_variant_iter_next_value (monitor_configs_iter);

      if (!monitor_config_variant)
        break;

      monitor_config = create_monitor_config_from_variant (manager,
                                                           monitor_config_variant,
                                                           error);

      g_variant_unref (monitor_config_variant);

      if (!monitor_config)
        goto err;

      if (!gf_verify_monitor_config (monitor_config, error))
        {
          gf_monitor_config_free (monitor_config);
          goto err;
        }

      monitor_configs = g_list_append (monitor_configs, monitor_config);
    }
  g_variant_iter_free (monitor_configs_iter);

  if (!monitor_configs)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Empty logical monitor");
      goto err;
    }

  monitor_config = monitor_configs->data;
  if (!find_monitor_mode_scale (manager, layout_mode, monitor_config,
                                scale, &scale, error))
    goto err;

  if (!derive_logical_monitor_size (monitor_config, &width, &height,
                                    scale, transform, layout_mode, error))
    goto err;

  logical_monitor_config = g_new0 (GfLogicalMonitorConfig, 1);
  *logical_monitor_config = (GfLogicalMonitorConfig) {
    .layout = {
      .x = x,
      .y = y,
      .width = width,
      .height = height
    },
    .transform = transform,
    .scale = scale,
    .is_primary = is_primary,
    .monitor_configs = monitor_configs
  };

  if (!gf_verify_logical_monitor_config (logical_monitor_config, layout_mode,
                                         manager, error))
    {
      gf_logical_monitor_config_free (logical_monitor_config);
      return NULL;
    }

  return logical_monitor_config;

err:
  g_list_free_full (monitor_configs, (GDestroyNotify) gf_monitor_config_free);
  return NULL;
}

#undef MONITOR_MODE_SPEC_FORMAT
#undef MONITOR_CONFIG_FORMAT
#undef MONITOR_CONFIGS_FORMAT
#undef LOGICAL_MONITOR_CONFIG_FORMAT

static gboolean
is_valid_layout_mode (GfLogicalMonitorLayoutMode layout_mode)
{
  switch (layout_mode)
    {
      case GF_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
      case GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
        return TRUE;

      default:
        break;
    }

  return FALSE;
}

static const gchar *
get_connector_type_name (GfConnectorType connector_type)
{
  switch (connector_type)
    {
      case GF_CONNECTOR_TYPE_Unknown: return "Unknown";
      case GF_CONNECTOR_TYPE_VGA: return "VGA";
      case GF_CONNECTOR_TYPE_DVII: return "DVII";
      case GF_CONNECTOR_TYPE_DVID: return "DVID";
      case GF_CONNECTOR_TYPE_DVIA: return "DVIA";
      case GF_CONNECTOR_TYPE_Composite: return "Composite";
      case GF_CONNECTOR_TYPE_SVIDEO: return "SVIDEO";
      case GF_CONNECTOR_TYPE_LVDS: return "LVDS";
      case GF_CONNECTOR_TYPE_Component: return "Component";
      case GF_CONNECTOR_TYPE_9PinDIN: return "9PinDIN";
      case GF_CONNECTOR_TYPE_DisplayPort: return "DisplayPort";
      case GF_CONNECTOR_TYPE_HDMIA: return "HDMIA";
      case GF_CONNECTOR_TYPE_HDMIB: return "HDMIB";
      case GF_CONNECTOR_TYPE_TV: return "TV";
      case GF_CONNECTOR_TYPE_eDP: return "eDP";
      case GF_CONNECTOR_TYPE_VIRTUAL: return "VIRTUAL";
      case GF_CONNECTOR_TYPE_DSI: return "DSI";
      default: g_assert_not_reached ();
    }

  return NULL;
}

static gboolean
is_main_tiled_monitor_output (GfOutput *output)
{
  const GfOutputInfo *output_info;

  output_info = gf_output_get_info (output);

  return (output_info->tile_info.loc_h_tile == 0 &&
          output_info->tile_info.loc_v_tile == 0);
}

static void
rebuild_monitors (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;
  GList *gpus;
  GList *l;

  priv = gf_monitor_manager_get_instance_private (manager);

  if (manager->monitors)
    {
      g_list_free_full (manager->monitors, g_object_unref);
      manager->monitors = NULL;
    }

  gpus = gf_backend_get_gpus (priv->backend);

  for (l = gpus; l; l = l->next)
    {
      GfGpu *gpu = l->data;
      GList *k;

      for (k = gf_gpu_get_outputs (gpu); k; k = k->next)
        {
          GfOutput *output = k->data;
          const GfOutputInfo *output_info;

          output_info = gf_output_get_info (output);

          if (output_info->tile_info.group_id)
            {
              if (is_main_tiled_monitor_output (output))
                {
                  GfMonitorTiled *monitor_tiled;

                  monitor_tiled = gf_monitor_tiled_new (gpu, manager, output);
                  manager->monitors = g_list_append (manager->monitors, monitor_tiled);
                }
            }
          else
            {
              GfMonitorNormal *monitor_normal;

              monitor_normal = gf_monitor_normal_new (gpu, output);
              manager->monitors = g_list_append (manager->monitors, monitor_normal);
            }
        }
    }
}

static GList *
combine_gpu_lists (GfMonitorManager *manager,
                   GList            * (*list_getter) (GfGpu *gpu))
{
  GfMonitorManagerPrivate *priv;
  GList *gpus;
  GList *list = NULL;
  GList *l;

  priv = gf_monitor_manager_get_instance_private (manager);

  gpus = gf_backend_get_gpus (priv->backend);

  for (l = gpus; l; l = l->next)
    {
      GfGpu *gpu = l->data;

      list = g_list_concat (list, g_list_copy (list_getter (gpu)));
    }

  return list;
}

static gboolean
gf_monitor_manager_handle_get_resources (GfDBusDisplayConfig   *skeleton,
                                         GDBusMethodInvocation *invocation,
                                         GfMonitorManager      *manager)
{
  GfMonitorManagerClass *manager_class;
  GList *combined_modes;
  GList *combined_outputs;
  GList *combined_crtcs;
  GVariantBuilder crtc_builder;
  GVariantBuilder output_builder;
  GVariantBuilder mode_builder;
  GList *l;
  guint i, j;
  gint max_screen_width;
  gint max_screen_height;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  combined_modes = combine_gpu_lists (manager, gf_gpu_get_modes);
  combined_outputs = combine_gpu_lists (manager, gf_gpu_get_outputs);
  combined_crtcs = combine_gpu_lists (manager, gf_gpu_get_crtcs);

  g_variant_builder_init (&crtc_builder, G_VARIANT_TYPE ("a(uxiiiiiuaua{sv})"));
  g_variant_builder_init (&output_builder, G_VARIANT_TYPE ("a(uxiausauaua{sv})"));
  g_variant_builder_init (&mode_builder, G_VARIANT_TYPE ("a(uxuudu)"));

  for (l = combined_crtcs, i = 0; l; l = l->next, i++)
    {
      GfCrtc *crtc = l->data;
      GVariantBuilder transforms;
      const GfCrtcConfig *crtc_config;

      g_variant_builder_init (&transforms, G_VARIANT_TYPE ("au"));
      for (j = 0; j <= GF_MONITOR_TRANSFORM_FLIPPED_270; j++)
        {
          if (gf_crtc_get_all_transforms (crtc) & (1 << j))
            g_variant_builder_add (&transforms, "u", j);
        }

      crtc_config = gf_crtc_get_config (crtc);

      if (crtc_config != NULL)
        {
          int current_mode_index;

          current_mode_index = g_list_index (combined_modes, crtc_config->mode);

          g_variant_builder_add (&crtc_builder, "(uxiiiiiuaua{sv})",
                                 i, /* ID */
                                 (int64_t) gf_crtc_get_id (crtc),
                                 crtc_config->layout.x,
                                 crtc_config->layout.y,
                                 crtc_config->layout.width,
                                 crtc_config->layout.height,
                                 current_mode_index,
                                 (uint32_t) crtc_config->transform,
                                 &transforms,
                                 NULL /* properties */);
        }
      else
        {
          g_variant_builder_add (&crtc_builder, "(uxiiiiiuaua{sv})",
                                 i, /* ID */
                                 (int64_t) gf_crtc_get_id (crtc),
                                 0,
                                 0,
                                 0,
                                 0,
                                 -1,
                                 (uint32_t) GF_MONITOR_TRANSFORM_NORMAL,
                                 &transforms,
                                 NULL /* properties */);
        }
    }

  for (l = combined_outputs, i = 0; l; l = l->next, i++)
    {
      GfOutput *output;
      const GfOutputInfo *output_info;
      GVariantBuilder crtcs, modes, clones, properties;
      GBytes *edid;
      GfCrtc *crtc;
      int crtc_index;
      int backlight;
      int min_backlight_step;
      gboolean is_primary;
      gboolean is_presentation;
      const char *connector_type_name;
      gboolean is_underscanning;
      gboolean supports_underscanning;

      output = l->data;
      output_info = gf_output_get_info (output);

      g_variant_builder_init (&crtcs, G_VARIANT_TYPE ("au"));
      for (j = 0; j < output_info->n_possible_crtcs; j++)
        {
          GfCrtc *possible_crtc;
          guint possible_crtc_index;

          possible_crtc = output_info->possible_crtcs[j];
          possible_crtc_index = g_list_index (combined_crtcs, possible_crtc);

          g_variant_builder_add (&crtcs, "u", possible_crtc_index);
        }

      g_variant_builder_init (&modes, G_VARIANT_TYPE ("au"));
      for (j = 0; j < output_info->n_modes; j++)
        {
          guint mode_index;

          mode_index = g_list_index (combined_modes, output_info->modes[j]);
          g_variant_builder_add (&modes, "u", mode_index);

        }

      g_variant_builder_init (&clones, G_VARIANT_TYPE ("au"));
      for (j = 0; j < output_info->n_possible_clones; j++)
        {
          guint possible_clone_index;

          possible_clone_index = g_list_index (combined_outputs,
                                               output_info->possible_clones[j]);

          g_variant_builder_add (&clones, "u", possible_clone_index);
        }

      backlight = gf_output_get_backlight (output);
      min_backlight_step =
        output_info->backlight_max - output_info->backlight_min
        ? 100 / (output_info->backlight_max - output_info->backlight_min)
        : -1;
      is_primary = gf_output_is_primary (output);
      is_presentation = gf_output_is_presentation (output);
      is_underscanning = gf_output_is_underscanning (output);
      connector_type_name = get_connector_type_name (output_info->connector_type);
      supports_underscanning = output_info->supports_underscanning;

      g_variant_builder_init (&properties, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&properties, "{sv}", "vendor",
                             g_variant_new_string (output_info->vendor));
      g_variant_builder_add (&properties, "{sv}", "product",
                             g_variant_new_string (output_info->product));
      g_variant_builder_add (&properties, "{sv}", "serial",
                             g_variant_new_string (output_info->serial));
      g_variant_builder_add (&properties, "{sv}", "width-mm",
                             g_variant_new_int32 (output_info->width_mm));
      g_variant_builder_add (&properties, "{sv}", "height-mm",
                             g_variant_new_int32 (output_info->height_mm));
      g_variant_builder_add (&properties, "{sv}", "display-name",
                             g_variant_new_string (output_info->name));
      g_variant_builder_add (&properties, "{sv}", "backlight",
                             g_variant_new_int32 (backlight));
      g_variant_builder_add (&properties, "{sv}", "min-backlight-step",
                             g_variant_new_int32 (min_backlight_step));
      g_variant_builder_add (&properties, "{sv}", "primary",
                             g_variant_new_boolean (is_primary));
      g_variant_builder_add (&properties, "{sv}", "presentation",
                             g_variant_new_boolean (is_presentation));
      g_variant_builder_add (&properties, "{sv}", "connector-type",
                             g_variant_new_string (connector_type_name));
      g_variant_builder_add (&properties, "{sv}", "underscanning",
                             g_variant_new_boolean (is_underscanning));
      g_variant_builder_add (&properties, "{sv}", "supports-underscanning",
                             g_variant_new_boolean (supports_underscanning));

      edid = manager_class->read_edid (manager, output);

      if (edid)
        {
          g_variant_builder_add (&properties, "{sv}", "edid",
                                 g_variant_new_from_bytes (G_VARIANT_TYPE ("ay"),
                                                           edid, TRUE));
          g_bytes_unref (edid);
        }

      if (output_info->tile_info.group_id)
        {
          GVariant *tile_variant;

          tile_variant = g_variant_new ("(uuuuuuuu)",
                                        output_info->tile_info.group_id,
                                        output_info->tile_info.flags,
                                        output_info->tile_info.max_h_tiles,
                                        output_info->tile_info.max_v_tiles,
                                        output_info->tile_info.loc_h_tile,
                                        output_info->tile_info.loc_v_tile,
                                        output_info->tile_info.tile_w,
                                        output_info->tile_info.tile_h);

          g_variant_builder_add (&properties, "{sv}", "tile", tile_variant);
        }

      crtc = gf_output_get_assigned_crtc (output);
      crtc_index = crtc ? g_list_index (combined_crtcs, crtc) : -1;

      g_variant_builder_add (&output_builder, "(uxiausauaua{sv})",
                             i, /* ID */
                             gf_output_get_id (output),
                             crtc_index,
                             &crtcs,
                             gf_output_get_name (output),
                             &modes,
                             &clones,
                             &properties);
    }

  for (l = combined_modes, i = 0; l; l = l->next, i++)
    {
      GfCrtcMode *mode = l->data;
      const GfCrtcModeInfo *crtc_mode_info;

      crtc_mode_info = gf_crtc_mode_get_info (mode);

      g_variant_builder_add (&mode_builder, "(uxuudu)",
                             i, /* ID */
                             (int64_t) gf_crtc_mode_get_id (mode),
                             (uint32_t) crtc_mode_info->width,
                             (uint32_t) crtc_mode_info->height,
                             (double) crtc_mode_info->refresh_rate,
                             (uint32_t) crtc_mode_info->flags);
    }

  if (!gf_monitor_manager_get_max_screen_size (manager,
                                               &max_screen_width,
                                               &max_screen_height))
    {
      /* No max screen size, just send something large */
      max_screen_width = 65535;
      max_screen_height = 65535;
    }

  gf_dbus_display_config_complete_get_resources (skeleton, invocation, manager->serial,
                                                 g_variant_builder_end (&crtc_builder),
                                                 g_variant_builder_end (&output_builder),
                                                 g_variant_builder_end (&mode_builder),
                                                 max_screen_width, max_screen_height);

  g_list_free (combined_modes);
  g_list_free (combined_outputs);
  g_list_free (combined_crtcs);

  return TRUE;
}

static gboolean
gf_monitor_manager_handle_change_backlight (GfDBusDisplayConfig   *skeleton,
                                            GDBusMethodInvocation *invocation,
                                            guint                  serial,
                                            guint                  output_index,
                                            gint                   value,
                                            GfMonitorManager      *manager)
{
  GfMonitorManagerClass *manager_class;
  GList *combined_outputs;
  GfOutput *output;
  const GfOutputInfo *output_info;
  int new_backlight;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  combined_outputs = combine_gpu_lists (manager, gf_gpu_get_outputs);

  if (output_index >= g_list_length (combined_outputs))

    {
      g_list_free (combined_outputs);
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid output id");
      return TRUE;
    }

  output = g_list_nth_data (combined_outputs, output_index);
  g_list_free (combined_outputs);

  if (value < 0 || value > 100)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid backlight value");
      return TRUE;
    }

  output_info = gf_output_get_info (output);

  if (gf_output_get_backlight (output) == -1 ||
      (output_info->backlight_min == 0 &&
       output_info->backlight_max == 0))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Output does not support changing backlight");
      return TRUE;
    }

  manager_class->change_backlight (manager, output, value);

  new_backlight = gf_output_get_backlight (output);
  gf_dbus_display_config_complete_change_backlight (skeleton,
                                                    invocation,
                                                    new_backlight);

  return TRUE;
}

static gboolean
gf_monitor_manager_handle_get_crtc_gamma (GfDBusDisplayConfig   *skeleton,
                                          GDBusMethodInvocation *invocation,
                                          guint                  serial,
                                          guint                  crtc_id,
                                          GfMonitorManager      *manager)
{
  GfMonitorManagerClass *manager_class;
  GList *combined_crtcs;
  GfCrtc *crtc;
  gsize size;
  gushort *red;
  gushort *green;
  gushort *blue;
  GBytes *red_bytes;
  GBytes *green_bytes;
  GBytes *blue_bytes;
  GVariant *red_v;
  GVariant *green_v;
  GVariant *blue_v;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  combined_crtcs = combine_gpu_lists (manager, gf_gpu_get_crtcs);

  if (crtc_id >= g_list_length (combined_crtcs))
    {
      g_list_free (combined_crtcs);
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid crtc id");
      return TRUE;
    }

  crtc = g_list_nth_data (combined_crtcs, crtc_id);
  g_list_free (combined_crtcs);

  if (manager_class->get_crtc_gamma)
    {
      manager_class->get_crtc_gamma (manager, crtc, &size, &red, &green, &blue);
    }
  else
    {
      size = 0;
      red = green = blue = NULL;
    }

  red_bytes = g_bytes_new_take (red, size * sizeof (gushort));
  green_bytes = g_bytes_new_take (green, size * sizeof (gushort));
  blue_bytes = g_bytes_new_take (blue, size * sizeof (gushort));

  red_v = g_variant_new_from_bytes (G_VARIANT_TYPE ("aq"), red_bytes, TRUE);
  green_v = g_variant_new_from_bytes (G_VARIANT_TYPE ("aq"), green_bytes, TRUE);
  blue_v = g_variant_new_from_bytes (G_VARIANT_TYPE ("aq"), blue_bytes, TRUE);

  gf_dbus_display_config_complete_get_crtc_gamma (skeleton, invocation,
                                                  red_v, green_v, blue_v);

  g_bytes_unref (red_bytes);
  g_bytes_unref (green_bytes);
  g_bytes_unref (blue_bytes);

  return TRUE;
}

static gboolean
gf_monitor_manager_handle_set_crtc_gamma (GfDBusDisplayConfig   *skeleton,
                                          GDBusMethodInvocation *invocation,
                                          guint                  serial,
                                          guint                  crtc_id,
                                          GVariant              *red_v,
                                          GVariant              *green_v,
                                          GVariant              *blue_v,
                                          GfMonitorManager      *manager)
{
  GfMonitorManagerClass *manager_class;
  GList *combined_crtcs;
  GfCrtc *crtc;
  GBytes *red_bytes;
  GBytes *green_bytes;
  GBytes *blue_bytes;
  gsize size, dummy;
  gushort *red;
  gushort *green;
  gushort *blue;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  combined_crtcs = combine_gpu_lists (manager, gf_gpu_get_crtcs);

  if (crtc_id >= g_list_length (combined_crtcs))
    {
      g_list_free (combined_crtcs);
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid crtc id");
      return TRUE;
    }

  crtc = g_list_nth_data (combined_crtcs, crtc_id);
  g_list_free (combined_crtcs);

  red_bytes = g_variant_get_data_as_bytes (red_v);
  green_bytes = g_variant_get_data_as_bytes (green_v);
  blue_bytes = g_variant_get_data_as_bytes (blue_v);

  size = g_bytes_get_size (red_bytes) / sizeof (gushort);
  red = (gushort*) g_bytes_get_data (red_bytes, &dummy);
  green = (gushort*) g_bytes_get_data (green_bytes, &dummy);
  blue = (gushort*) g_bytes_get_data (blue_bytes, &dummy);

  if (manager_class->set_crtc_gamma)
    manager_class->set_crtc_gamma (manager, crtc, size, red, green, blue);

  gf_dbus_display_config_complete_set_crtc_gamma (skeleton, invocation);

  g_bytes_unref (red_bytes);
  g_bytes_unref (green_bytes);
  g_bytes_unref (blue_bytes);

  return TRUE;
}

#define MODE_FORMAT "(siiddada{sv})"
#define MODES_FORMAT "a" MODE_FORMAT
#define MONITOR_SPEC_FORMAT "(ssss)"
#define MONITOR_FORMAT "(" MONITOR_SPEC_FORMAT MODES_FORMAT "a{sv})"
#define MONITORS_FORMAT "a" MONITOR_FORMAT

#define LOGICAL_MONITOR_MONITORS_FORMAT "a" MONITOR_SPEC_FORMAT
#define LOGICAL_MONITOR_FORMAT "(iidub" LOGICAL_MONITOR_MONITORS_FORMAT "a{sv})"
#define LOGICAL_MONITORS_FORMAT "a" LOGICAL_MONITOR_FORMAT

static gboolean
gf_monitor_manager_handle_get_current_state (GfDBusDisplayConfig   *skeleton,
                                             GDBusMethodInvocation *invocation,
                                             GfMonitorManager      *manager)
{
  GfMonitorManagerPrivate *priv;
  GfSettings *settings;
  GVariantBuilder monitors_builder;
  GVariantBuilder logical_monitors_builder;
  GVariantBuilder properties_builder;
  GList *l;
  GfMonitorManagerCapability capabilities;
  gint ui_scaling_factor;
  gint max_screen_width;
  gint max_screen_height;

  priv = gf_monitor_manager_get_instance_private (manager);
  settings = gf_backend_get_settings (priv->backend);

  g_variant_builder_init (&monitors_builder, G_VARIANT_TYPE (MONITORS_FORMAT));
  g_variant_builder_init (&logical_monitors_builder, G_VARIANT_TYPE (LOGICAL_MONITORS_FORMAT));
  g_variant_builder_init (&properties_builder, G_VARIANT_TYPE ("a{sv}"));

  for (l = manager->monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;
      GfMonitorSpec *monitor_spec = gf_monitor_get_spec (monitor);
      GfMonitorMode *current_mode;
      GfMonitorMode *preferred_mode;
      GVariantBuilder modes_builder;
      GVariantBuilder monitor_properties_builder;
      GList *k;
      gboolean is_builtin;
      const char *display_name;
      gint i;

      current_mode = gf_monitor_get_current_mode (monitor);
      preferred_mode = gf_monitor_get_preferred_mode (monitor);

      g_variant_builder_init (&modes_builder, G_VARIANT_TYPE (MODES_FORMAT));
      for (k = gf_monitor_get_modes (monitor); k; k = k->next)
        {
          GfMonitorMode *monitor_mode = k->data;
          GVariantBuilder supported_scales_builder;
          const gchar *mode_id;
          gint mode_width, mode_height;
          gfloat refresh_rate;
          gfloat preferred_scale;
          gfloat *supported_scales;
          gint n_supported_scales;
          GVariantBuilder mode_properties_builder;
          GfCrtcModeFlag mode_flags;

          if (!gf_monitor_mode_should_be_advertised (monitor_mode))
            continue;

          mode_id = gf_monitor_mode_get_id (monitor_mode);
          gf_monitor_mode_get_resolution (monitor_mode,
                                          &mode_width, &mode_height);

          refresh_rate = gf_monitor_mode_get_refresh_rate (monitor_mode);

          preferred_scale =
            gf_monitor_manager_calculate_monitor_mode_scale (manager,
                                                             monitor,
                                                             monitor_mode);

          g_variant_builder_init (&supported_scales_builder,
                                  G_VARIANT_TYPE ("ad"));
          supported_scales =
            gf_monitor_manager_calculate_supported_scales (manager,
                                                           manager->layout_mode,
                                                           monitor,
                                                           monitor_mode,
                                                           &n_supported_scales);
          for (i = 0; i < n_supported_scales; i++)
            g_variant_builder_add (&supported_scales_builder, "d",
                                   (gdouble) supported_scales[i]);
          g_free (supported_scales);

          mode_flags = gf_monitor_mode_get_flags (monitor_mode);

          g_variant_builder_init (&mode_properties_builder,
                                  G_VARIANT_TYPE ("a{sv}"));
          if (monitor_mode == current_mode)
            g_variant_builder_add (&mode_properties_builder, "{sv}",
                                   "is-current",
                                   g_variant_new_boolean (TRUE));
          if (monitor_mode == preferred_mode)
            g_variant_builder_add (&mode_properties_builder, "{sv}",
                                   "is-preferred",
                                   g_variant_new_boolean (TRUE));
          if (mode_flags & GF_CRTC_MODE_FLAG_INTERLACE)
            g_variant_builder_add (&mode_properties_builder, "{sv}",
                                   "is-interlaced",
                                   g_variant_new_boolean (TRUE));

          g_variant_builder_add (&modes_builder, MODE_FORMAT,
                                 mode_id,
                                 mode_width,
                                 mode_height,
                                 (gdouble) refresh_rate,
                                 (gdouble) preferred_scale,
                                 &supported_scales_builder,
                                 &mode_properties_builder);
        }

      g_variant_builder_init (&monitor_properties_builder,
                              G_VARIANT_TYPE ("a{sv}"));
      if (gf_monitor_supports_underscanning (monitor))
        {
          gboolean is_underscanning = gf_monitor_is_underscanning (monitor);

          g_variant_builder_add (&monitor_properties_builder, "{sv}",
                                 "is-underscanning",
                                 g_variant_new_boolean (is_underscanning));
        }

      is_builtin = gf_monitor_is_laptop_panel (monitor);
      g_variant_builder_add (&monitor_properties_builder, "{sv}",
                             "is-builtin",
                             g_variant_new_boolean (is_builtin));

      display_name = gf_monitor_get_display_name (monitor);
      g_variant_builder_add (&monitor_properties_builder, "{sv}",
                             "display-name",
                             g_variant_new_string (display_name));

      g_variant_builder_add (&monitors_builder, MONITOR_FORMAT,
                             monitor_spec->connector,
                             monitor_spec->vendor,
                             monitor_spec->product,
                             monitor_spec->serial,
                             &modes_builder,
                             &monitor_properties_builder);
    }

  for (l = manager->logical_monitors; l; l = l->next)
    {
      GfLogicalMonitor *logical_monitor = l->data;
      GVariantBuilder logical_monitor_monitors_builder;
      GList *k;

      g_variant_builder_init (&logical_monitor_monitors_builder,
                              G_VARIANT_TYPE (LOGICAL_MONITOR_MONITORS_FORMAT));

      for (k = logical_monitor->monitors; k; k = k->next)
        {
          GfMonitor *monitor = k->data;
          GfMonitorSpec *monitor_spec = gf_monitor_get_spec (monitor);

          g_variant_builder_add (&logical_monitor_monitors_builder,
                                 MONITOR_SPEC_FORMAT,
                                 monitor_spec->connector,
                                 monitor_spec->vendor,
                                 monitor_spec->product,
                                 monitor_spec->serial);
        }

      g_variant_builder_add (&logical_monitors_builder,
                             LOGICAL_MONITOR_FORMAT,
                             logical_monitor->rect.x,
                             logical_monitor->rect.y,
                             (gdouble) logical_monitor->scale,
                             logical_monitor->transform,
                             logical_monitor->is_primary,
                             &logical_monitor_monitors_builder,
                             NULL);
    }

  capabilities = gf_monitor_manager_get_capabilities (manager);

  g_variant_builder_add (&properties_builder, "{sv}",
                         "layout-mode",
                         g_variant_new_uint32 (manager->layout_mode));
  if (capabilities & GF_MONITOR_MANAGER_CAPABILITY_LAYOUT_MODE)
    {
      g_variant_builder_add (&properties_builder, "{sv}",
                             "supports-changing-layout-mode",
                             g_variant_new_boolean (TRUE));
    }

  if (capabilities & GF_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED)
    {
      g_variant_builder_add (&properties_builder, "{sv}",
                             "global-scale-required",
                             g_variant_new_boolean (TRUE));
    }

  ui_scaling_factor = gf_settings_get_ui_scaling_factor (settings);
  g_variant_builder_add (&properties_builder, "{sv}",
                         "legacy-ui-scaling-factor",g_variant_new_int32 (ui_scaling_factor));

  if (gf_monitor_manager_get_max_screen_size (manager,
                                              &max_screen_width,
                                              &max_screen_height))
    {
      GVariantBuilder max_screen_size_builder;

      g_variant_builder_init (&max_screen_size_builder,
                              G_VARIANT_TYPE ("(ii)"));
      g_variant_builder_add (&max_screen_size_builder, "i",
                             max_screen_width);
      g_variant_builder_add (&max_screen_size_builder, "i",
                             max_screen_height);

      g_variant_builder_add (&properties_builder, "{sv}",
                             "max-screen-size",
                             g_variant_builder_end (&max_screen_size_builder));
    }

  gf_dbus_display_config_complete_get_current_state (skeleton, invocation, manager->serial,
                                                     g_variant_builder_end (&monitors_builder),
                                                     g_variant_builder_end (&logical_monitors_builder),
                                                     g_variant_builder_end (&properties_builder));

  return TRUE;
}

#undef MODE_FORMAT
#undef MODES_FORMAT
#undef MONITOR_SPEC_FORMAT
#undef MONITOR_FORMAT
#undef MONITORS_FORMAT
#undef LOGICAL_MONITOR_MONITORS_FORMAT
#undef LOGICAL_MONITOR_FORMAT
#undef LOGICAL_MONITORS_FORMAT

static gboolean
gf_monitor_manager_handle_apply_monitors_config (GfDBusDisplayConfig   *skeleton,
                                                 GDBusMethodInvocation *invocation,
                                                 guint                  serial,
                                                 guint                  method,
                                                 GVariant              *logical_monitor_configs_variant,
                                                 GVariant              *properties_variant,
                                                 GfMonitorManager      *manager)
{
  GfMonitorManagerCapability capabilities;
  GVariant *layout_mode_variant;
  GfLogicalMonitorLayoutMode layout_mode;
  GVariantIter configs_iter;
  GList *logical_monitor_configs;
  GError *error;
  GfMonitorsConfig *config;

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  capabilities = gf_monitor_manager_get_capabilities (manager);
  layout_mode_variant = NULL;
  logical_monitor_configs = NULL;
  error = NULL;

  if (properties_variant)
    {
      layout_mode_variant = g_variant_lookup_value (properties_variant,
                                                    "layout-mode",
                                                    G_VARIANT_TYPE ("u"));
    }

  if (layout_mode_variant &&
      capabilities & GF_MONITOR_MANAGER_CAPABILITY_LAYOUT_MODE)
    {
      g_variant_get (layout_mode_variant, "u", &layout_mode);
    }
  else if (!layout_mode_variant)
    {
      layout_mode = gf_monitor_manager_get_default_layout_mode (manager);
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Can't set layout mode");
      return TRUE;
    }

  if (!is_valid_layout_mode (layout_mode))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "Invalid layout mode specified");
      return TRUE;
    }

  g_variant_iter_init (&configs_iter, logical_monitor_configs_variant);

  while (TRUE)
    {
      GVariant *config_variant;
      GfLogicalMonitorConfig *logical_monitor_config;

      config_variant = g_variant_iter_next_value (&configs_iter);
      if (!config_variant)
        break;

      logical_monitor_config = create_logical_monitor_config_from_variant (manager,
                                                                           config_variant,
                                                                           layout_mode,
                                                                           &error);

      if (!logical_monitor_config)
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "%s", error->message);

          g_error_free (error);
          g_list_free_full (logical_monitor_configs,
                            (GDestroyNotify) gf_logical_monitor_config_free);

          return TRUE;
        }

      logical_monitor_configs = g_list_append (logical_monitor_configs,
                                               logical_monitor_config);
    }

  config = gf_monitors_config_new (manager, logical_monitor_configs,
                                   layout_mode, GF_MONITORS_CONFIG_FLAG_NONE);

  if (!gf_verify_monitors_config (config, manager, &error))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "%s", error->message);

      g_error_free (error);
      g_object_unref (config);

      return TRUE;
    }

  if (!gf_monitor_manager_is_config_applicable (manager, config, &error))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "%s", error->message);

      g_error_free (error);
      g_object_unref (config);

      return TRUE;
    }

  if (method != GF_MONITORS_CONFIG_METHOD_VERIFY)
    cancel_persistent_confirmation (manager);

  if (!gf_monitor_manager_apply_monitors_config (manager, config, method, &error))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "%s", error->message);

      g_error_free (error);
      g_object_unref (config);

      return TRUE;
    }

  if (method == GF_MONITORS_CONFIG_METHOD_PERSISTENT)
    request_persistent_confirmation (manager);

  gf_dbus_display_config_complete_apply_monitors_config (skeleton, invocation);

  return TRUE;
}

static void
monitor_manager_setup_dbus_config_handlers (GfMonitorManager *manager)
{
  g_signal_connect_object (manager->display_config, "handle-get-resources",
                           G_CALLBACK (gf_monitor_manager_handle_get_resources),
                           manager, 0);
  g_signal_connect_object (manager->display_config, "handle-change-backlight",
                           G_CALLBACK (gf_monitor_manager_handle_change_backlight),
                           manager, 0);
  g_signal_connect_object (manager->display_config, "handle-get-crtc-gamma",
                           G_CALLBACK (gf_monitor_manager_handle_get_crtc_gamma),
                           manager, 0);
  g_signal_connect_object (manager->display_config, "handle-set-crtc-gamma",
                           G_CALLBACK (gf_monitor_manager_handle_set_crtc_gamma),
                           manager, 0);
  g_signal_connect_object (manager->display_config, "handle-get-current-state",
                           G_CALLBACK (gf_monitor_manager_handle_get_current_state),
                           manager, 0);
  g_signal_connect_object (manager->display_config, "handle-apply-monitors-config",
                           G_CALLBACK (gf_monitor_manager_handle_apply_monitors_config),
                           manager, 0);
}

static void
bus_acquired_cb (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  GfMonitorManager *manager;
  GDBusInterfaceSkeleton *skeleton;
  GError *error;
  gboolean exported;

  manager = GF_MONITOR_MANAGER (user_data);
  skeleton = G_DBUS_INTERFACE_SKELETON (manager->display_config);

  error = NULL;
  exported = g_dbus_interface_skeleton_export (skeleton,
                                               connection,
                                               "/org/gnome/Mutter/DisplayConfig",
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
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
name_lost_cb (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
}

static GBytes *
gf_monitor_manager_real_read_edid (GfMonitorManager *manager,
                                   GfOutput         *output)
{
  return NULL;
}

static gboolean
gf_monitor_manager_real_is_lid_closed (GfMonitorManager *manager)
{
  if (!manager->up_client)
    return FALSE;

  return manager->lid_is_closed;
}

static void
gf_monitor_manager_real_read_current_state (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;
  GList *l;

  priv = gf_monitor_manager_get_instance_private (manager);

  manager->serial++;

  for (l = gf_backend_get_gpus (priv->backend); l; l = l->next)
    {
      GfGpu *gpu = l->data;
      GError *error = NULL;

      if (!gf_gpu_read_current (gpu, &error))
        {
          g_warning ("Failed to read current KMS state: %s", error->message);
          g_clear_error (&error);
        }
    }

  rebuild_monitors (manager);
}

static void
lid_is_closed_changed (UpClient   *client,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  GfMonitorManager *manager;
  gboolean lid_is_closed;

  manager = user_data;
  lid_is_closed = up_client_get_lid_is_closed (manager->up_client);

  if (lid_is_closed == manager->lid_is_closed)
    return;

  manager->lid_is_closed = lid_is_closed;
  gf_monitor_manager_ensure_configured (manager);
}

static void
gf_monitor_manager_constructed (GObject *object)
{
  GfMonitorManager *manager;
  GfMonitorManagerClass *manager_class;
  GfMonitorManagerPrivate *priv;
  GfOrientationManager *orientation_manager;

  manager = GF_MONITOR_MANAGER (object);
  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);
  priv = gf_monitor_manager_get_instance_private (manager);

  G_OBJECT_CLASS (gf_monitor_manager_parent_class)->constructed (object);

  manager->display_config = gf_dbus_display_config_skeleton_new ();
  monitor_manager_setup_dbus_config_handlers (manager);

  if (manager_class->is_lid_closed == gf_monitor_manager_real_is_lid_closed)
    {
      manager->up_client = up_client_new ();

      if (manager->up_client)
        {
          g_signal_connect_object (manager->up_client, "notify::lid-is-closed",
                                   G_CALLBACK (lid_is_closed_changed), manager, 0);

          manager->lid_is_closed = up_client_get_lid_is_closed (manager->up_client);
        }
    }

  g_signal_connect_object (manager->display_config, "notify::power-save-mode",
                           G_CALLBACK (power_save_mode_changed), manager,
                           G_CONNECT_SWAPPED);

  orientation_manager = gf_backend_get_orientation_manager (priv->backend);
  g_signal_connect_object (orientation_manager, "orientation-changed",
                           G_CALLBACK (orientation_changed), manager, 0);

  g_signal_connect_object (orientation_manager,
                           "notify::has-accelerometer",
                           G_CALLBACK (update_panel_orientation_managed),
                           manager,
                           G_CONNECT_SWAPPED);

  manager->current_switch_config = GF_MONITOR_SWITCH_CONFIG_UNKNOWN;

  priv->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                      "org.gnome.Mutter.DisplayConfig",
                                      G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                      G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                      bus_acquired_cb,
                                      name_acquired_cb,
                                      name_lost_cb,
                                      g_object_ref (manager),
                                      g_object_unref);
}

static void
gf_monitor_manager_dispose (GObject *object)
{
  GfMonitorManager *manager;
  GfMonitorManagerPrivate *priv;

  manager = GF_MONITOR_MANAGER (object);
  priv = gf_monitor_manager_get_instance_private (manager);

  if (priv->bus_name_id != 0)
    {
      g_bus_unown_name (priv->bus_name_id);
      priv->bus_name_id = 0;
    }

  g_clear_object (&manager->display_config);
  g_clear_object (&manager->config_manager);
  g_clear_object (&manager->up_client);

  priv->backend = NULL;

  G_OBJECT_CLASS (gf_monitor_manager_parent_class)->dispose (object);
}

static void
gf_monitor_manager_finalize (GObject *object)
{
  GfMonitorManager *manager;

  manager = GF_MONITOR_MANAGER (object);

  g_list_free_full (manager->logical_monitors, g_object_unref);

  G_OBJECT_CLASS (gf_monitor_manager_parent_class)->finalize (object);
}

static void
gf_monitor_manager_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GfMonitorManager *manager;
  GfMonitorManagerPrivate *priv;

  manager = GF_MONITOR_MANAGER (object);
  priv = gf_monitor_manager_get_instance_private (manager);

  switch (property_id)
    {
      case PROP_BACKEND:
        g_value_set_object (value, priv->backend);
        break;

      case PROP_PANEL_ORIENTATION_MANAGED:
        g_value_set_boolean (value, manager->panel_orientation_managed);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_monitor_manager_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GfMonitorManager *manager;
  GfMonitorManagerPrivate *priv;

  manager = GF_MONITOR_MANAGER (object);
  priv = gf_monitor_manager_get_instance_private (manager);

  switch (property_id)
    {
      case PROP_BACKEND:
        priv->backend = g_value_get_object (value);
        break;

      case PROP_PANEL_ORIENTATION_MANAGED:
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gf_monitor_manager_install_properties (GObjectClass *object_class)
{
  manager_properties[PROP_BACKEND] =
    g_param_spec_object ("backend",
                         "GfBackend",
                         "GfBackend",
                         GF_TYPE_BACKEND,
                         G_PARAM_WRITABLE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

 manager_properties[PROP_PANEL_ORIENTATION_MANAGED] =
    g_param_spec_boolean ("panel-orientation-managed",
                          "Panel orientation managed",
                          "Panel orientation is managed",
                          FALSE,
                          G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     manager_properties);
}

static void
gf_monitor_manager_install_signals (GObjectClass *object_class)
{
  manager_signals[MONITORS_CHANGED] =
    g_signal_new ("monitors-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  manager_signals[POWER_SAVE_MODE_CHANGED] =
    g_signal_new ("power-save-mode-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  manager_signals[CONFIRM_DISPLAY_CHANGE] =
    g_signal_new ("confirm-display-change",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
gf_monitor_manager_class_init (GfMonitorManagerClass *manager_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (manager_class);

  object_class->constructed = gf_monitor_manager_constructed;
  object_class->dispose = gf_monitor_manager_dispose;
  object_class->finalize = gf_monitor_manager_finalize;
  object_class->get_property = gf_monitor_manager_get_property;
  object_class->set_property = gf_monitor_manager_set_property;

  manager_class->read_edid = gf_monitor_manager_real_read_edid;
  manager_class->is_lid_closed = gf_monitor_manager_real_is_lid_closed;
  manager_class->read_current_state = gf_monitor_manager_real_read_current_state;

  gf_monitor_manager_install_properties (object_class);
  gf_monitor_manager_install_signals (object_class);
}

static void
gf_monitor_manager_init (GfMonitorManager *manager)
{
}

GfBackend *
gf_monitor_manager_get_backend (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;

  priv = gf_monitor_manager_get_instance_private (manager);

  return priv->backend;
}

void
gf_monitor_manager_setup (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;
  GfMonitorManagerClass *manager_class;

  priv = gf_monitor_manager_get_instance_private (manager);
  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  priv->in_init = TRUE;

  manager->config_manager = gf_monitor_config_manager_new (manager);

  gf_monitor_manager_read_current_state (manager);
  manager_class->ensure_initial_config (manager);

  priv->in_init = FALSE;
}

void
gf_monitor_manager_rebuild_derived (GfMonitorManager *manager,
                                    GfMonitorsConfig *config)
{
  GfMonitorManagerPrivate *priv;
  GList *old_logical_monitors;

  priv = gf_monitor_manager_get_instance_private (manager);

  gf_monitor_manager_update_monitor_modes_derived (manager);

  if (priv->in_init)
    return;

  old_logical_monitors = manager->logical_monitors;

  gf_monitor_manager_update_logical_state_derived (manager, config);
  gf_monitor_manager_notify_monitors_changed (manager);

  g_list_free_full (old_logical_monitors, g_object_unref);
}

GList *
gf_monitor_manager_get_logical_monitors (GfMonitorManager *manager)
{
  return manager->logical_monitors;
}

GfMonitor *
gf_monitor_manager_get_primary_monitor (GfMonitorManager *manager)
{
  return find_monitor (manager, gf_monitor_is_primary);
}

GfMonitor *
gf_monitor_manager_get_laptop_panel (GfMonitorManager *manager)
{
  return find_monitor (manager, gf_monitor_is_laptop_panel);
}

GfMonitor *
gf_monitor_manager_get_monitor_from_spec (GfMonitorManager *manager,
                                          GfMonitorSpec    *monitor_spec)
{
  GList *l;

  for (l = manager->monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;

      if (gf_monitor_spec_equals (gf_monitor_get_spec (monitor), monitor_spec))
        return monitor;
    }

  return NULL;
}

GList *
gf_monitor_manager_get_monitors (GfMonitorManager *manager)
{
  return manager->monitors;
}

GfLogicalMonitor *
gf_monitor_manager_get_primary_logical_monitor (GfMonitorManager *manager)
{
  return manager->primary_logical_monitor;
}

gboolean
gf_monitor_manager_has_hotplug_mode_update (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;
  GList *gpus;
  GList *l;

  priv = gf_monitor_manager_get_instance_private (manager);

  gpus = gf_backend_get_gpus (priv->backend);

  for (l = gpus; l; l = l->next)
    {
      GfGpu *gpu = l->data;

      if (gf_gpu_has_hotplug_mode_update (gpu))
        return TRUE;
    }

  return FALSE;
}

void
gf_monitor_manager_read_current_state (GfMonitorManager *manager)
{
  return GF_MONITOR_MANAGER_GET_CLASS (manager)->read_current_state (manager);
}

void
gf_monitor_manager_on_hotplug (GfMonitorManager *manager)
{
  gf_monitor_manager_ensure_configured (manager);
}

gboolean
gf_monitor_manager_get_monitor_matrix (GfMonitorManager *manager,
                                       GfMonitor        *monitor,
                                       GfLogicalMonitor *logical_monitor,
                                       gfloat            matrix[6])
{
  GfMonitorTransform transform;
  gfloat viewport[9];

  if (!calculate_viewport_matrix (manager, logical_monitor, viewport))
    return FALSE;

  /* Get transform corrected for LCD panel-orientation. */
  transform = logical_monitor->transform;
  transform = gf_monitor_logical_to_crtc_transform (monitor, transform);
  multiply_matrix (viewport, transform_matrices[transform], matrix);

  return TRUE;
}

void
gf_monitor_manager_tiled_monitor_added (GfMonitorManager *manager,
                                        GfMonitor        *monitor)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  if (manager_class->tiled_monitor_added)
    manager_class->tiled_monitor_added (manager, monitor);
}

void
gf_monitor_manager_tiled_monitor_removed (GfMonitorManager *manager,
                                          GfMonitor        *monitor)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  if (manager_class->tiled_monitor_removed)
    manager_class->tiled_monitor_removed (manager, monitor);
}

gboolean
gf_monitor_manager_is_transform_handled (GfMonitorManager   *manager,
                                         GfCrtc             *crtc,
                                         GfMonitorTransform  transform)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->is_transform_handled (manager, crtc, transform);
}

GfMonitorsConfig *
gf_monitor_manager_ensure_configured (GfMonitorManager *manager)
{
  gboolean use_stored_config;
  GfMonitorsConfigMethod method;
  GfMonitorsConfigMethod fallback_method;
  GfMonitorsConfig *config;
  GError *error;

  use_stored_config = should_use_stored_config (manager);
  if (use_stored_config)
    method = GF_MONITORS_CONFIG_METHOD_PERSISTENT;
  else
    method = GF_MONITORS_CONFIG_METHOD_TEMPORARY;

  fallback_method = GF_MONITORS_CONFIG_METHOD_TEMPORARY;
  config = NULL;
  error = NULL;

  if (use_stored_config)
    {
      config = gf_monitor_config_manager_get_stored (manager->config_manager);

      if (config)
        {
          if (!gf_monitor_manager_apply_monitors_config (manager, config,
                                                         method, &error))
            {
              config = NULL;
              g_warning ("Failed to use stored monitor configuration: %s",
                         error->message);
              g_clear_error (&error);
            }
          else
            {
              g_object_ref (config);
              goto done;
            }
        }
    }

  config = gf_monitor_config_manager_create_suggested (manager->config_manager);
  if (config)
    {
      if (!gf_monitor_manager_apply_monitors_config (manager, config,
                                                     method, &error))
        {
          g_clear_object (&config);
          g_warning ("Failed to use suggested monitor configuration: %s",
                     error->message);
          g_clear_error (&error);
        }
      else
        {
          goto done;
        }
    }

  config = gf_monitor_config_manager_get_previous (manager->config_manager);
  if (config)
    {
      config = g_object_ref (config);

      if (gf_monitor_manager_is_config_complete (manager, config))
        {
          if (!gf_monitor_manager_apply_monitors_config (manager, config,
                                                         method, &error))
            {
              g_warning ("Failed to use suggested monitor configuration: %s",
                         error->message);
              g_clear_error (&error);
            }
          else
            {
              goto done;
            }
        }

      g_clear_object (&config);
    }

  config = gf_monitor_config_manager_create_linear (manager->config_manager);

  if (config)
    {
      if (!gf_monitor_manager_apply_monitors_config (manager, config,
                                                     method, &error))
        {
          g_clear_object (&config);
          g_warning ("Failed to use linear monitor configuration: %s",
                     error->message);
          g_clear_error (&error);
        }
      else
        {
          goto done;
        }
    }

  config = gf_monitor_config_manager_create_fallback (manager->config_manager);
  if (config)
    {
      if (!gf_monitor_manager_apply_monitors_config (manager, config,
                                                     fallback_method,
                                                     &error))
        {
          g_clear_object (&config);
          g_warning ("Failed to use fallback monitor configuration: %s",
                 error->message);
          g_clear_error (&error);
        }
      else
        {
          goto done;
        }
    }

done:
  if (!config)
    {
      if (!gf_monitor_manager_apply_monitors_config (manager, NULL,
                                                     fallback_method,
                                                     &error))
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
        }

      return NULL;
    }

  g_object_unref (config);

  return config;
}

void
gf_monitor_manager_update_logical_state_derived (GfMonitorManager *manager,
                                                 GfMonitorsConfig *config)
{
  if (config)
    manager->current_switch_config = gf_monitors_config_get_switch_config (config);
  else
    manager->current_switch_config = GF_MONITOR_SWITCH_CONFIG_UNKNOWN;

  manager->layout_mode = GF_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;

  gf_monitor_manager_rebuild_logical_monitors_derived (manager, config);
}

gboolean
gf_monitor_manager_is_lid_closed (GfMonitorManager *manager)
{
  return GF_MONITOR_MANAGER_GET_CLASS (manager)->is_lid_closed (manager);
}

gfloat
gf_monitor_manager_calculate_monitor_mode_scale (GfMonitorManager *manager,
                                                 GfMonitor        *monitor,
                                                 GfMonitorMode    *monitor_mode)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->calculate_monitor_mode_scale (manager, monitor, monitor_mode);
}

gfloat *
gf_monitor_manager_calculate_supported_scales (GfMonitorManager           *manager,
                                               GfLogicalMonitorLayoutMode  layout_mode,
                                               GfMonitor                  *monitor,
                                               GfMonitorMode              *monitor_mode,
                                               gint                       *n_supported_scales)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->calculate_supported_scales (manager, layout_mode,
                                                    monitor, monitor_mode,
                                                    n_supported_scales);
}

gboolean
gf_monitor_manager_is_scale_supported (GfMonitorManager           *manager,
                                       GfLogicalMonitorLayoutMode  layout_mode,
                                       GfMonitor                  *monitor,
                                       GfMonitorMode              *monitor_mode,
                                       gfloat                      scale)
{
  gfloat *supported_scales;
  gint n_supported_scales;
  gint i;

  supported_scales = gf_monitor_manager_calculate_supported_scales (manager, layout_mode,
                                                                    monitor, monitor_mode,
                                                                    &n_supported_scales);

  for (i = 0; i < n_supported_scales; i++)
    {
      if (supported_scales[i] == scale)
        {
          g_free (supported_scales);
          return TRUE;
        }
    }

  g_free (supported_scales);
  return FALSE;
}

GfMonitorManagerCapability
gf_monitor_manager_get_capabilities (GfMonitorManager *manager)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->get_capabilities (manager);
}

gboolean
gf_monitor_manager_get_max_screen_size (GfMonitorManager *manager,
                                        gint             *max_width,
                                        gint             *max_height)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->get_max_screen_size (manager, max_width, max_height);
}

GfLogicalMonitorLayoutMode
gf_monitor_manager_get_default_layout_mode (GfMonitorManager *manager)
{
  GfMonitorManagerClass *manager_class;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);

  return manager_class->get_default_layout_mode (manager);
}

GfMonitorConfigManager *
gf_monitor_manager_get_config_manager (GfMonitorManager *manager)
{
  return manager->config_manager;
}

char *
gf_monitor_manager_get_vendor_name (GfMonitorManager *manager,
                                    const char       *vendor)
{
  if (!manager->pnp_ids)
    manager->pnp_ids = gnome_pnp_ids_new ();

  return gnome_pnp_ids_get_pnp_id (manager->pnp_ids, vendor);
}

GfPowerSave
gf_monitor_manager_get_power_save_mode (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;

  priv = gf_monitor_manager_get_instance_private (manager);

  return priv->power_save_mode;
}

void
gf_monitor_manager_power_save_mode_changed (GfMonitorManager *manager,
                                            GfPowerSave       mode)
{
  GfMonitorManagerPrivate *priv;

  priv = gf_monitor_manager_get_instance_private (manager);

  if (priv->power_save_mode == mode)
    return;

  priv->power_save_mode = mode;
  g_signal_emit (manager, manager_signals[POWER_SAVE_MODE_CHANGED], 0);
}

gint
gf_monitor_manager_get_monitor_for_connector (GfMonitorManager *manager,
                                              const gchar      *connector)
{
  GList *l;

  for (l = manager->monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;

      if (gf_monitor_is_active (monitor) &&
          g_str_equal (connector, gf_monitor_get_connector (monitor)))
        return gf_monitor_get_logical_monitor (monitor)->number;
    }

  return -1;
}

gboolean
gf_monitor_manager_get_is_builtin_display_on (GfMonitorManager *manager)
{
  GfMonitor *laptop_panel;

  g_return_val_if_fail (GF_IS_MONITOR_MANAGER (manager), FALSE);

  laptop_panel = gf_monitor_manager_get_laptop_panel (manager);
  if (!laptop_panel)
    return FALSE;

  return gf_monitor_is_active (laptop_panel);
}

GfMonitorSwitchConfigType
gf_monitor_manager_get_switch_config (GfMonitorManager *manager)
{
  return manager->current_switch_config;
}

gboolean
gf_monitor_manager_can_switch_config (GfMonitorManager *manager)
{
  return (!gf_monitor_manager_is_lid_closed (manager) &&
          g_list_length (manager->monitors) > 1);
}

void
gf_monitor_manager_switch_config (GfMonitorManager          *manager,
                                  GfMonitorSwitchConfigType  config_type)
{
  GfMonitorsConfig *config;
  GError *error;

  g_return_if_fail (config_type != GF_MONITOR_SWITCH_CONFIG_UNKNOWN);

  config = gf_monitor_config_manager_create_for_switch_config (manager->config_manager,
                                                               config_type);

  if (!config)
    return;

  error = NULL;
  if (!gf_monitor_manager_apply_monitors_config (manager, config,
                                                 GF_MONITORS_CONFIG_METHOD_TEMPORARY,
                                                 &error))
    {
      g_warning ("Failed to use switch monitor configuration: %s", error->message);
      g_error_free (error);
    }
  else
    {
      manager->current_switch_config = config_type;
    }

  g_object_unref (config);
}

gint
gf_monitor_manager_get_display_configuration_timeout (void)
{
  return DEFAULT_DISPLAY_CONFIGURATION_TIMEOUT;
}

gboolean
gf_monitor_manager_get_panel_orientation_managed (GfMonitorManager *self)
{
  return self->panel_orientation_managed;
}

void
gf_monitor_manager_confirm_configuration (GfMonitorManager *manager,
                                          gboolean          ok)
{
  GfMonitorManagerPrivate *priv;

  priv = gf_monitor_manager_get_instance_private (manager);

  if (!priv->persistent_timeout_id)
    {
      /* too late */
      return;
    }

  cancel_persistent_confirmation (manager);
  confirm_configuration (manager, ok);
}
