/*
 * Copyright (C) 2015 Sebastian Geiger
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

#include "gf-candidate-popup.h"
#include "gf-ibus-manager.h"

struct _GfIBusManager
{
  GObject           parent;

  GfCandidatePopup *candidate_popup;
};

enum
{
  SIGNAL_READY,
  SIGNAL_PROPERTIES_REGISTERED,
  SIGNAL_PROPERTY_UPDATED,
  SIGNAL_SET_CONTENT_TYPE,

  SIGNAL_ENGINE_SET,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GfIBusManager, gf_ibus_manager, G_TYPE_OBJECT)

static void
gf_ibus_manager_dispose (GObject *object)
{
  GfIBusManager *manager;

  manager = GF_IBUS_MANAGER (object);

  g_clear_object (&manager->candidate_popup);

  G_OBJECT_CLASS (gf_ibus_manager_parent_class)->dispose (object);
}

static void
gf_ibus_manager_class_init (GfIBusManagerClass *manager_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (manager_class);

  object_class->dispose = gf_ibus_manager_dispose;

  signals[SIGNAL_READY] =
    g_signal_new ("ready", G_TYPE_FROM_CLASS (manager_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals[SIGNAL_PROPERTIES_REGISTERED] =
    g_signal_new ("properties-registered", G_TYPE_FROM_CLASS (manager_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_STRING, IBUS_TYPE_PROP_LIST);

  signals[SIGNAL_PROPERTY_UPDATED] =
    g_signal_new ("property-updated", G_TYPE_FROM_CLASS (manager_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_STRING, IBUS_TYPE_PROPERTY);

  signals[SIGNAL_SET_CONTENT_TYPE] =
    g_signal_new ("set-content-type", G_TYPE_FROM_CLASS (manager_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

  signals[SIGNAL_ENGINE_SET] =
    g_signal_new ("engine-set", G_TYPE_FROM_CLASS (manager_class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gf_ibus_manager_init (GfIBusManager *manager)
{
  manager->candidate_popup = gf_candidate_popup_new ();
}

GfIBusManager *
gf_ibus_manager_new (void)
{
  return g_object_new (GF_TYPE_IBUS_MANAGER, NULL);
}

IBusEngineDesc *
gf_ibus_manager_get_engine_desc (GfIBusManager *manager,
                                 const gchar   *id)
{
  return NULL;
}

void
gf_ibus_manager_set_engine (GfIBusManager *manager,
                            const gchar   *id)
{
  g_signal_emit (manager, signals[SIGNAL_ENGINE_SET], 0);
}

void
gf_ibus_manager_preload_engines (GfIBusManager  *manager,
                                 gchar         **engines)
{
}
