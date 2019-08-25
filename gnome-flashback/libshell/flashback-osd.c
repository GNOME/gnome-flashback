/*
 * Copyright (C) 2015-2019 Alberts Muktupāvels
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

#include <config.h>
#include "flashback-osd.h"
#include "gf-osd-window.h"

struct _FlashbackOsd
{
  GObject       parent;

  GfOsdWindow **windows;
  gint          n_monitors;

  gulong        monitors_changed_id;
};

G_DEFINE_TYPE (FlashbackOsd, flashback_osd, G_TYPE_OBJECT)

static void
monitors_changed (GdkScreen *screen,
                  gpointer   user_data)
{
  FlashbackOsd *osd;
  GdkDisplay *display;
  gint n_monitors;
  gint i;

  osd = FLASHBACK_OSD (user_data);

  display = gdk_display_get_default ();
  n_monitors = gdk_display_get_n_monitors (display);

  if (osd->windows != NULL)
    {
      for (i = 0; i < osd->n_monitors; i++)
        gtk_widget_destroy (GTK_WIDGET (osd->windows[i]));
      g_free (osd->windows);
    }

  osd->windows = g_new0 (GfOsdWindow *, n_monitors);

  for (i = 0; i < n_monitors; i++)
    osd->windows[i] = gf_osd_window_new (i);

  osd->n_monitors = n_monitors;
}

static void
flashabck_osd_finalize (GObject *object)
{
  GdkScreen *screen;
  FlashbackOsd *osd;

  screen = gdk_screen_get_default ();
  osd = FLASHBACK_OSD (object);

  g_signal_handler_disconnect (screen, osd->monitors_changed_id);

  if (osd->windows != NULL)
    {
      gint i;

      for (i = 0; i < osd->n_monitors; i++)
        gtk_widget_destroy (GTK_WIDGET (osd->windows[i]));
      g_free (osd->windows);
    }

  G_OBJECT_CLASS (flashback_osd_parent_class)->finalize (object);
}

static void
flashback_osd_class_init (FlashbackOsdClass *osd_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (osd_class);

  object_class->finalize = flashabck_osd_finalize;
}

static void
flashback_osd_init (FlashbackOsd *osd)
{
  GdkScreen *screen;

  screen = gdk_screen_get_default ();

  osd->monitors_changed_id =
    g_signal_connect (screen, "monitors-changed",
                      G_CALLBACK (monitors_changed), osd);

  monitors_changed (screen, osd);
}

FlashbackOsd *
flashback_osd_new (void)
{
  return g_object_new (FLASHBACK_TYPE_OSD, NULL);
}

void
flashback_osd_show (FlashbackOsd     *osd,
                    GfMonitorManager *monitor_manager,
                    GVariant         *params)
{
  GVariantDict dict;
  const gchar *icon_name;
  const gchar *label;
  GIcon *icon;
  gdouble level;
  gdouble max_level;
  const gchar *connector;
  gint monitor;
  gint i;

  g_variant_dict_init (&dict, params);

  if (!g_variant_dict_lookup (&dict, "icon", "&s", &icon_name))
    icon_name = NULL;

  if (!g_variant_dict_lookup (&dict, "label", "&s", &label))
    label = NULL;

  if (!g_variant_dict_lookup (&dict, "level", "d", &level))
    level = -1;

  if (!g_variant_dict_lookup (&dict, "max_level", "d", &max_level))
    max_level = 1.0;

  if (!g_variant_dict_lookup (&dict, "connector", "&s", &connector))
    connector = NULL;

  monitor = -1;
  if (connector != NULL)
    monitor = gf_monitor_manager_get_monitor_for_connector (monitor_manager, connector);

  icon = NULL;
  if (icon_name)
    icon = g_icon_new_for_string (icon_name, NULL);

  if (monitor != -1)
    {
      for (i = 0; i < osd->n_monitors; i++)
        {
          if (i == monitor)
            {
              gf_osd_window_set_icon (osd->windows[i], icon);
              gf_osd_window_set_label (osd->windows[i], label);
              gf_osd_window_set_level (osd->windows[i], level);
              gf_osd_window_set_max_level (osd->windows[i], max_level);
              gf_osd_window_show (osd->windows[i]);
            }
          else
            {
              gf_osd_window_hide (osd->windows[i]);
            }
        }
    }
  else
    {
      for (i = 0; i < osd->n_monitors; i++)
        {
          gf_osd_window_set_icon (osd->windows[i], icon);
          gf_osd_window_set_label (osd->windows[i], label);
          gf_osd_window_set_level (osd->windows[i], level);
          gf_osd_window_set_max_level (osd->windows[i], max_level);
          gf_osd_window_show (osd->windows[i]);
        }
    }

  if (icon)
    g_object_unref (icon);
}
