/*
 * Copyright (C) 2015 Alberts Muktupāvels
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
#include "flashback-osd-window.h"

struct _FlashbackOsd
{
  GObject              parent;

  FlashbackOsdWindow **windows;
  gint                 n_monitors;
};

G_DEFINE_TYPE (FlashbackOsd, flashback_osd, G_TYPE_OBJECT)

static void
monitors_changed (GdkScreen *screen,
                  gpointer   user_data)
{
  FlashbackOsd *osd;
  gint n_monitors;
  gint i;

  osd = FLASHBACK_OSD (user_data);
  n_monitors = gdk_screen_get_n_monitors (screen);

  if (osd->windows != NULL)
    {
      for (i = 0; i < osd->n_monitors; i++)
        gtk_widget_destroy (GTK_WIDGET (osd->windows[i]));
      g_free (osd->windows);
    }

  osd->windows = g_new0 (FlashbackOsdWindow *, n_monitors);

  for (i = 0; i < n_monitors; i++)
    osd->windows[i] = flashback_osd_window_new (i);

  osd->n_monitors = n_monitors;
}

static void
flashabck_osd_finalize (GObject *object)
{
  GdkScreen *screen;
  FlashbackOsd *osd;

  screen = gdk_screen_get_default ();
  osd = FLASHBACK_OSD (object);

  g_signal_handlers_disconnect_by_func (screen, monitors_changed, osd);

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
flashback_osd_show (FlashbackOsd *osd,
                    GVariant     *params)
{
  GVariantDict dict;
  const gchar *icon_name;
  const gchar *label;
  GIcon *icon;
  gint level;
  gint monitor;
  gint i;

  g_variant_dict_init (&dict, params);

  if (!g_variant_dict_lookup (&dict, "icon", "&s", &icon_name))
    icon_name = NULL;

  if (!g_variant_dict_lookup (&dict, "label", "&s", &label))
    label = NULL;

  if (!g_variant_dict_lookup (&dict, "level", "i", &level))
    level = -1;

  if (!g_variant_dict_lookup (&dict, "monitor", "i", &monitor))
    monitor = -1;

  icon = NULL;
  if (icon_name)
    icon = g_icon_new_for_string (icon_name, NULL);

  if (monitor != -1)
    {
      for (i = 0; i < osd->n_monitors; i++)
        {
          if (i == monitor)
            {
              flashback_osd_window_set_icon (osd->windows[i], icon);
              flashback_osd_window_set_label (osd->windows[i], label);
              flashback_osd_window_set_level (osd->windows[i], level);
              flashback_osd_window_show (osd->windows[i]);
            }
          else
            {
              flashback_osd_window_hide (osd->windows[i]);
            }
        }
    }
  else
    {
      for (i = 0; i < osd->n_monitors; i++)
        {
          flashback_osd_window_set_icon (osd->windows[i], icon);
          flashback_osd_window_set_label (osd->windows[i], label);
          flashback_osd_window_set_level (osd->windows[i], level);
          flashback_osd_window_show (osd->windows[i]);
        }
    }

  if (icon)
    g_object_unref (icon);
}
