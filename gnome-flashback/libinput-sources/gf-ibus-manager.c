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

#include "gf-ibus-manager.h"
#include "gf-candidate-popup.h"

struct _GfIBusManager
{
  GObject           parent;

  GfCandidatePopup *candidate_popup;
};

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
