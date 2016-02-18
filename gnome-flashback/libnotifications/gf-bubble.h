/*
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2016 Alberts Muktupāvels
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

#ifndef GF_BUBBLE_H
#define GF_BUBBLE_H

#include <gtk/gtk.h>
#include <libcommon/gf-popup-window.h>

#include "nd-notification.h"

G_BEGIN_DECLS

#define GF_TYPE_BUBBLE gf_bubble_get_type ()
G_DECLARE_DERIVABLE_TYPE (GfBubble, gf_bubble, GF, BUBBLE, GfPopupWindow)

struct _GfBubbleClass
{
  GfPopupWindowClass parent_class;

  void (* changed) (GfBubble *bubble);
};

GfBubble       *gf_bubble_new_for_notification (NdNotification *notification);

NdNotification *gf_bubble_get_notification     (GfBubble       *bubble);

G_END_DECLS

#endif
