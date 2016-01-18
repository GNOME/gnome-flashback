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

#ifndef GF_CANDIDATE_POPUP_H
#define GF_CANDIDATE_POPUP_H

#include <ibus-1.0/ibus.h>
#include <libcommon/gf-popup-window.h>

G_BEGIN_DECLS

#define GF_TYPE_CANDIDATE_POPUP gf_candidate_popup_get_type ()
G_DECLARE_FINAL_TYPE (GfCandidatePopup, gf_candidate_popup,
                      GF, CANDIDATE_POPUP, GfPopupWindow)

GfCandidatePopup *gf_candidate_popup_new               (void);

void              gf_candidate_popup_set_panel_service (GfCandidatePopup *popup,
                                                        IBusPanelService *service);

G_END_DECLS

#endif
