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

#include "flashback-workarounds.h"

struct _FlashbackWorkarounds
{
  GObject parent;
};

G_DEFINE_TYPE (FlashbackWorkarounds, flashback_workarounds, G_TYPE_OBJECT)

static void
flashback_workarounds_dispose (GObject *object)
{
  G_OBJECT_CLASS (flashback_workarounds_parent_class)->dispose (object);
}

static void
flashback_workarounds_class_init (FlashbackWorkaroundsClass *workarounds_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (workarounds_class);

  object_class->dispose = flashback_workarounds_dispose;
}

static void
flashback_workarounds_init (FlashbackWorkarounds *workarounds)
{
}

FlashbackWorkarounds *
flashback_workarounds_new (void)
{
	return g_object_new (FLASHBACK_TYPE_WORKAROUNDS, NULL);
}
