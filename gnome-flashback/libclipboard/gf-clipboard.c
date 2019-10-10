/*
 * Copyright (C) 2019 Alberts Muktupāvels
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
#include "gf-clipboard.h"

#include "gsd-clipboard-manager.h"

struct _GfClipboard
{
  GObject              parent;

  GsdClipboardManager *manager;
};

G_DEFINE_TYPE (GfClipboard, gf_clipboard, G_TYPE_OBJECT)

static void
gf_clipboard_dispose (GObject *object)
{
  GfClipboard *self;

  self = GF_CLIPBOARD (object);

  if (self->manager != NULL)
    {
      gsd_clipboard_manager_stop (self->manager);
      g_clear_object (&self->manager);
    }

  G_OBJECT_CLASS (gf_clipboard_parent_class)->dispose (object);
}

static void
gf_clipboard_class_init (GfClipboardClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_clipboard_dispose;
}

static void
gf_clipboard_init (GfClipboard *self)
{
  self->manager = gsd_clipboard_manager_new ();

  gsd_clipboard_manager_start (self->manager, NULL);
}

GfClipboard *
gf_clipboard_new (void)
{
  return g_object_new (GF_TYPE_CLIPBOARD, NULL);
}
