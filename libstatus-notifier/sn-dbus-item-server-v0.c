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

#include "sn-dbus-item-server-v0.h"
#include "sn-item-v0-gen.h"
#include "sn-dbus-menu-server.h"
#include "sn-watcher-v0-gen.h"
#include "sn-item-private.h"
#include "sn-watcher-private.h"

struct _SnDBusItemServerV0
{
  SnDBusItemServer  parent;

  guint             id;
  SnItemV0Gen      *dbus_item;

  guint             name_id;
  guint             watcher_id;

  GCancellable     *watcher_cancellable;
  SnWatcherV0Gen   *watcher;

  gboolean          is_item_ready;
  gboolean          registered;

  GtkMenu          *menu;
  SnDBusMenuServer *dbus_menu;
};

G_DEFINE_TYPE (SnDBusItemServerV0, sn_dbus_item_server_v0,
               SN_TYPE_DBUS_ITEM_SERVER)

static gboolean
handle_context_menu (SnItemV0Gen           *item_v0_gen,
                     GDBusMethodInvocation *invocation,
                     gint                   x,
                     gint                   y,
                     SnDBusItemServerV0    *server_v0)
{
  sn_item_v0_gen_complete_context_menu (item_v0_gen, invocation);
  sn_dbus_item_server_emit_context_menu (SN_DBUS_ITEM_SERVER (server_v0),
                                         x, y);

  return TRUE;
}

static gboolean
handle_activate (SnItemV0Gen           *item_v0_gen,
                 GDBusMethodInvocation *invocation,
                 gint                   x,
                 gint                   y,
                 SnDBusItemServerV0    *server_v0)
{
  sn_item_v0_gen_complete_activate (item_v0_gen, invocation);
  sn_dbus_item_server_emit_activate (SN_DBUS_ITEM_SERVER (server_v0), x, y);

  return TRUE;
}

static gboolean
handle_secondary_activate (SnItemV0Gen           *item_v0_gen,
                           GDBusMethodInvocation *invocation,
                           gint                   x,
                           gint                   y,
                           SnDBusItemServerV0    *server_v0)
{
  sn_item_v0_gen_complete_secondary_activate (item_v0_gen, invocation);
  sn_dbus_item_server_emit_secondary_activate (SN_DBUS_ITEM_SERVER (server_v0),
                                               x, y);

  return TRUE;
}

static gboolean
handle_scroll (SnItemV0Gen           *item_v0_gen,
               GDBusMethodInvocation *invocation,
               gint                   delta,
               const gchar           *orientation,
               SnDBusItemServerV0    *server_v0)
{
  SnItemOrientation scroll_orientation;

  scroll_orientation = SN_ITEM_ORIENTATION_HORIZONTAL;

  if (g_strcmp0 (orientation, "Vertical") == 0)
    scroll_orientation = SN_ITEM_ORIENTATION_VERTICAL;
  else if (g_strcmp0 (orientation, "Horizontal") == 0)
    scroll_orientation = SN_ITEM_ORIENTATION_HORIZONTAL;
  else
    g_warning ("Unknown scroll orientation: %s", orientation);

  sn_item_v0_gen_complete_scroll (item_v0_gen, invocation);
  sn_dbus_item_server_emit_scroll (SN_DBUS_ITEM_SERVER (server_v0),
                                   delta, scroll_orientation);

  return TRUE;
}

static void
host_registered_cb (SnWatcherV0Gen     *watcher_v0_gen,
                    SnDBusItemServerV0 *server_v0)
{
  gboolean registered;

  registered = sn_watcher_v0_gen_get_is_host_registered (watcher_v0_gen);

  sn_dbus_item_server_emit_hosts_changed (SN_DBUS_ITEM_SERVER (server_v0),
                                          registered);
}

static void
register_item_cb (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  SnDBusItemServerV0 *server;
  GError *error;

  server = SN_DBUS_ITEM_SERVER_V0 (user_data);
  error = NULL;

  sn_watcher_v0_gen_call_register_item_finish (server->watcher, res, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        sn_dbus_item_emit_error (SN_DBUS_ITEM (server), error);
      g_error_free (error);

      return;
    }

  server->registered = TRUE;
}

static void
register_with_watcher (SnDBusItemServerV0 *server)
{
  SnDBusItem *impl;
  const gchar *object_path;

  if (!server->watcher || !server->is_item_ready || server->registered)
    return;

  impl = SN_DBUS_ITEM (server);
  object_path = sn_dbus_item_get_object_path (impl);

  sn_watcher_v0_gen_call_register_item (server->watcher, object_path, NULL,
                                        register_item_cb, server);
}

static void
proxy_ready_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  GError *error;
  SnWatcherV0Gen *watcher;
  SnDBusItemServerV0 *server;

  error = NULL;
  watcher = sn_watcher_v0_gen_proxy_new_finish (res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  server = SN_DBUS_ITEM_SERVER_V0 (user_data);
  server->watcher = watcher;

  if (error)
    {
      sn_dbus_item_emit_error (SN_DBUS_ITEM (server), error);
      g_error_free (error);

      return;
    }

  g_signal_connect (server->watcher, "host-registered",
                    G_CALLBACK (host_registered_cb), server);

  register_with_watcher (server);
}

static void
name_appeared_cb (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (user_data);

  server->watcher_cancellable = g_cancellable_new ();
  sn_watcher_v0_gen_proxy_new (connection, G_DBUS_PROXY_FLAGS_NONE,
                               SN_WATCHER_V0_BUS_NAME,
                               SN_WATCHER_V0_OBJECT_PATH,
                               server->watcher_cancellable,
                               proxy_ready_cb, server);
}

static void
name_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (user_data);

  if (server->watcher_cancellable)
    {
      g_cancellable_cancel (server->watcher_cancellable);
      g_clear_object (&server->watcher_cancellable);
    }

  g_clear_object (&server->watcher);

  server->registered = FALSE;
}

static void
sn_dbus_item_server_v0_dispose (GObject *object)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (object);

  if (server->registered)
    {
      SnDBusItem *impl;

      impl = SN_DBUS_ITEM (server);

      SN_DBUS_ITEM_GET_CLASS (impl)->unregister (impl);
    }

  if (server->watcher_cancellable)
    {
      g_cancellable_cancel (server->watcher_cancellable);
      g_clear_object (&server->watcher_cancellable);
    }

  if (server->name_id > 0)
    {
      g_bus_unown_name (server->name_id);
      server->name_id = 0;
    }

  if (server->watcher_id > 0)
    {
      g_bus_unwatch_name (server->watcher_id);
      server->watcher_id = 0;
    }

  g_clear_object (&server->watcher);
  g_clear_object (&server->dbus_item);

  g_clear_object (&server->menu);
  g_clear_object (&server->dbus_menu);

  G_OBJECT_CLASS (sn_dbus_item_server_v0_parent_class)->dispose (object);
}

static const gchar *
sn_dbus_item_server_v0_get_attention_icon_name (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_attention_icon_name (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_attention_icon_name (SnDBusItem  *impl,
                                                const gchar *attention_icon_name)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_attention_icon_name (server->dbus_item,
                                          attention_icon_name);
  sn_item_v0_gen_emit_new_attention_icon (server->dbus_item);
}

static GVariant *
sn_dbus_item_server_v0_get_attention_icon_pixmap (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_attention_icon_pixmap (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_attention_icon_pixmap (SnDBusItem *impl,
                                                  GVariant   *attention_icon_pixmap)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_attention_icon_pixmap (server->dbus_item,
                                            attention_icon_pixmap);
  sn_item_v0_gen_emit_new_attention_icon (server->dbus_item);
}

static const gchar *
sn_dbus_item_server_v0_get_attention_movie_name (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_attention_movie_name (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_attention_movie_name (SnDBusItem  *impl,
                                                 const gchar *attention_movie_name)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_attention_movie_name (server->dbus_item,
                                           attention_movie_name);
}

static SnItemCategory
sn_dbus_item_server_v0_get_category (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;
  const gchar *string;
  SnItemCategory category;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);
  string = sn_item_v0_gen_get_category (server->dbus_item);

  if (g_strcmp0 (string, "ApplicationStatus") == 0)
    category = SN_ITEM_CATEGORY_APPLICATION_STATUS;
  else if (g_strcmp0 (string, "Hardware") == 0)
    category = SN_ITEM_CATEGORY_HARDWARE;
  else if (g_strcmp0 (string, "SystemServices") == 0)
    category = SN_ITEM_CATEGORY_SYSTEM_SERVICES;
  else if (g_strcmp0 (string, "Communications") == 0)
    category = SN_ITEM_CATEGORY_COMMUNICATIONS;
  else
    category = SN_ITEM_CATEGORY_APPLICATION_STATUS;

  return category;
}

static void
sn_dbus_item_server_v0_set_category (SnDBusItem     *impl,
                                     SnItemCategory  category)
{
  SnDBusItemServerV0 *server;
  const gchar *string;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  switch (category)
    {
      case SN_ITEM_CATEGORY_APPLICATION_STATUS:
        string = "ApplicationStatus";
        break;

      case SN_ITEM_CATEGORY_COMMUNICATIONS:
        string = "Communications";
        break;

      case SN_ITEM_CATEGORY_SYSTEM_SERVICES:
        string = "SystemServices";
        break;

      case SN_ITEM_CATEGORY_HARDWARE:
        string = "Hardware";
        break;

      default:
        string = "ApplicationStatus";
        break;
    }

  sn_item_v0_gen_set_category (server->dbus_item, string);
}

static const gchar *
sn_dbus_item_server_v0_get_id (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_id (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_id (SnDBusItem  *impl,
                               const gchar *id)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_id (server->dbus_item, id);
}

static const gchar *
sn_dbus_item_server_v0_get_icon_name (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_icon_name (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_icon_name (SnDBusItem  *impl,
                                      const gchar *icon_name)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_icon_name (server->dbus_item, icon_name);
  sn_item_v0_gen_emit_new_icon (server->dbus_item);
}

static GVariant *
sn_dbus_item_server_v0_get_icon_pixmap (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_icon_pixmap (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_icon_pixmap (SnDBusItem *impl,
                                        GVariant   *icon_pixmap)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_icon_pixmap (server->dbus_item, icon_pixmap);
  sn_item_v0_gen_emit_new_icon (server->dbus_item);
}

static const gchar *
sn_dbus_item_server_v0_get_icon_theme_path (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_icon_theme_path (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_icon_theme_path (SnDBusItem  *impl,
                                            const gchar *icon_theme_path)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_icon_theme_path (server->dbus_item, icon_theme_path);
  sn_item_v0_gen_emit_new_icon_theme_path (server->dbus_item, icon_theme_path);
}

static gboolean
sn_dbus_item_server_v0_get_item_is_menu (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_item_is_menu (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_item_is_menu (SnDBusItem *impl,
                                         gboolean    item_is_menu)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_item_is_menu (server->dbus_item, item_is_menu);
}

static GtkMenu *
sn_dbus_item_server_v0_get_menu (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return server->menu;
}

static void
sn_dbus_item_server_v0_set_menu (SnDBusItem *impl,
                                 GtkMenu    *menu)
{
  SnDBusItemServerV0 *server;
  const gchar *object_path;
  gchar *menu_path;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  g_clear_object (&server->menu);
  g_clear_object (&server->dbus_menu);

  if (menu == NULL)
    {
      sn_item_v0_gen_set_menu (server->dbus_item, "/");
      return;
    }

  object_path = sn_dbus_item_get_object_path (impl);
  menu_path = g_strdup_printf ("%s/Menu", object_path);

  server->menu = g_object_ref_sink (menu);
  server->dbus_menu = sn_dbus_menu_server_new (menu, menu_path);

  sn_item_v0_gen_set_menu (server->dbus_item, menu_path);
  g_free (menu_path);
}

static const gchar *
sn_dbus_item_server_v0_get_overlay_icon_name (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_overlay_icon_name (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_overlay_icon_name (SnDBusItem  *impl,
                                              const gchar *overlay_icon_name)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_overlay_icon_name (server->dbus_item, overlay_icon_name);
  sn_item_v0_gen_emit_new_overlay_icon (server->dbus_item);
}

static GVariant *
sn_dbus_item_server_v0_get_overlay_icon_pixmap (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_overlay_icon_pixmap (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_overlay_icon_pixmap (SnDBusItem *impl,
                                                GVariant   *overlay_icon_pixmap)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_overlay_icon_pixmap (server->dbus_item,
                                          overlay_icon_pixmap);
  sn_item_v0_gen_emit_new_overlay_icon (server->dbus_item);
}

static SnItemStatus
sn_dbus_item_server_v0_get_status (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;
  const gchar *string;
  SnItemStatus status;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);
  string = sn_item_v0_gen_get_status (server->dbus_item);

  if (g_strcmp0 (string, "Passive") == 0)
    status = SN_ITEM_STATUS_PASSIVE;
  else if (g_strcmp0 (string, "Active") == 0)
    status = SN_ITEM_STATUS_ACTIVE;
  else if (g_strcmp0 (string, "NeedsAttention") == 0)
    status = SN_ITEM_STATUS_NEEDS_ATTENTION;
  else
    status = SN_ITEM_STATUS_PASSIVE;

  return status;
}

static void
sn_dbus_item_server_v0_set_status (SnDBusItem   *impl,
                                   SnItemStatus  status)
{
  SnDBusItemServerV0 *server;
  const gchar *string;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  switch (status)
    {
      case SN_ITEM_STATUS_PASSIVE:
        string = "Passive";
        break;

      case SN_ITEM_STATUS_ACTIVE:
        string = "Active";
        break;

      case SN_ITEM_STATUS_NEEDS_ATTENTION:
        string = "NeedsAttention";
        break;

      default:
        string = "Passive";
        break;
    }

  sn_item_v0_gen_set_status (server->dbus_item, string);
  sn_item_v0_gen_emit_new_status (server->dbus_item, string);
}

static const gchar *
sn_dbus_item_server_v0_get_title (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_title (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_title (SnDBusItem  *impl,
                                  const gchar *title)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_title (server->dbus_item, title);
  sn_item_v0_gen_emit_new_title (server->dbus_item);
}

static GVariant *
sn_dbus_item_server_v0_get_tooltip (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_tool_tip (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_tooltip (SnDBusItem *impl,
                                    GVariant   *tooltip)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_tool_tip (server->dbus_item, tooltip);
  sn_item_v0_gen_emit_new_tool_tip (server->dbus_item);
}

static gint
sn_dbus_item_server_v0_get_window_id (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  return sn_item_v0_gen_get_window_id (server->dbus_item);
}

static void
sn_dbus_item_server_v0_set_window_id (SnDBusItem *impl,
                                      gint        window_id)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);

  sn_item_v0_gen_set_window_id (server->dbus_item, window_id);
}

static void
sn_dbus_item_server_v0_context_menu (SnDBusItem *impl,
                                     gint        x,
                                     gint        y)
{
  g_assert_not_reached ();
}

static void
sn_dbus_item_server_v0_activate (SnDBusItem *impl,
                                 gint        x,
                                 gint        y)
{
  g_assert_not_reached ();
}

static void
sn_dbus_item_server_v0_secondary_activate (SnDBusItem *impl,
                                           gint        x,
                                           gint        y)
{
  g_assert_not_reached ();
}

static void
sn_dbus_item_server_v0_scroll (SnDBusItem        *impl,
                               gint               delta,
                               SnItemOrientation  orientation)
{
  g_assert_not_reached ();
}

static void
bus_acquired_cb (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  SnDBusItemServerV0 *server;
  GDBusInterfaceSkeleton *skeleton;
  gchar *object_path;
  GError *error;

  server = SN_DBUS_ITEM_SERVER_V0 (user_data);

  skeleton = G_DBUS_INTERFACE_SKELETON (server->dbus_item);
  object_path = g_strdup_printf ("%s/%d", SN_ITEM_V0_OBJECT_PATH, server->id);
  error = NULL;

  g_signal_connect (server->dbus_item, "handle-context-menu",
                    G_CALLBACK (handle_context_menu), server);
  g_signal_connect (server->dbus_item, "handle-activate",
                    G_CALLBACK (handle_activate), server);
  g_signal_connect (server->dbus_item, "handle-secondary-activate",
                    G_CALLBACK (handle_secondary_activate), server);
  g_signal_connect (server->dbus_item, "handle-scroll",
                    G_CALLBACK (handle_scroll), server);

  if (!g_dbus_interface_skeleton_export (skeleton, connection,
                                         object_path, &error))
    {
      sn_dbus_item_emit_error (SN_DBUS_ITEM (server), error);
      g_error_free (error);

      g_free (object_path);

      return;
    }

  g_object_set (server, "bus-name", name, "object-path", object_path, NULL);
  g_free (object_path);

  register_with_watcher (server);
}

static void
sn_dbus_item_server_v0_register (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;
  gchar *bus_name;
  GBusNameOwnerFlags flags;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);
  server->is_item_ready = TRUE;

  if (server->name_id > 0)
    return;

  bus_name = g_strdup_printf ("%s-%d-%d", SN_ITEM_V0_BUS_NAME,
                              getpid (), server->id);
  flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
          G_BUS_NAME_OWNER_FLAGS_REPLACE;

  server->name_id = g_bus_own_name (G_BUS_TYPE_SESSION, bus_name, flags,
                                    bus_acquired_cb, NULL, NULL,
                                    server, NULL);

  g_free (bus_name);
}

static void
sn_dbus_item_server_v0_unregister (SnDBusItem *impl)
{
  SnDBusItemServerV0 *server;

  server = SN_DBUS_ITEM_SERVER_V0 (impl);
  server->is_item_ready = FALSE;

  if (server->name_id > 0)
    {
      g_bus_unown_name (server->name_id);
      server->name_id = 0;
    }

  server->registered = FALSE;
}

static void
sn_dbus_item_server_v0_class_init (SnDBusItemServerV0Class *server_class)
{
  GObjectClass *object_class;
  SnDBusItemClass *impl_class;

  object_class = G_OBJECT_CLASS (server_class);
  impl_class = SN_DBUS_ITEM_CLASS (server_class);

  object_class->dispose = sn_dbus_item_server_v0_dispose;

  impl_class->get_attention_icon_name = sn_dbus_item_server_v0_get_attention_icon_name;
  impl_class->set_attention_icon_name = sn_dbus_item_server_v0_set_attention_icon_name;
  impl_class->get_attention_icon_pixmap = sn_dbus_item_server_v0_get_attention_icon_pixmap;
  impl_class->set_attention_icon_pixmap = sn_dbus_item_server_v0_set_attention_icon_pixmap;
  impl_class->get_attention_movie_name = sn_dbus_item_server_v0_get_attention_movie_name;
  impl_class->set_attention_movie_name = sn_dbus_item_server_v0_set_attention_movie_name;
  impl_class->get_category = sn_dbus_item_server_v0_get_category;
  impl_class->set_category = sn_dbus_item_server_v0_set_category;
  impl_class->get_id = sn_dbus_item_server_v0_get_id;
  impl_class->set_id = sn_dbus_item_server_v0_set_id;
  impl_class->get_icon_name = sn_dbus_item_server_v0_get_icon_name;
  impl_class->set_icon_name = sn_dbus_item_server_v0_set_icon_name;
  impl_class->get_icon_pixmap = sn_dbus_item_server_v0_get_icon_pixmap;
  impl_class->set_icon_pixmap = sn_dbus_item_server_v0_set_icon_pixmap;
  impl_class->get_icon_theme_path = sn_dbus_item_server_v0_get_icon_theme_path;
  impl_class->set_icon_theme_path = sn_dbus_item_server_v0_set_icon_theme_path;
  impl_class->get_item_is_menu = sn_dbus_item_server_v0_get_item_is_menu;
  impl_class->set_item_is_menu = sn_dbus_item_server_v0_set_item_is_menu;
  impl_class->get_menu = sn_dbus_item_server_v0_get_menu;
  impl_class->set_menu = sn_dbus_item_server_v0_set_menu;
  impl_class->get_overlay_icon_name = sn_dbus_item_server_v0_get_overlay_icon_name;
  impl_class->set_overlay_icon_name = sn_dbus_item_server_v0_set_overlay_icon_name;
  impl_class->get_overlay_icon_pixmap = sn_dbus_item_server_v0_get_overlay_icon_pixmap;
  impl_class->set_overlay_icon_pixmap = sn_dbus_item_server_v0_set_overlay_icon_pixmap;
  impl_class->get_status = sn_dbus_item_server_v0_get_status;
  impl_class->set_status = sn_dbus_item_server_v0_set_status;
  impl_class->get_title = sn_dbus_item_server_v0_get_title;
  impl_class->set_title = sn_dbus_item_server_v0_set_title;
  impl_class->get_tooltip = sn_dbus_item_server_v0_get_tooltip;
  impl_class->set_tooltip = sn_dbus_item_server_v0_set_tooltip;
  impl_class->get_window_id = sn_dbus_item_server_v0_get_window_id;
  impl_class->set_window_id = sn_dbus_item_server_v0_set_window_id;

  impl_class->context_menu = sn_dbus_item_server_v0_context_menu;
  impl_class->activate = sn_dbus_item_server_v0_activate;
  impl_class->secondary_activate = sn_dbus_item_server_v0_secondary_activate;
  impl_class->scroll = sn_dbus_item_server_v0_scroll;

  impl_class->register_ = sn_dbus_item_server_v0_register;
  impl_class->unregister = sn_dbus_item_server_v0_unregister;
}

static void
sn_dbus_item_server_v0_init (SnDBusItemServerV0 *server)
{
  static guint next_id;
  GBusNameWatcherFlags flags;

  server->id = ++next_id;
  server->dbus_item = sn_item_v0_gen_skeleton_new ();

  flags = G_BUS_NAME_WATCHER_FLAGS_NONE;
  server->watcher_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                         SN_WATCHER_V0_BUS_NAME, flags,
                                         name_appeared_cb, name_vanished_cb,
                                         server, NULL);
}
