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

#include <libstatus-notifier/sn.h>

#include "gf-watcher.h"

struct _GfWatcher
{
  GObject    parent;

  SnWatcher *watcher;
};

G_DEFINE_TYPE (GfWatcher, gf_watcher, G_TYPE_OBJECT)

static void
gf_watcher_finalize (GObject *object)
{
  GfWatcher *watcher;

  watcher = GF_WATCHER (object);

  g_clear_object (&watcher->watcher);

  G_OBJECT_CLASS (gf_watcher_parent_class)->finalize (object);
}

static void
gf_watcher_class_init (GfWatcherClass *watcher_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (watcher_class);

  object_class->finalize = gf_watcher_finalize;
}

static void
gf_watcher_init (GfWatcher *watcher)
{
  watcher->watcher = sn_watcher_new (SN_WATCHER_FLAGS_NONE);
}

GfWatcher *
gf_watcher_new (void)
{
  return g_object_new (GF_TYPE_WATCHER, NULL);
}
