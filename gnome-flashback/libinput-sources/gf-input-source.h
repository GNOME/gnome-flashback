/*
 * Copyright (C) 2015 Sebastian Geiger
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

#ifndef GF_INPUT_SOURCE_H
#define GF_INPUT_SOURCE_H

#include <glib-object.h>

#define INPUT_SOURCE_TYPE_XKB "xkb"
#define INPUT_SOURCE_TYPE_IBUS "ibus"

G_BEGIN_DECLS

#define GF_TYPE_INPUT_SOURCE gf_input_source_get_type ()
G_DECLARE_DERIVABLE_TYPE (GfInputSource, gf_input_source,
                          GF, INPUT_SOURCE, GObject)

struct _GfInputSourceClass
{
  GObjectClass parent_class;

  const char * (* get_display_name) (GfInputSource  *self);

  const char * (* get_short_name)   (GfInputSource  *self);

  gboolean     (* set_short_name)   (GfInputSource  *self,
                                     const char     *short_name);

  gboolean     (* get_layout)       (GfInputSource  *self,
                                     const char    **layout,
                                     const char    **variant);

  const char * (* get_xkb_id)       (GfInputSource  *self);
};

const gchar *gf_input_source_get_source_type  (GfInputSource  *source);

const gchar *gf_input_source_get_id           (GfInputSource  *source);

const gchar *gf_input_source_get_display_name (GfInputSource  *source);

const gchar *gf_input_source_get_short_name   (GfInputSource  *source);

void         gf_input_source_set_short_name   (GfInputSource  *source,
                                               const gchar    *short_name);

guint        gf_input_source_get_index        (GfInputSource  *source);

gboolean     gf_input_source_get_layout       (GfInputSource  *source,
                                               const char    **layout,
                                               const char    **variant);

const gchar *gf_input_source_get_xkb_id       (GfInputSource  *source);

void         gf_input_source_activate         (GfInputSource  *source,
                                               gboolean        interactive);

G_END_DECLS

#endif
