/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2014 Alberts Muktupāvels
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
 * Adapted from mutter 3.16.0:
 * - /src/backends/meta-monitor-manager.c
 */

#include <config.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>
#include "flashback-confirm-dialog.h"
#include "flashback-display-config.h"
#include "flashback-monitor-config.h"
#include "flashback-monitor-manager.h"

struct _FlashbackDisplayConfig
{
  GObject                  parent;
  gint                     bus_name;
  MetaDBusDisplayConfig   *skeleton;
  FlashbackMonitorManager *manager;
  GtkWidget               *confirm_dialog;
};

G_DEFINE_TYPE (FlashbackDisplayConfig, flashback_display_config, G_TYPE_OBJECT)

static const double known_diagonals[] = { 12.1, 13.3, 15.6 };

static void
power_save_mode_changed (MetaDBusDisplayConfig *skeleton,
                         GParamSpec            *pspec,
                         gpointer               user_data)
{
  FlashbackDisplayConfig *config;
  FlashbackMonitorManager *manager;
  int mode;

  config = FLASHBACK_DISPLAY_CONFIG (user_data);
  manager = config->manager;

  mode = meta_dbus_display_config_get_power_save_mode (skeleton);

  if (mode == META_POWER_SAVE_UNSUPPORTED)
    return;

  /* If DPMS is unsupported, force the property back. */
  if (manager->power_save_mode == META_POWER_SAVE_UNSUPPORTED)
    {
      meta_dbus_display_config_set_power_save_mode (META_DBUS_DISPLAY_CONFIG (manager), META_POWER_SAVE_UNSUPPORTED);
      return;
    }

  flashback_monitor_manager_set_power_save_mode (manager, mode);
  manager->power_save_mode = mode;
}

static void
destroy_confirm_dialog (FlashbackDisplayConfig *dialog)
{
  if (dialog->confirm_dialog != NULL)
    {
      gtk_widget_destroy (GTK_WIDGET (dialog->confirm_dialog));
      dialog->confirm_dialog = NULL;
    }
}

static gboolean
save_config_timeout (gpointer user_data)
{
  FlashbackDisplayConfig *dispay_config;

  dispay_config = FLASHBACK_DISPLAY_CONFIG (user_data);

  destroy_confirm_dialog (dispay_config);

  flashback_monitor_config_restore_previous (dispay_config->manager->monitor_config);

  dispay_config->manager->persistent_timeout_id = 0;
  return G_SOURCE_REMOVE;
}

static void
confirm_dialog_response_cb (FlashbackConfirmDialog *dialog,
                            gint                    response_id,
                            gpointer                user_data)
{
  FlashbackDisplayConfig *config;
  gboolean ok;

  config = FLASHBACK_DISPLAY_CONFIG (user_data);

  switch (response_id)
    {
      case FLASHBACK_CONFIRM_DIALOG_RESPONSE_KEEP_CHANGES:
        ok = TRUE;
        break;
      case FLASHBACK_CONFIRM_DIALOG_RESPONSE_REVERT_SETTINGS:
      default:
        ok = FALSE;
        break;
    }

  destroy_confirm_dialog (config);

  flashback_monitor_manager_confirm_configuration (config->manager, ok);
}

static gboolean
output_can_config (MetaOutput      *output,
                   MetaCRTC        *crtc,
                   MetaMonitorMode *mode)
{
  unsigned int i;
  gboolean ok = FALSE;

  for (i = 0; i < output->n_possible_crtcs && !ok; i++)
    ok = output->possible_crtcs[i] == crtc;

  if (!ok)
    return FALSE;

  if (mode == NULL)
    return TRUE;

  ok = FALSE;
  for (i = 0; i < output->n_modes && !ok; i++)
    ok = output->modes[i] == mode;

  return ok;
}

static gboolean
output_can_clone (MetaOutput *output,
                  MetaOutput *clone)
{
  unsigned int i;
  gboolean ok = FALSE;

  for (i = 0; i < output->n_possible_clones && !ok; i++)
    ok = output->possible_clones[i] == clone;

  return ok;
}

static char *
diagonal_to_str (double d)
{
  unsigned int i;

  for (i = 0; i < G_N_ELEMENTS (known_diagonals); i++)
    {
      double delta;

      delta = fabs(known_diagonals[i] - d);
      if (delta < 0.1)
        return g_strdup_printf ("%0.1lf\"", known_diagonals[i]);
    }

  return g_strdup_printf ("%d\"", (int) (d + 0.5));
}

static char *
make_display_name (FlashbackMonitorManager *manager,
                   MetaOutput              *output)
{
  MetaConnectorType type;
  char *inches = NULL;
  char *vendor_name = NULL;
  char *ret;

  type = output->connector_type;

  if (type == META_CONNECTOR_TYPE_eDP || type == META_CONNECTOR_TYPE_LVDS)
    return g_strdup (_("Built-in display"));

  if (output->width_mm > 0 && output->height_mm > 0)
    {
      double d = sqrt (output->width_mm * output->width_mm +
                       output->height_mm * output->height_mm);
      inches = diagonal_to_str (d / 25.4);
    }

  if (g_strcmp0 (output->vendor, "unknown") != 0)
    {
      if (!manager->pnp_ids)
        manager->pnp_ids = gnome_pnp_ids_new ();

      vendor_name = gnome_pnp_ids_get_pnp_id (manager->pnp_ids,
                                              output->vendor);

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
      /* TRANSLATORS: this is a monitor vendor name, followed by a
       * size in inches, like 'Dell 15"'
       */
      ret = g_strdup_printf (_("%s %s"), vendor_name, inches);
    }
  else
    {
      ret = g_strdup (vendor_name);
    }

  g_free (inches);
  g_free (vendor_name);

  return ret;
}

static const char *
get_connector_type_name (MetaConnectorType connector_type)
{
  switch (connector_type)
    {
      case META_CONNECTOR_TYPE_Unknown:
        return "Unknown";
      case META_CONNECTOR_TYPE_VGA:
        return "VGA";
      case META_CONNECTOR_TYPE_DVII:
        return "DVII";
      case META_CONNECTOR_TYPE_DVID:
        return "DVID";
      case META_CONNECTOR_TYPE_DVIA:
        return "DVIA";
      case META_CONNECTOR_TYPE_Composite:
        return "Composite";
      case META_CONNECTOR_TYPE_SVIDEO:
        return "SVIDEO";
      case META_CONNECTOR_TYPE_LVDS:
        return "LVDS";
      case META_CONNECTOR_TYPE_Component:
        return "Component";
      case META_CONNECTOR_TYPE_9PinDIN:
        return "9PinDIN";
      case META_CONNECTOR_TYPE_DisplayPort:
        return "DisplayPort";
      case META_CONNECTOR_TYPE_HDMIA:
        return "HDMIA";
      case META_CONNECTOR_TYPE_HDMIB:
        return "HDMIB";
      case META_CONNECTOR_TYPE_TV:
        return "TV";
      case META_CONNECTOR_TYPE_eDP:
        return "eDP";
      case META_CONNECTOR_TYPE_VIRTUAL:
        return "VIRTUAL";
      case META_CONNECTOR_TYPE_DSI:
        return "DSI";
      default:
        g_assert_not_reached ();
    }
}

static gboolean
handle_get_resources (MetaDBusDisplayConfig *skeleton,
                      GDBusMethodInvocation *invocation,
                      gpointer               user_data)
{
  FlashbackDisplayConfig *config;
  FlashbackMonitorManager *manager;
  GVariantBuilder crtc_builder;
  GVariantBuilder output_builder;
  GVariantBuilder mode_builder;
  unsigned int i;
  unsigned int j;

  config = FLASHBACK_DISPLAY_CONFIG (user_data);
  manager = config->manager;

  g_variant_builder_init (&crtc_builder, G_VARIANT_TYPE ("a(uxiiiiiuaua{sv})"));
  g_variant_builder_init (&output_builder, G_VARIANT_TYPE ("a(uxiausauaua{sv})"));
  g_variant_builder_init (&mode_builder, G_VARIANT_TYPE ("a(uxuudu)"));

  for (i = 0; i < manager->n_crtcs; i++)
    {
      MetaCRTC *crtc;
      GVariantBuilder transforms;

      crtc = &manager->crtcs[i];

      g_variant_builder_init (&transforms, G_VARIANT_TYPE ("au"));
      for (j = 0; j <= META_MONITOR_TRANSFORM_FLIPPED_270; j++)
        if (crtc->all_transforms & (1 << j))
          g_variant_builder_add (&transforms, "u", j);

      g_variant_builder_add (&crtc_builder, "(uxiiiiiuaua{sv})",
                             i, /* ID */
                             (gint64) crtc->crtc_id,
                             (int) crtc->rect.x,
                             (int) crtc->rect.y,
                             (int) crtc->rect.width,
                             (int) crtc->rect.height,
                             (int) (crtc->current_mode ? crtc->current_mode - manager->modes : -1),
                             (guint32) crtc->transform,
                             &transforms,
                             NULL /* properties */);
    }

  for (i = 0; i < manager->n_outputs; i++)
    {
      MetaOutput *output;
      GVariantBuilder crtcs;
      GVariantBuilder modes;
      GVariantBuilder clones;
      GVariantBuilder properties;
      GBytes *edid;

      output = &manager->outputs[i];

      g_variant_builder_init (&crtcs, G_VARIANT_TYPE ("au"));
      for (j = 0; j < output->n_possible_crtcs; j++)
        g_variant_builder_add (&crtcs, "u",
                               (unsigned)(output->possible_crtcs[j] - manager->crtcs));

      g_variant_builder_init (&modes, G_VARIANT_TYPE ("au"));
      for (j = 0; j < output->n_modes; j++)
        g_variant_builder_add (&modes, "u",
                               (unsigned)(output->modes[j] - manager->modes));

      g_variant_builder_init (&clones, G_VARIANT_TYPE ("au"));
      for (j = 0; j < output->n_possible_clones; j++)
        g_variant_builder_add (&clones, "u",
                               (unsigned)(output->possible_clones[j] - manager->outputs));

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

      edid = flashback_monitor_manager_read_edid (manager, output);

      if (edid)
        {
          g_variant_builder_add (&properties, "{sv}", "edid",
                                 g_variant_new_from_bytes (G_VARIANT_TYPE ("ay"),
                                                           edid, TRUE));
          g_bytes_unref (edid);
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

      g_variant_builder_add (&output_builder, "(uxiausauaua{sv})",
                             i, /* ID */
                             (gint64) output->winsys_id,
                             (int) (output->crtc ? output->crtc - manager->crtcs : -1),
                             &crtcs,
                             output->name,
                             &modes,
                             &clones,
                             &properties);
    }

  for (i = 0; i < manager->n_modes; i++)
    {
      MetaMonitorMode *mode;

      mode = &manager->modes[i];

      g_variant_builder_add (&mode_builder, "(uxuudu)",
                             i, /* ID */
                             (gint64) mode->mode_id,
                             (guint32) mode->width,
                             (guint32) mode->height,
                             (double) mode->refresh_rate,
                             (guint32) mode->flags);
    }

  meta_dbus_display_config_complete_get_resources (skeleton,
                                                   invocation,
                                                   manager->serial,
                                                   g_variant_builder_end (&crtc_builder),
                                                   g_variant_builder_end (&output_builder),
                                                   g_variant_builder_end (&mode_builder),
                                                   manager->max_screen_width,
                                                   manager->max_screen_height);

  return TRUE;
}

static gboolean
handle_apply_configuration (MetaDBusDisplayConfig *skeleton,
                            GDBusMethodInvocation *invocation,
                            guint                  serial,
                            gboolean               persistent,
                            GVariant              *crtcs,
                            GVariant              *outputs,
                            gpointer               user_data)
{
  FlashbackDisplayConfig *config;
  FlashbackMonitorManager *manager;
  GVariantIter crtc_iter;
  GVariantIter output_iter;
  GVariantIter *nested_outputs;
  GVariant *properties;
  guint crtc_id;
  int new_mode;
  int x;
  int y;
  int new_screen_width;
  int new_screen_height;
  gint transform;
  guint output_index;
  GPtrArray *crtc_infos;
  GPtrArray *output_infos;

  config = FLASHBACK_DISPLAY_CONFIG (user_data);
  manager = config->manager;

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  crtc_infos = g_ptr_array_new_full (g_variant_n_children (crtcs),
                                     (GDestroyNotify) meta_crtc_info_free);
  output_infos = g_ptr_array_new_full (g_variant_n_children (outputs),
                                       (GDestroyNotify) meta_output_info_free);

  /* Validate all arguments */
  new_screen_width = 0; new_screen_height = 0;
  g_variant_iter_init (&crtc_iter, crtcs);
  while (g_variant_iter_loop (&crtc_iter, "(uiiiuaua{sv})",
                              &crtc_id, &new_mode, &x, &y, &transform,
                              &nested_outputs, NULL))
    {
      MetaCRTCInfo *crtc_info;
      MetaOutput *first_output;
      MetaCRTC *crtc;
      MetaMonitorMode *mode;

      crtc_info = g_slice_new (MetaCRTCInfo);
      crtc_info->outputs = g_ptr_array_new ();

      if (crtc_id >= manager->n_crtcs)
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "Invalid CRTC id");
          return TRUE;
        }

      crtc = &manager->crtcs[crtc_id];
      crtc_info->crtc = crtc;

      if (new_mode != -1 && (new_mode < 0 || (unsigned) new_mode >= manager->n_modes))
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "Invalid mode id");
          return TRUE;
        }

      mode = new_mode != -1 ? &manager->modes[new_mode] : NULL;
      crtc_info->mode = mode;

      if (mode)
        {
          int width;
          int height;

          if (transform % 2)
            {
              width = mode->height;
              height = mode->width;
            }
          else
            {
              width = mode->width;
              height = mode->height;
            }

          if (x < 0 ||
              x + width > manager->max_screen_width ||
              y < 0 ||
              y + height > manager->max_screen_height)
            {
              g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                     G_DBUS_ERROR_INVALID_ARGS,
                                                     "Invalid CRTC geometry");
              return TRUE;
            }

          new_screen_width = MAX (new_screen_width, x + width);
          new_screen_height = MAX (new_screen_height, y + height);
          crtc_info->x = x;
          crtc_info->y = y;
        }
      else
        {
          crtc_info->x = 0;
          crtc_info->y = 0;
        }

      if (transform < META_MONITOR_TRANSFORM_NORMAL ||
          transform > META_MONITOR_TRANSFORM_FLIPPED_270 ||
          ((crtc->all_transforms & (1 << transform)) == 0))
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "Invalid transform");
          return TRUE;
        }

      crtc_info->transform = transform;

      first_output = NULL;
      while (g_variant_iter_loop (nested_outputs, "u", &output_index))
        {
          MetaOutput *output;

          if (output_index >= manager->n_outputs)
            {
              g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                     G_DBUS_ERROR_INVALID_ARGS,
                                                     "Invalid output id");
              return TRUE;
            }

          output = &manager->outputs[output_index];

          if (!output_can_config (output, crtc, mode))
            {
              g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                     G_DBUS_ERROR_INVALID_ARGS,
                                                     "Output cannot be assigned to this CRTC or mode");
              return TRUE;
            }

          g_ptr_array_add (crtc_info->outputs, output);

          if (first_output)
            {
              if (!output_can_clone (output, first_output))
                {
                  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                         G_DBUS_ERROR_INVALID_ARGS,
                                                         "Outputs cannot be cloned");
                  return TRUE;
                }
            }
          else
            first_output = output;
        }

      if (!first_output && mode)
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "Mode specified without outputs?");
          return TRUE;
        }

      g_ptr_array_add (crtc_infos, crtc_info);
    }

  if (new_screen_width == 0 || new_screen_height == 0)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Refusing to disable all outputs");
      return TRUE;
    }

  g_variant_iter_init (&output_iter, outputs);
  while (g_variant_iter_loop (&output_iter, "(u@a{sv})", &output_index, &properties))
    {
      MetaOutputInfo *output_info;
      gboolean primary;
      gboolean presentation;
      gboolean underscanning;

      if (output_index >= manager->n_outputs)
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "Invalid output id");
          return TRUE;
        }

      output_info = g_slice_new0 (MetaOutputInfo);
      output_info->output = &manager->outputs[output_index];

      if (g_variant_lookup (properties, "primary", "b", &primary))
        output_info->is_primary = primary;

      if (g_variant_lookup (properties, "presentation", "b", &presentation))
        output_info->is_presentation = presentation;

      if (g_variant_lookup (properties, "underscanning", "b", &underscanning))
        output_info->is_underscanning = underscanning;

      g_ptr_array_add (output_infos, output_info);
    }

  /* If we were in progress of making a persistent change and we see a
     new request, it's likely that the old one failed in some way, so
     don't save it, but also don't queue for restoring it.
  */
  if (manager->persistent_timeout_id && persistent)
    {
      g_source_remove (manager->persistent_timeout_id);
      manager->persistent_timeout_id = 0;
    }

  flashback_monitor_manager_apply_configuration (manager,
                                                 (MetaCRTCInfo **) crtc_infos->pdata,
                                                 crtc_infos->len,
                                                 (MetaOutputInfo **) output_infos->pdata,
                                                 output_infos->len);

  g_ptr_array_unref (crtc_infos);
  g_ptr_array_unref (output_infos);

  /* Update MetaMonitorConfig data structures immediately so that we
     don't revert the change at the next XRandR event, then ask the plugin
     manager (through MetaScreen) to confirm the display change with the
     appropriate UI. Then wait 20 seconds and if not confirmed, revert the
     configuration.
  */
  flashback_monitor_config_update_current (manager->monitor_config);

  if (persistent)
    {
      manager->persistent_timeout_id = g_timeout_add_seconds (20, save_config_timeout, config);
      g_source_set_name_by_id (manager->persistent_timeout_id, "[gnome-flashback] save_config_timeout");

      config->confirm_dialog = flashback_confirm_dialog_new (20);
      g_signal_connect (config->confirm_dialog, "response",
                        G_CALLBACK (confirm_dialog_response_cb), config);

      gtk_window_present (GTK_WINDOW (config->confirm_dialog));
    }

  meta_dbus_display_config_complete_apply_configuration (skeleton,
                                                         invocation);

  return TRUE;
}

static gboolean
handle_change_backlight (MetaDBusDisplayConfig *skeleton,
                         GDBusMethodInvocation *invocation,
                         guint                  serial,
                         guint                  output_index,
                         gint                   value,
                         gpointer               user_data)
{
  FlashbackDisplayConfig *config;
  FlashbackMonitorManager *manager;
  MetaOutput *output;

  config = FLASHBACK_DISPLAY_CONFIG (user_data);
  manager = config->manager;

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  if (output_index >= manager->n_outputs)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid output id");
      return TRUE;
    }

  output = &manager->outputs[output_index];

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

  flashback_monitor_manager_change_backlight (manager, output, value);

  meta_dbus_display_config_complete_change_backlight (skeleton,
                                                      invocation,
                                                      output->backlight);

  return TRUE;
}

static gboolean
handle_get_crtc_gamma (MetaDBusDisplayConfig *skeleton,
                       GDBusMethodInvocation *invocation,
                       guint                  serial,
                       guint                  crtc_id,
                       gpointer               user_data)
{
  FlashbackDisplayConfig *config;
  FlashbackMonitorManager *manager;
  MetaCRTC *crtc;
  gsize size;
  unsigned short *red;
  unsigned short *green;
  unsigned short *blue;
  GBytes *red_bytes;
  GBytes *green_bytes;
  GBytes *blue_bytes;
  GVariant *red_v;
  GVariant *green_v;
  GVariant *blue_v;

  config = FLASHBACK_DISPLAY_CONFIG (user_data);
  manager = config->manager;

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  if (crtc_id >= manager->n_crtcs)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid crtc id");
      return TRUE;
    }

  crtc = &manager->crtcs[crtc_id];

  flashback_monitor_manager_get_crtc_gamma (manager, crtc, &size, &red, &green, &blue);

  red_bytes = g_bytes_new_take (red, size * sizeof (unsigned short));
  green_bytes = g_bytes_new_take (green, size * sizeof (unsigned short));
  blue_bytes = g_bytes_new_take (blue, size * sizeof (unsigned short));

  red_v = g_variant_new_from_bytes (G_VARIANT_TYPE ("aq"), red_bytes, TRUE);
  green_v = g_variant_new_from_bytes (G_VARIANT_TYPE ("aq"), green_bytes, TRUE);
  blue_v = g_variant_new_from_bytes (G_VARIANT_TYPE ("aq"), blue_bytes, TRUE);

  g_bytes_unref (red_bytes);
  g_bytes_unref (green_bytes);
  g_bytes_unref (blue_bytes);

  meta_dbus_display_config_complete_get_crtc_gamma (skeleton,
                                                    invocation,
                                                    red_v,
                                                    green_v,
                                                    blue_v);

  return TRUE;
}

static gboolean
handle_set_crtc_gamma (MetaDBusDisplayConfig *skeleton,
                       GDBusMethodInvocation *invocation,
                       guint                  serial,
                       guint                  crtc_id,
                       GVariant              *red_v,
                       GVariant              *green_v,
                       GVariant              *blue_v,
                       gpointer               user_data)
{
  FlashbackDisplayConfig *config;
  FlashbackMonitorManager *manager;
  MetaCRTC *crtc;
  gsize size;
  gsize dummy;
  unsigned short *red;
  unsigned short *green;
  unsigned short *blue;
  GBytes *red_bytes;
  GBytes *green_bytes;
  GBytes *blue_bytes;

  config = FLASHBACK_DISPLAY_CONFIG (user_data);
  manager = config->manager;

  if (serial != manager->serial)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "The requested configuration is based on stale information");
      return TRUE;
    }

  if (crtc_id >= manager->n_crtcs)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid crtc id");
      return TRUE;
    }

  crtc = &manager->crtcs[crtc_id];

  red_bytes = g_variant_get_data_as_bytes (red_v);
  green_bytes = g_variant_get_data_as_bytes (green_v);
  blue_bytes = g_variant_get_data_as_bytes (blue_v);

  size = g_bytes_get_size (red_bytes) / sizeof (unsigned short);
  red = (unsigned short*) g_bytes_get_data (red_bytes, &dummy);
  green = (unsigned short*) g_bytes_get_data (green_bytes, &dummy);
  blue = (unsigned short*) g_bytes_get_data (blue_bytes, &dummy);

  flashback_monitor_manager_set_crtc_gamma (manager, crtc, size, red, green, blue);

  g_bytes_unref (red_bytes);
  g_bytes_unref (green_bytes);
  g_bytes_unref (blue_bytes);

  meta_dbus_display_config_complete_set_crtc_gamma (skeleton,
                                                    invocation);

  return TRUE;
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  FlashbackDisplayConfig *config;
  GDBusInterfaceSkeleton *iface;
  GError *error;

  config = FLASHBACK_DISPLAY_CONFIG (user_data);

  g_signal_connect (config->skeleton, "handle-get-resources",
                    G_CALLBACK (handle_get_resources), config);
  g_signal_connect (config->skeleton, "handle-apply-configuration",
                    G_CALLBACK (handle_apply_configuration), config);
  g_signal_connect (config->skeleton, "handle-change-backlight",
                    G_CALLBACK (handle_change_backlight), config);
  g_signal_connect (config->skeleton, "handle-get-crtc-gamma",
                    G_CALLBACK (handle_get_crtc_gamma), config);
  g_signal_connect (config->skeleton, "handle-set-crtc-gamma",
                    G_CALLBACK (handle_set_crtc_gamma), config);

  g_signal_connect (config->skeleton, "notify::power-save-mode",
                    G_CALLBACK (power_save_mode_changed), config);

  iface = G_DBUS_INTERFACE_SKELETON (config->skeleton);
  error = NULL;

  if (!g_dbus_interface_skeleton_export (iface, connection,
                                         "/org/gnome/Mutter/DisplayConfig",
                                         &error))
    {
      g_warning ("Failed to export interface: %s", error->message);
      g_error_free (error);
      return;
    }
}

static void
on_name_acquired (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const char      *name,
              gpointer         user_data)
{
}

static void
flashback_display_config_finalize (GObject *object)
{
  FlashbackDisplayConfig *config;

  config = FLASHBACK_DISPLAY_CONFIG (object);

  if (config->skeleton)
    {
      g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (config->skeleton));
      g_clear_object (&config->skeleton);
    }

  if (config->bus_name)
    {
      g_bus_unown_name (config->bus_name);
      config->bus_name = 0;
    }

  destroy_confirm_dialog (config);

  g_clear_object (&config->manager);

  G_OBJECT_CLASS (flashback_display_config_parent_class)->finalize (object);
}

static void
flashback_display_config_class_init (FlashbackDisplayConfigClass *config_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (config_class);

  object_class->finalize = flashback_display_config_finalize;
}

static void
flashback_display_config_init (FlashbackDisplayConfig *config)
{
  config->skeleton = meta_dbus_display_config_skeleton_new ();
  config->manager = flashback_monitor_manager_new (config->skeleton);
  config->bus_name = g_bus_own_name (G_BUS_TYPE_SESSION,
                                     "org.gnome.Mutter.DisplayConfig",
                                     G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                     G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                     on_bus_acquired,
                                     on_name_acquired,
                                     on_name_lost,
                                     config,
                                     NULL);
}

FlashbackDisplayConfig *
flashback_display_config_new (void)
{
  return g_object_new (FLASHBACK_TYPE_DISPLAY_CONFIG,
                       NULL);
}

/**
 * flashback_display_config_get_monitor_manager:
 * @config: a #FlashbackMonitorManager
 *
 * Returns: (transfer none):
 */
FlashbackMonitorManager *
flashback_display_config_get_monitor_manager (FlashbackDisplayConfig *config)
{
  if (config == NULL)
    return NULL;

  return config->manager;
}
