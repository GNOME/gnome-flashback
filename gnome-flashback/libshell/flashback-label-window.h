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

#ifndef FLASHBACK_LABEL_WINDOW_H
#define FLASHBACK_LABEL_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define FLASHBACK_TYPE_LABEL_WINDOW flashback_label_window_get_type ()
G_DECLARE_FINAL_TYPE (FlashbackLabelWindow, flashback_label_window,
                      FLASHBACK, LABEL_WINDOW, GtkWindow)

FlashbackLabelWindow *flashback_label_window_new  (gint                  monitor,
                                                   const gchar          *label);

void                  flashback_label_window_show (FlashbackLabelWindow *window);
void                  flashback_label_window_hide (FlashbackLabelWindow *window);

G_END_DECLS

#endif
