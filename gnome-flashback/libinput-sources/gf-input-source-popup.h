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

#ifndef GF_INPUT_SOURCE_POPUP_H
#define GF_INPUT_SOURCE_POPUP_H

#include <libcommon/gf-popup-window.h>

G_BEGIN_DECLS

#define GF_TYPE_INPUT_SOURCE_POPUP gf_input_source_popup_get_type ()
G_DECLARE_FINAL_TYPE (GfInputSourcePopup, gf_input_source_popup,
                      GF, INPUT_SOURCE_POPUP, GfPopupWindow)

GtkWidget *gf_input_source_popup_new (GList    *mru_sources,
                                      gboolean  backward,
                                      guint     keyval,
                                      guint     modifiers);

G_END_DECLS

#endif
