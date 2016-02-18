/*
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

#include "config.h"

#include "nd-daemon.h"
#include "gf-notifications.h"

struct _GfNotifications
{
  GObject   parent;

  NdDaemon *daemon;
};

G_DEFINE_TYPE (GfNotifications, gf_notifications, G_TYPE_OBJECT)

static void
gf_notifications_dispose (GObject *object)
{
  GfNotifications *notifications;

  notifications = GF_NOTIFICATIONS (object);

  g_clear_object (&notifications->daemon);

  G_OBJECT_CLASS (gf_notifications_parent_class)->dispose (object);
}

static void
gf_notifications_class_init (GfNotificationsClass *notifications_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (notifications_class);

  object_class->dispose = gf_notifications_dispose;
}

static void
gf_notifications_init (GfNotifications *notifications)
{
  notifications->daemon = nd_daemon_new ();
}

GfNotifications *
gf_notifications_new (void)
{
  return g_object_new (GF_TYPE_NOTIFICATIONS, NULL);
}
