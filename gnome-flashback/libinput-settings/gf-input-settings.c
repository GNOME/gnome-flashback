/*
 * Copyright (C) 2016 Alberts Muktupāvels
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

#include "gf-input-settings.h"

struct _GfInputSettings
{
  GObject parent;
};

G_DEFINE_TYPE (GfInputSettings, gf_input_settings, G_TYPE_OBJECT)

static void
gf_input_settings_class_init (GfInputSettingsClass *settings_class)
{
}

static void
gf_input_settings_init (GfInputSettings *settings)
{
}

GfInputSettings *
gf_input_settings_new (void)
{
  return g_object_new (GF_TYPE_INPUT_SETTINGS, NULL);
}
