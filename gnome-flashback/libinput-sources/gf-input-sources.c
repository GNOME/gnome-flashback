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

#include "gf-ibus-manager.h"
#include "gf-input-sources.h"
#include "gf-input-source-manager.h"

struct _GfInputSources
{
  GObject               parent;

  GfIBusManager        *ibus_manager;
  GfInputSourceManager *input_source_manager;
};

G_DEFINE_TYPE (GfInputSources, gf_input_sources, G_TYPE_OBJECT)

static void
sources_changed_cb (GfInputSourceManager *manager,
                    gpointer              user_data)
{
}

static void
gf_input_sources_dispose (GObject *object)
{
  GfInputSources *sources;

  sources = GF_INPUT_SOURCES (object);

  g_clear_object (&sources->ibus_manager);
  g_clear_object (&sources->input_source_manager);

  G_OBJECT_CLASS (gf_input_sources_parent_class)->dispose (object);
}

static void
gf_input_sources_class_init (GfInputSourcesClass *sources_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (sources_class);

  object_class->dispose = gf_input_sources_dispose;
}

static void
gf_input_sources_init (GfInputSources *sources)
{
  sources->ibus_manager = gf_ibus_manager_new ();
  sources->input_source_manager = gf_input_source_manager_new (sources->ibus_manager);

  g_signal_connect (sources->input_source_manager, "sources-changed",
                    G_CALLBACK (sources_changed_cb), sources);

  gf_input_source_manager_reload (sources->input_source_manager);
}

GfInputSources *
gf_input_sources_new (void)
{
  return g_object_new (GF_TYPE_INPUT_SOURCES, NULL);
}
