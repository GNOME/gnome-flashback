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

typedef struct
{
  GfBackend *backend;

  gboolean   in_init;

  guint      bus_name_id;
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

        }
    }

  return TRUE;
}

static gboolean
gf_monitor_manager_is_config_complete (GfMonitorManager *manager,
                                       GfMonitorsConfig *config)
{
  GList *l;
  guint configured_monitor_count = 0;
  guint expected_monitor_count = 0;

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      GfLogicalMonitorConfig *logical_monitor_config = l->data;
      GList *k;

      for (k = logical_monitor_config->monitor_configs; k; k = k->next)
        configured_monitor_count++;
    }

  for (l = manager->monitors; l; l = l->next)
    {
      GfMonitor *monitor = l->data;

      if (gf_monitor_is_laptop_panel (monitor))
        {
          if (!gf_monitor_manager_is_lid_closed (manager))
            expected_monitor_count++;
        }
      else
        {
          expected_monitor_count++;
        }
    }

  if (configured_monitor_count != expected_monitor_count)
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

static gboolean
is_main_tiled_monitor_output (GfOutput *output)
{
  return output->tile_info.loc_h_tile == 0 && output->tile_info.loc_v_tile == 0;
}

static void
rebuild_monitors (GfMonitorManager *manager)
{
  guint i;

  if (manager->monitors)
    {
      g_list_free_full (manager->monitors, g_object_unref);
      manager->monitors = NULL;
    }

  for (i = 0; i < manager->n_outputs; i++)
    {
      GfOutput *output = &manager->outputs[i];

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

static void
free_output_array (GfOutput *old_outputs,
                   gint      n_old_outputs)
{
  gint i;

  for (i = 0; i < n_old_outputs; i++)
    gf_monitor_manager_clear_output (&old_outputs[i]);

  g_free (old_outputs);
}

static void
free_mode_array (GfCrtcMode *old_modes,
                 gint        n_old_modes)
{
  gint i;

  for (i = 0; i < n_old_modes; i++)
    gf_monitor_manager_clear_mode (&old_modes[i]);

  g_free (old_modes);
}

static void
free_crtc_array (GfCrtc *old_crtcs,
                 gint    n_old_crtcs)
{
  gint i;

  for (i = 0; i < n_old_crtcs; i++)
    gf_monitor_manager_clear_crtc (&old_crtcs[i]);

  g_free (old_crtcs);
}

static gboolean
gf_monitor_manager_handle_get_resources (GfDBusDisplayConfig   *skeleton,
                                         GDBusMethodInvocation *invocation)
{
  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "Not implemented");

  return TRUE;
}

static gboolean
gf_monitor_manager_handle_change_backlight (GfDBusDisplayConfig   *skeleton,
                                            GDBusMethodInvocation *invocation,
                                            guint                  serial,
                                            guint                  output_index,
                                            gint                   value)
{
  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "Not implemented");

  return TRUE;
}

static gboolean
gf_monitor_manager_handle_get_crtc_gamma (GfDBusDisplayConfig   *skeleton,
                                          GDBusMethodInvocation *invocation,
                                          guint                  serial,
                                          guint                  crtc_id)
{
  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "Not implemented");

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
  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "Not implemented");

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
  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "Not implemented");

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

gboolean
gf_monitor_manager_has_hotplug_mode_update (GfMonitorManager *manager)
{
  guint i;

  for (i = 0; i < manager->n_outputs; i++)
    {
      GfOutput *output = &manager->outputs[i];

      if (output->hotplug_mode_update)
        return TRUE;
    }

  return FALSE;
}

void
gf_monitor_manager_read_current_state (GfMonitorManager *manager)
{
  GfOutput *old_outputs;
  GfCrtc *old_crtcs;
  GfCrtcMode *old_modes;
  guint n_old_outputs;
  guint n_old_crtcs;
  guint n_old_modes;

  /* Some implementations of read_current use the existing information
   * we have available, so don't free the old configuration until after
   * read_current finishes.
   */
  old_outputs = manager->outputs;
  n_old_outputs = manager->n_outputs;
  old_crtcs = manager->crtcs;
  n_old_crtcs = manager->n_crtcs;
  old_modes = manager->modes;
  n_old_modes = manager->n_modes;

  manager->serial++;
  GF_MONITOR_MANAGER_GET_CLASS (manager)->read_current (manager);

  rebuild_monitors (manager);

  free_output_array (old_outputs, n_old_outputs);
  free_mode_array (old_modes, n_old_modes);
  free_crtc_array (old_crtcs, n_old_crtcs);
}

void
gf_monitor_manager_on_hotplug (GfMonitorManager *manager)
{
  gf_monitor_manager_ensure_configured (manager);
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

void
gf_monitor_manager_clear_output (GfOutput *output)
{
  g_free (output->name);
  g_free (output->vendor);
  g_free (output->product);
  g_free (output->serial);
  g_free (output->modes);
  g_free (output->possible_crtcs);
  g_free (output->possible_clones);

  if (output->driver_notify)
    output->driver_notify (output);

  memset (output, 0, sizeof (*output));
}

void
gf_monitor_manager_clear_mode (GfCrtcMode *mode)
{
  g_free (mode->name);

  if (mode->driver_notify)
    mode->driver_notify (mode);

  memset (mode, 0, sizeof (*mode));
}

void
gf_monitor_manager_clear_crtc (GfCrtc *crtc)
{
  if (crtc->driver_notify)
    crtc->driver_notify (crtc);

  memset (crtc, 0, sizeof (*crtc));
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

gboolean
gf_monitor_manager_can_switch_config (GfMonitorManager *manager)
{
  return (!gf_monitor_manager_is_lid_closed (manager) &&
          g_list_length (manager->monitors) > 1);
}
