/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
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
 * Adapted from mutter:
 * - src/backends/meta-monitor-manager.c
 */

#include "config.h"

#include <glib/gi18n-lib.h>
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
  GfBackend *backend;

  gboolean   in_init;

  guint      bus_name_id;

  guint      persistent_timeout_id;
} GfMonitorManagerPrivate;

typedef gboolean (* MonitorMatchFunc) (GfMonitor *monitor);

enum
{
  PROP_0,

  PROP_BACKEND,

  LAST_PROP
};

static GParamSpec *manager_properties[LAST_PROP] = { NULL };

enum
{
  CONFIRM_DISPLAY_CHANGE,

  LAST_SIGNAL
};

static guint manager_signals[LAST_SIGNAL] = { 0 };

static void gf_monitor_manager_display_config_init (GfDBusDisplayConfigIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GfMonitorManager, gf_monitor_manager, GF_DBUS_TYPE_DISPLAY_CONFIG_SKELETON,
                                  G_ADD_PRIVATE (GfMonitorManager)
                                  G_IMPLEMENT_INTERFACE (GF_DBUS_TYPE_DISPLAY_CONFIG, gf_monitor_manager_display_config_init))

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
  GfMonitorManagerClass *manager_class;
  GfDBusDisplayConfig *display_config;
  gint mode;

  manager_class = GF_MONITOR_MANAGER_GET_CLASS (manager);
  display_config = GF_DBUS_DISPLAY_CONFIG (manager);
  mode = gf_dbus_display_config_get_power_save_mode (display_config);

  if (mode == GF_POWER_SAVE_UNSUPPORTED)
    return;

  /* If DPMS is unsupported, force the property back. */
  if (manager->power_save_mode == GF_POWER_SAVE_UNSUPPORTED)
    {
      gf_dbus_display_config_set_power_save_mode (display_config, GF_POWER_SAVE_UNSUPPORTED);
      return;
    }

  if (manager_class->set_power_save_mode)
    manager_class->set_power_save_mode (manager, mode);

  manager->power_save_mode = mode;
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

  manager->current_switch_config = GF_MONITOR_SWITCH_CONFIG_UNKNOWN;

  gf_backend_monitors_changed (priv->backend);

  g_signal_emit_by_name (manager, "monitors-changed");
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
  GfMonitor *primary_monitor;

  primary_monitor = gf_monitor_manager_get_primary_monitor (manager);
  if (!primary_monitor)
    return 1.0;

  return calculate_monitor_scale (manager, primary_monitor);
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
               scale,
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
  g_variant_lookup (properties_variant, "underscanning", "b", &enable_underscanning);

  g_variant_unref (properties_variant);
  g_free (connector);
  g_free (mode_id);

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

static const gdouble known_diagonals[] =
  {
    12.1,
    13.3,
    15.6
  };

static gchar *
diagonal_to_str (gdouble d)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (known_diagonals); i++)
    {
      gdouble delta;

      delta = fabs(known_diagonals[i] - d);

      if (delta < 0.1)
        return g_strdup_printf ("%0.1lf\"", known_diagonals[i]);
    }

  return g_strdup_printf ("%d\"", (int) (d + 0.5));
}

static gchar *
make_display_name (GfMonitorManager *manager,
                   GfOutput         *output)
{
  gchar *inches;
  gchar *vendor_name;

  if (gf_output_is_laptop (output))
    return g_strdup (_("Built-in display"));

  inches = NULL;
  vendor_name = NULL;

  if (output->width_mm > 0 && output->height_mm > 0)
    {
      gint width_mm;
      gint height_mm;
      gdouble d;

      width_mm = output->width_mm;
      height_mm = output->height_mm;
      d = sqrt (width_mm * width_mm + height_mm * height_mm);

      inches = diagonal_to_str (d / 25.4);
    }

  if (g_strcmp0 (output->vendor, "unknown") != 0)
    {
      if (!manager->pnp_ids)
        manager->pnp_ids = gnome_pnp_ids_new ();

      vendor_name = gnome_pnp_ids_get_pnp_id (manager->pnp_ids, output->vendor);

      if (!vendor_name)
        vendor_name = g_strdup (output->vendor);
    }
  else
    {
      if (inches != NULL)
        vendor_name = g_strdup (_("Unknown"));
      else
        vendor_name = g_strdup (_("Unknown Display"));
    }

  if (inches != NULL)
    {
      gchar *display_name;

      /* TRANSLATORS: this is a monitor vendor name, followed by a
       * size in inches, like 'Dell 15"'
       */
      display_name = g_strdup_printf (_("%s %s"), vendor_name, inches);

      g_free (vendor_name);
      g_free (inches);

      return display_name;
    }

  return vendor_name;
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
  return output->tile_info.loc_h_tile == 0 && output->tile_info.loc_v_tile == 0;
}

static void
rebuild_monitors (GfMonitorManager *manager)
{
  GList *l;

  if (manager->monitors)
    {
      g_list_free_full (manager->monitors, g_object_unref);
      manager->monitors = NULL;
    }

  for (l = manager->outputs; l; l = l->next)
    {
      GfOutput *output = l->data;

      if (output->tile_info.group_id)
        {
          if (is_main_tiled_monitor_output (output))
            {
              GfMonitorTiled *monitor_tiled;

              monitor_tiled = gf_monitor_tiled_new (manager, output);
              manager->monitors = g_list_append (manager->monitors, monitor_tiled);
            }
        }
      else
        {
          GfMonitorNormal *monitor_normal;

          monitor_normal = gf_monitor_normal_new (manager, output);
          manager->monitors = g_list_append (manager->monitors, monitor_normal);
        }
    }
}

static gboolean
gf_monitor_manager_handle_get_resources (GfDBusDisplayConfig   *skeleton,
                                         GDBusMethodInvocation *invocation)
{
  GfMonitorManager *manager;
  GfMonitorManagerClass *manager_class;
  GVariantBuilder crtc_builder;
  GVariantBuilder output_builder;
  GVariantBuilder mode_builder;
  GList *l;
  guint i, j;
  gint max_screen_width;
  gint max_screen_height;

  manager = GF_MONITOR_MANAGER (skeleton);
  manager_class = GF_MONITOR_MANAGER_GET_CLASS (skeleton);

  g_variant_builder_init (&crtc_builder, G_VARIANT_TYPE ("a(uxiiiiiuaua{sv})"));
  g_variant_builder_init (&output_builder, G_VARIANT_TYPE ("a(uxiausauaua{sv})"));
  g_variant_builder_init (&mode_builder, G_VARIANT_TYPE ("a(uxuudu)"));

  for (l = manager->crtcs, i = 0; l; l = l->next, i++)
    {
      GfCrtc *crtc = l->data;
      GVariantBuilder transforms;
      gint current_mode_index;

      g_variant_builder_init (&transforms, G_VARIANT_TYPE ("au"));
      for (j = 0; j <= GF_MONITOR_TRANSFORM_FLIPPED_270; j++)
        if (crtc->all_transforms & (1 << j))
          g_variant_builder_add (&transforms, "u", j);

      if (crtc->current_mode)
        current_mode_index = g_list_index (manager->modes, crtc->current_mode);
      else
        current_mode_index = -1;

      g_variant_builder_add (&crtc_builder, "(uxiiiiiuaua{sv})",
                             i, /* ID */
                             (gint64) crtc->crtc_id,
                             (gint) crtc->rect.x,
                             (gint) crtc->rect.y,
                             (gint) crtc->rect.width,
                             (gint) crtc->rect.height,
                             current_mode_index,
                             (guint32) crtc->transform,
                             &transforms,
                             NULL /* properties */);
    }

  for (l = manager->outputs, i = 0; l; l = l->next, i++)
    {
      GfOutput *output = l->data;
      GVariantBuilder crtcs, modes, clones, properties;
      GBytes *edid;
      gchar *edid_file;
      gint crtc_index;

      g_variant_builder_init (&crtcs, G_VARIANT_TYPE ("au"));
      for (j = 0; j < output->n_possible_crtcs; j++)
        {
          GfCrtc *possible_crtc;
          guint possible_crtc_index;

          possible_crtc = output->possible_crtcs[j];
          possible_crtc_index = g_list_index (manager->crtcs, possible_crtc);

          g_variant_builder_add (&crtcs, "u", possible_crtc_index);
        }

      g_variant_builder_init (&modes, G_VARIANT_TYPE ("au"));
      for (j = 0; j < output->n_modes; j++)
        {
          guint mode_index;

          mode_index = g_list_index (manager->modes, output->modes[j]);
          g_variant_builder_add (&modes, "u", mode_index);

        }

      g_variant_builder_init (&clones, G_VARIANT_TYPE ("au"));
      for (j = 0; j < output->n_possible_clones; j++)
        {
          guint possible_clone_index;

          possible_clone_index = g_list_index (manager->outputs,
                                               output->possible_clones[j]);

          g_variant_builder_add (&clones, "u", possible_clone_index);
        }

      g_variant_builder_init (&properties, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&properties, "{sv}", "vendor",
                             g_variant_new_string (output->vendor));
      g_variant_builder_add (&properties, "{sv}", "product",
                             g_variant_new_string (output->product));
      g_variant_builder_add (&properties, "{sv}", "serial",
                             g_variant_new_string (output->serial));
      g_variant_builder_add (&properties, "{sv}", "width-mm",
                             g_variant_new_int32 (output->width_mm));
      g_variant_builder_add (&properties, "{sv}", "height-mm",
                             g_variant_new_int32 (output->height_mm));
      g_variant_builder_add (&properties, "{sv}", "display-name",
                             g_variant_new_take_string (make_display_name (manager, output)));
      g_variant_builder_add (&properties, "{sv}", "backlight",
                             g_variant_new_int32 (output->backlight));
      g_variant_builder_add (&properties, "{sv}", "min-backlight-step",
                             g_variant_new_int32 ((output->backlight_max - output->backlight_min) ?
                                                  100 / (output->backlight_max - output->backlight_min) : -1));
      g_variant_builder_add (&properties, "{sv}", "primary",
                             g_variant_new_boolean (output->is_primary));
      g_variant_builder_add (&properties, "{sv}", "presentation",
                             g_variant_new_boolean (output->is_presentation));
      g_variant_builder_add (&properties, "{sv}", "connector-type",
                             g_variant_new_string (get_connector_type_name (output->connector_type)));
      g_variant_builder_add (&properties, "{sv}", "underscanning",
                             g_variant_new_boolean (output->is_underscanning));
      g_variant_builder_add (&properties, "{sv}", "supports-underscanning",
                             g_variant_new_boolean (output->supports_underscanning));

      edid_file = manager_class->get_edid_file (manager, output);
      if (edid_file)
        {
          g_variant_builder_add (&properties, "{sv}", "edid-file",
                                 g_variant_new_take_string (edid_file));
        }
      else
        {
          edid = manager_class->read_edid (manager, output);

          if (edid)
            {
              g_variant_builder_add (&properties, "{sv}", "edid",
                                     g_variant_new_from_bytes (G_VARIANT_TYPE ("ay"),
                                                               edid, TRUE));
              g_bytes_unref (edid);
            }
        }

      if (output->tile_info.group_id)
        {
          g_variant_builder_add (&properties, "{sv}", "tile",
                                 g_variant_new ("(uuuuuuuu)",
                                                output->tile_info.group_id,
                                                output->tile_info.flags,
                                                output->tile_info.max_h_tiles,
                                                output->tile_info.max_v_tiles,
                                                output->tile_info.loc_h_tile,
                                                output->tile_info.loc_v_tile,
                                                output->tile_info.tile_w,
                                                output->tile_info.tile_h));
        }

      crtc_index = output->crtc ? g_list_index (manager->crtcs, output->crtc) : -1;

      g_variant_builder_add (&output_builder, "(uxiausauaua{sv})",
                             i, /* ID */
                             (gint64) output->winsys_id,
                             crtc_index,
                             &crtcs,
                             output->name,
                             &modes,
                             &clones,
                             &properties);
    }

  for (l = manager->modes, i = 0; l; l = l->next, i++)
    {
      GfCrtcMode *mode = l->data;

      g_variant_builder_add (&mode_builder, "(uxuudu)",
                             i, /* ID */
                             (gint64) mode->mode_id,
                             (guint32) mode->width,
                             (guint32) mode->height,
                             (gdouble) mode->refresh_rate,
                             (guint32) mode->flags);
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

  return TRUE;
}

static gboolean
gf_monitor_manager_handle_change_backlight (GfDBusDisplayConfig   *skeleton,
                                            GDBusMethodInvocation *invocation,
                                            guint                  serial,
                                            guint                  output_index,
                                            gint                   value)
{
  GfMonitorManager *manager;
  GfMonitorManagerClass *manager_class;
  GfOutput *output;

  manager = GF_MONITOR_MANAGER (skeleton);
  manager_class = GF_MONITOR_MANAGER_GET_CLASS (skeleton);

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  if (output_index >= g_list_length (manager->outputs))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid output id");
      return TRUE;
    }

  output = g_list_nth_data (manager->outputs, output_index);

  if (value < 0 || value > 100)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid backlight value");
      return TRUE;
    }

  if (output->backlight == -1 ||
      (output->backlight_min == 0 && output->backlight_max == 0))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Output does not support changing backlight");
      return TRUE;
    }

  manager_class->change_backlight (manager, output, value);

  gf_dbus_display_config_complete_change_backlight (skeleton, invocation,
                                                    output->backlight);

  return TRUE;
}

static gboolean
gf_monitor_manager_handle_get_crtc_gamma (GfDBusDisplayConfig   *skeleton,
                                          GDBusMethodInvocation *invocation,
                                          guint                  serial,
                                          guint                  crtc_id)
{
  GfMonitorManager *manager;
  GfMonitorManagerClass *manager_class;
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

  manager = GF_MONITOR_MANAGER (skeleton);
  manager_class = GF_MONITOR_MANAGER_GET_CLASS (skeleton);

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  if (crtc_id >= g_list_length (manager->crtcs))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid crtc id");
      return TRUE;
    }

  crtc = g_list_nth_data (manager->crtcs, crtc_id);

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
                                          GVariant              *blue_v)
{
  GfMonitorManager *manager;
  GfMonitorManagerClass *manager_class;
  GfCrtc *crtc;
  GBytes *red_bytes;
  GBytes *green_bytes;
  GBytes *blue_bytes;
  gsize size, dummy;
  gushort *red;
  gushort *green;
  gushort *blue;

  manager = GF_MONITOR_MANAGER (skeleton);
  manager_class = GF_MONITOR_MANAGER_GET_CLASS (skeleton);

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  if (crtc_id >= g_list_length (manager->crtcs))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid crtc id");
      return TRUE;
    }

  crtc = g_list_nth_data (manager->crtcs, crtc_id);

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
                                             GDBusMethodInvocation *invocation)
{
  GfMonitorManager *manager;
  GVariantBuilder monitors_builder;
  GVariantBuilder logical_monitors_builder;
  GVariantBuilder properties_builder;
  GList *l;
  GfMonitorManagerCapability capabilities;
  gint max_screen_width;
  gint max_screen_height;

  manager = GF_MONITOR_MANAGER (skeleton);

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
      GfOutput *main_output;
      gchar *display_name;
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
                                 refresh_rate,
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

      main_output = gf_monitor_get_main_output (monitor);
      display_name = make_display_name (manager, main_output);
      g_variant_builder_add (&monitor_properties_builder, "{sv}",
                             "display-name",
                             g_variant_new_take_string (display_name));

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
  if ((capabilities & GF_MONITOR_MANAGER_CAPABILITY_MIRRORING) == 0)
    {
      g_variant_builder_add (&properties_builder, "{sv}",
                             "supports-mirroring",
                             g_variant_new_boolean (FALSE));
    }

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
                                                 GVariant              *properties_variant)
{
  GfMonitorManager *manager;
  GfMonitorManagerCapability capabilities;
  GVariant *layout_mode_variant;
  GfLogicalMonitorLayoutMode layout_mode;
  GVariantIter configs_iter;
  GList *logical_monitor_configs;
  GError *error;
  GfMonitorsConfig *config;

  manager = GF_MONITOR_MANAGER (skeleton);

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
gf_monitor_manager_display_config_init (GfDBusDisplayConfigIface *iface)
{
  iface->handle_get_resources = gf_monitor_manager_handle_get_resources;
  iface->handle_change_backlight = gf_monitor_manager_handle_change_backlight;
  iface->handle_get_crtc_gamma = gf_monitor_manager_handle_get_crtc_gamma;
  iface->handle_set_crtc_gamma = gf_monitor_manager_handle_set_crtc_gamma;
  iface->handle_get_current_state = gf_monitor_manager_handle_get_current_state;
  iface->handle_apply_monitors_config = gf_monitor_manager_handle_apply_monitors_config;
}

static void
bus_acquired_cb (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  GfMonitorManager *manager;
  GDBusInterfaceSkeleton *skeleton;

  manager = GF_MONITOR_MANAGER (user_data);
  skeleton = G_DBUS_INTERFACE_SKELETON (manager);

  g_dbus_interface_skeleton_export (skeleton, connection,
                                    "/org/gnome/Mutter/DisplayConfig",
                                    NULL);
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

static gchar *
gf_monitor_manager_real_get_edid_file (GfMonitorManager *manager,
                                       GfOutput         *output)
{
  return NULL;
}

static gboolean
gf_monitor_manager_real_is_lid_closed (GfMonitorManager *manager)
{
  if (!manager->up_client)
    return FALSE;

  return up_client_get_lid_is_closed (manager->up_client);
}

static void
lid_is_closed_changed (UpClient   *client,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  GfMonitorManager *manager = user_data;

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

  if (manager_class->is_lid_closed == gf_monitor_manager_real_is_lid_closed)
    {
      manager->up_client = up_client_new ();

      g_signal_connect_object (manager->up_client, "notify::lid-is-closed",
                               G_CALLBACK (lid_is_closed_changed), manager, 0);
    }

  g_signal_connect_object (manager, "notify::power-save-mode",
                           G_CALLBACK (power_save_mode_changed), manager, 0);

  orientation_manager = gf_backend_get_orientation_manager (priv->backend);
  g_signal_connect_object (orientation_manager, "orientation-changed",
                           G_CALLBACK (orientation_changed), manager, 0);

  manager->current_switch_config = GF_MONITOR_SWITCH_CONFIG_UNKNOWN;

  manager->config_manager = gf_monitor_config_manager_new (manager);

  gf_monitor_manager_read_current_state (manager);
  manager_class->ensure_initial_config (manager);

  priv->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                      "org.gnome.Mutter.DisplayConfig",
                                      G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                      G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                      bus_acquired_cb,
                                      name_acquired_cb,
                                      name_lost_cb,
                                      g_object_ref (manager),
                                      g_object_unref);

  priv->in_init = FALSE;
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

  g_list_free_full (manager->outputs, g_object_unref);
  g_list_free_full (manager->modes, g_object_unref);
  g_list_free_full (manager->crtcs, g_object_unref);

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

  g_object_class_install_properties (object_class, LAST_PROP,
                                     manager_properties);
}

static void
gf_monitor_manager_install_signals (GObjectClass *object_class)
{
  manager_signals[CONFIRM_DISPLAY_CHANGE] =
    g_signal_new ("confirm-display-change",
                  G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
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

  manager_class->get_edid_file = gf_monitor_manager_real_get_edid_file;
  manager_class->read_edid = gf_monitor_manager_real_read_edid;
  manager_class->is_lid_closed = gf_monitor_manager_real_is_lid_closed;

  gf_monitor_manager_install_properties (object_class);
  gf_monitor_manager_install_signals (object_class);
}

static void
gf_monitor_manager_init (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;

  priv = gf_monitor_manager_get_instance_private (manager);

  priv->in_init = TRUE;
}

GfBackend *
gf_monitor_manager_get_backend (GfMonitorManager *manager)
{
  GfMonitorManagerPrivate *priv;

  priv = gf_monitor_manager_get_instance_private (manager);

  return priv->backend;
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

GList *
gf_monitor_manager_get_outputs (GfMonitorManager *manager)
{
  return manager->outputs;
}

GList *
gf_monitor_manager_get_crtcs (GfMonitorManager *manager)
{
  return manager->crtcs;
}

gboolean
gf_monitor_manager_has_hotplug_mode_update (GfMonitorManager *manager)
{
  GList *l;

  for (l = manager->outputs; l; l = l->next)
    {
      GfOutput *output = l->data;

      if (output->hotplug_mode_update)
        return TRUE;
    }

  return FALSE;
}

void
gf_monitor_manager_read_current_state (GfMonitorManager *manager)
{
  GList *old_outputs;
  GList *old_crtcs;
  GList *old_modes;

  /* Some implementations of read_current use the existing information
   * we have available, so don't free the old configuration until after
   * read_current finishes.
   */
  old_outputs = manager->outputs;
  old_crtcs = manager->crtcs;
  old_modes = manager->modes;

  manager->serial++;
  GF_MONITOR_MANAGER_GET_CLASS (manager)->read_current (manager);

  rebuild_monitors (manager);

  g_list_free_full (old_outputs, g_object_unref);
  g_list_free_full (old_modes, g_object_unref);
  g_list_free_full (old_crtcs, g_object_unref);
}

void
gf_monitor_manager_on_hotplug (GfMonitorManager *manager)
{
  gf_monitor_manager_ensure_configured (manager);
}

gboolean
gf_monitor_manager_get_monitor_matrix (GfMonitorManager *manager,
                                       GfLogicalMonitor *logical_monitor,
                                       gfloat            matrix[6])
{
  GfMonitorTransform transform;
  gfloat viewport[9];

  if (!calculate_viewport_matrix (manager, logical_monitor, viewport))
    return FALSE;

  transform = logical_monitor->transform;
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
      gf_monitor_manager_apply_monitors_config (manager, NULL,
                                                fallback_method,
                                                &error);
      return NULL;
    }

  g_object_unref (config);

  return config;
}

void
gf_monitor_manager_update_logical_state_derived (GfMonitorManager *manager,
                                                 GfMonitorsConfig *config)
{
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

gint
gf_monitor_manager_get_monitor_for_output (GfMonitorManager *manager,
                                           guint             id)
{
  GfOutput *output;
  GList *l;

  g_return_val_if_fail (GF_IS_MONITOR_MANAGER (manager), -1);
  g_return_val_if_fail (id < g_list_length (manager->outputs), -1);

  output = g_list_nth_data (manager->outputs, id);
  if (!output || !output->crtc)
    return -1;

  for (l = manager->logical_monitors; l; l = l->next)
    {
      GfLogicalMonitor *logical_monitor = l->data;

      if (gf_rectangle_contains_rect (&logical_monitor->rect, &output->crtc->rect))
        return logical_monitor->number;
    }

  return -1;
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
        {
          GfOutput *main_output = gf_monitor_get_main_output (monitor);

          return main_output->crtc->logical_monitor->number;
        }
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
