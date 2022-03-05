/*
 * Copyright (C) 2022 Alberts Muktupāvels
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
#include "gf-media-keys.h"

#include "gsd-media-keys-manager.h"

struct _GfMediaKeys
{
  GObject              parent;

  GsdMediaKeysManager *manager;
};

G_DEFINE_TYPE (GfMediaKeys, gf_media_keys, G_TYPE_OBJECT)

static void
gf_media_keys_dispose (GObject *object)
{
  GfMediaKeys *self;

  self = GF_MEDIA_KEYS (object);

  if (self->manager != NULL)
    {
      gsd_media_keys_manager_stop (self->manager);
      g_clear_object (&self->manager);
    }

  G_OBJECT_CLASS (gf_media_keys_parent_class)->dispose (object);
}

static void
gf_media_keys_class_init (GfMediaKeysClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_media_keys_dispose;
}

static void
gf_media_keys_init (GfMediaKeys *self)
{
  self->manager = gsd_media_keys_manager_new ();

  gsd_media_keys_manager_start (self->manager, NULL);
}

GfMediaKeys *
gf_media_keys_new (void)
{
  return g_object_new (GF_TYPE_MEDIA_KEYS, NULL);
}
