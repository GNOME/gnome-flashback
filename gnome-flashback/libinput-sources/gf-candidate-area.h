/*
 * Copyright (C) 2016 Sebastian Geiger
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

#ifndef GF_CANDIDATE_AREA_H
#define GF_CANDIDATE_AREA_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GF_TYPE_CANDIDATE_AREA gf_candidate_area_get_type ()
G_DECLARE_FINAL_TYPE (GfCandidateArea, gf_candidate_area,
                      GF, CANDIDATE_AREA, GtkBox)

GtkWidget *gf_candidate_area_new             (void);

void       gf_candidate_area_set_orientation (GfCandidateArea *area,
                                              gint             orientation);

void       gf_candidate_area_set_candidates  (GfCandidateArea *area,
                                              GSList          *indexes,
                                              GSList          *candidates,
                                              guint            cursor_position,
                                              gboolean         cursor_visible);

void       gf_candidate_area_update_buttons  (GfCandidateArea *area,
                                              gboolean         wraps_around,
                                              gint             page,
                                              gint             n_pages);

G_END_DECLS

#endif
