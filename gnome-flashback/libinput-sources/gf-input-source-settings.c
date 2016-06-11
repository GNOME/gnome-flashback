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

#include "config.h"

#include <gio/gio.h>

#include "gf-input-source-settings.h"

#define DESKTOP_INPUT_SOURCES_SCHEMA "org.gnome.desktop.input-sources"

#define KEY_SOURCES "sources"
#define KEY_MRU_SOURCES "mru-sources"
#define KEY_XKB_OPTIONS "xkb-options"
#define KEY_PER_WINDOW "per-window"

struct _GfInputSourceSettings
{
  GObject    parent;

  GSettings *settings;
};

enum
{
  SIGNAL_SOURCES_CHANGED,
  SIGNAL_XKB_OPTIONS_CHANGED,
  SIGNAL_PER_WINDOW_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfInputSourceSettings, gf_input_source_settings, G_TYPE_OBJECT)

static void
changed_cb (GSettings *settings,
            gchar     *key,
            gpointer   user_data)
{
  GfInputSourceSettings *source_settings;

  source_settings = GF_INPUT_SOURCE_SETTINGS (user_data);

  if (g_strcmp0 (key, KEY_SOURCES) == 0)
    g_signal_emit (source_settings, signals[SIGNAL_SOURCES_CHANGED], 0);
  else if (g_strcmp0 (key, KEY_XKB_OPTIONS) == 0)
    g_signal_emit (source_settings, signals[SIGNAL_XKB_OPTIONS_CHANGED], 0);
  else if (g_strcmp0 (key, KEY_PER_WINDOW) == 0)
    g_signal_emit (source_settings, signals[SIGNAL_PER_WINDOW_CHANGED], 0);
}

static void
gf_input_source_settings_dispose (GObject *object)
{
  GfInputSourceSettings *settings;

  settings = GF_INPUT_SOURCE_SETTINGS (object);

  g_clear_object (&settings->settings);

  G_OBJECT_CLASS (gf_input_source_settings_parent_class)->dispose (object);
}

static void
gf_input_source_settings_class_init (GfInputSourceSettingsClass *settings_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (settings_class);

  object_class->dispose = gf_input_source_settings_dispose;

  /**
   * GfInputSourceSettings::sources-changed:
   * @settings: the object on which the signal is emitted
   *
   * The ::sources-changed signal is emitted each time when sources setting
   * has changed.
   */
  signals[SIGNAL_SOURCES_CHANGED] =
    g_signal_new ("sources-changed", G_TYPE_FROM_CLASS (settings_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * GfInputSourceSettings::xkb-options-changed:
   * @settings: the object on which the signal is emitted
   *
   * The ::xkb-options-changed signal is emitted each time when xkb-options
   * setting has changed.
   */
  signals[SIGNAL_XKB_OPTIONS_CHANGED] =
    g_signal_new ("xkb-options-changed", G_TYPE_FROM_CLASS (settings_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * GfInputSourceSettings::per-window-changed:
   * @settings: the object on which the signal is emitted
   *
   * The ::per-window-changed signal is emitted each time when per-window
   * setting has changed.
   */
  signals[SIGNAL_PER_WINDOW_CHANGED] =
    g_signal_new ("per-window-changed", G_TYPE_FROM_CLASS (settings_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gf_input_source_settings_init (GfInputSourceSettings *settings)
{
  settings->settings = g_settings_new (DESKTOP_INPUT_SOURCES_SCHEMA);

  g_signal_connect (settings->settings, "changed",
                    G_CALLBACK (changed_cb), settings);
}

/**
 * gf_input_source_settings_new:
 *
 * Creates a new #GfInputSourceSettings.
 *
 * Returns: (transfer full): a newly created #GfInputSourceSettings.
 */
GfInputSourceSettings *
gf_input_source_settings_new (void)
{
  return g_object_new (GF_TYPE_INPUT_SOURCE_SETTINGS, NULL);
}

/**
 * gf_input_source_settings_get_sources:
 * @settings: a #GfInputSourceSettings
 *
 * List of input source identifiers available. Each source is specified as a
 * tuple of 2 strings. The first string is the type and can be one of 'xkb' or
 * 'ibus'.
 *
 * For 'xkb' sources the second string is 'xkb_layout+xkb_variant' or just
 * 'xkb_layout' if a XKB variant isn't needed.
 *
 * For 'ibus' sources the second string is the IBus engine name.
 *
 * Returns: (transfer full): a #GVariant with list of input sources.
 */
GVariant *
gf_input_source_settings_get_sources (GfInputSourceSettings *settings)
{
  return g_settings_get_value (settings->settings, KEY_SOURCES);
}

GVariant *
gf_input_source_settings_get_mru_sources (GfInputSourceSettings *settings)
{
  return g_settings_get_value (settings->settings, KEY_MRU_SOURCES);
}

void
gf_input_source_settings_set_mru_sources (GfInputSourceSettings *settings,
                                          GVariant              *mru_sources)
{
  g_settings_set_value (settings->settings, KEY_MRU_SOURCES, mru_sources);
}

/**
 * gf_input_source_settings_get_xkb_options:
 * @settings: a #GfInputSourceSettings
 *
 * Returns list of XKB options. Each option is an XKB option string as defined
 * by xkeyboard-config's rules files.
 *
 * Returns: (array zero-terminated=1) (transfer full): a newly-allocated,
 *          %NULL-termindated array of xkb options.
 */
gchar **
gf_input_source_settings_get_xkb_options (GfInputSourceSettings *settings)
{
  return g_settings_get_strv (settings->settings, KEY_XKB_OPTIONS);
}

/**
 * gf_input_source_settings_get_per_window:
 * @settings: a #GfInputSourceSettings
 *
 * Returns if input sources should be attached to currently focused window.
 *
 * Returns: %TRUE if input sources should be attached to focused window
 */
gboolean
gf_input_source_settings_get_per_window (GfInputSourceSettings *settings)
{
  return g_settings_get_boolean (settings->settings, KEY_PER_WINDOW);
}
