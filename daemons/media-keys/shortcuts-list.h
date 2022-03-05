/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SHORTCUTS_LIST_H__
#define __SHORTCUTS_LIST_H__

#include "shell-action-modes.h"
#include "media-keys.h"

#define SETTINGS_BINDING_DIR "org.gnome.gnome-flashback.keybindings"

#define SCREENSAVER_MODE SHELL_ACTION_MODE_ALL & ~SHELL_ACTION_MODE_UNLOCK_SCREEN
#define NO_LOCK_MODE SCREENSAVER_MODE & ~SHELL_ACTION_MODE_LOCK_SCREEN

static struct {
        MediaKeyType key_type;
        const char *settings_key;
        gboolean static_setting;
        ShellActionMode modes;
        MetaKeyBindingFlags grab_flags;
} media_keys[] = {
        { SCREENSHOT_KEY, "screenshot", FALSE, NO_LOCK_MODE, META_KEY_BINDING_IGNORE_AUTOREPEAT },
        { WINDOW_SCREENSHOT_KEY, "window-screenshot", FALSE, NO_LOCK_MODE, META_KEY_BINDING_IGNORE_AUTOREPEAT },
        { AREA_SCREENSHOT_KEY, "area-screenshot", FALSE, NO_LOCK_MODE, META_KEY_BINDING_IGNORE_AUTOREPEAT },
        { SCREENSHOT_CLIP_KEY, "screenshot-clip", FALSE, SHELL_ACTION_MODE_ALL, META_KEY_BINDING_IGNORE_AUTOREPEAT },
        { WINDOW_SCREENSHOT_CLIP_KEY, "window-screenshot-clip", FALSE, SHELL_ACTION_MODE_NORMAL, META_KEY_BINDING_IGNORE_AUTOREPEAT },
        { AREA_SCREENSHOT_CLIP_KEY, "area-screenshot-clip", FALSE, SHELL_ACTION_MODE_ALL, META_KEY_BINDING_IGNORE_AUTOREPEAT },
        { SCREENCAST_KEY, "screencast", FALSE, NO_LOCK_MODE, META_KEY_BINDING_IGNORE_AUTOREPEAT },
};

#undef SCREENSAVER_MODE

#endif /* __SHORTCUTS_LIST_H__ */
