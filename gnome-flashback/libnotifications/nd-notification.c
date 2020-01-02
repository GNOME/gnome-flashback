/*
 * Copyright (C) 2010 Red Hat, Inc.
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

#include <string.h>
#include <strings.h>
#include <gtk/gtk.h>

#include "nd-notification.h"

#define ND_NOTIFICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), ND_TYPE_NOTIFICATION, NdNotificationClass))
#define ND_IS_NOTIFICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ND_TYPE_NOTIFICATION))
#define ND_NOTIFICATION_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), ND_TYPE_NOTIFICATION, NdNotificationClass))

enum {
        CHANGED,
        CLOSED,
        ACTION_INVOKED,
        LAST_SIGNAL
};

struct _NdNotification {
        GObject       parent;

        gboolean      is_queued;
        gboolean      is_closed;

        gint64        update_time;

        char         *sender;
        guint32       id;
        char         *app_name;
        char         *app_icon;
        char         *summary;
        char         *body;
        char        **actions;
        GHashTable   *hints;
        int           timeout;
};

static void nd_notification_finalize     (GObject      *object);

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (NdNotification, nd_notification, G_TYPE_OBJECT)

static guint32 notification_serial = 1;

static guint32
get_next_notification_serial (void)
{
        guint32 serial;

        serial = notification_serial++;

        if ((gint32)notification_serial < 0) {
                notification_serial = 1;
        }

        return serial;
}

static void
start_element_cb (GMarkupParseContext  *context,
                  const gchar          *element_name,
                  const gchar         **attribute_names,
                  const gchar         **attribute_values,
                  gpointer              user_data,
                  GError              **error)
{
  GString *str;
  gint i;

  if (g_strcmp0 (element_name, "a") == 0)
    return;

  str = user_data;

  g_string_append (str, "<");
  g_string_append (str, element_name);

  for (i = 0; attribute_names[i] != NULL; i++)
    {
      gchar *tmp;

      tmp = g_markup_escape_text (attribute_values[i], -1);
      g_string_append_printf (str, " %s=\"%s\"", attribute_names[i], tmp);
      g_free (tmp);
    }

  g_string_append (str, ">");
}

static void
end_element_cb (GMarkupParseContext  *context,
                const gchar          *element_name,
                gpointer              user_data,
                GError              **error)
{
  GString *str;

  if (g_strcmp0 (element_name, "a") == 0)
    return;

  str = user_data;

  g_string_append_printf (str, "</%s>", element_name);
}

static void
text_cb (GMarkupParseContext  *context,
         const gchar          *text,
         gsize                 text_len,
         gpointer              user_data,
         GError              **error)
{
  GString *str;
  gchar *tmp;

  str = user_data;

  tmp = g_markup_escape_text (text, text_len);
  g_string_append (str, tmp);
  g_free (tmp);
}

static gboolean
parse_markup (const gchar  *text,
              gchar       **parsed_markup,
              GError      **error)
{
  GString *str;
  GMarkupParseContext *context;

  str = g_string_new (NULL);
  context = g_markup_parse_context_new (&(GMarkupParser) {
                                          start_element_cb,
                                          end_element_cb,
                                          text_cb
                                        },
                                        0, str, NULL);

  if (!g_markup_parse_context_parse (context, "<markup>", -1, error))
    {
      g_markup_parse_context_free (context);
      g_string_free (str, TRUE);

      return FALSE;
    }

  if (!g_markup_parse_context_parse (context, text, -1, error))
    {
      g_markup_parse_context_free (context);
      g_string_free (str, TRUE);

      return FALSE;
    }

  if (!g_markup_parse_context_parse (context, "</markup>", -1, error))
    {
      g_markup_parse_context_free (context);
      g_string_free (str, TRUE);

      return FALSE;
    }

  if (!g_markup_parse_context_end_parse (context, error))
    {
      g_markup_parse_context_free (context);
      g_string_free (str, TRUE);

      return FALSE;
    }

  *parsed_markup = g_string_free (str, FALSE);
  g_markup_parse_context_free (context);

  return TRUE;
}

static void
nd_notification_class_init (NdNotificationClass *class)
{
        GObjectClass *gobject_class;

        gobject_class = G_OBJECT_CLASS (class);

        gobject_class->finalize = nd_notification_finalize;

        signals [CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals [CLOSED] =
                g_signal_new ("closed",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE, 1, G_TYPE_INT);
        signals [ACTION_INVOKED] =
                g_signal_new ("action-invoked",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
nd_notification_init (NdNotification *notification)
{
        notification->id = get_next_notification_serial ();

        notification->app_name = NULL;
        notification->app_icon = NULL;
        notification->summary = NULL;
        notification->body = NULL;
        notification->actions = NULL;
        notification->hints = g_hash_table_new_full (g_str_hash,
                                                     g_str_equal,
                                                     g_free,
                                                     (GDestroyNotify) g_variant_unref);
}

static void
nd_notification_finalize (GObject *object)
{
        NdNotification *notification;

        notification = ND_NOTIFICATION (object);

        g_free (notification->sender);
        g_free (notification->app_name);
        g_free (notification->app_icon);
        g_free (notification->summary);
        g_free (notification->body);
        g_strfreev (notification->actions);

        if (notification->hints != NULL) {
                g_hash_table_destroy (notification->hints);
        }

        if (G_OBJECT_CLASS (nd_notification_parent_class)->finalize)
                (*G_OBJECT_CLASS (nd_notification_parent_class)->finalize) (object);
}

gboolean
nd_notification_update (NdNotification     *notification,
                        const gchar        *app_name,
                        const gchar        *app_icon,
                        const gchar        *summary,
                        const gchar        *body,
                        const gchar *const *actions,
                        GVariant           *hints,
                        gint                timeout)
{
        GVariant *item;
        GVariantIter iter;

        g_return_val_if_fail (ND_IS_NOTIFICATION (notification), FALSE);

        g_free (notification->app_name);
        notification->app_name = g_strdup (app_name);

        g_free (notification->app_icon);
        notification->app_icon = g_strdup (app_icon);

        g_free (notification->summary);
        notification->summary = g_strdup (summary);

        g_free (notification->body);
        notification->body = g_strdup (body);

        g_strfreev (notification->actions);
        notification->actions = g_strdupv ((char **)actions);

        g_hash_table_remove_all (notification->hints);

        g_variant_iter_init (&iter, hints);
        while ((item = g_variant_iter_next_value (&iter))) {
                gchar *key;
                GVariant *value;

                g_variant_get (item, "{sv}", &key, &value);
                g_hash_table_insert (notification->hints, g_strdup (key),
                                     g_variant_ref (value));

                g_variant_unref (value);
                g_free (key);
        }

        notification->timeout = timeout;

        g_signal_emit (notification, signals[CHANGED], 0);

        notification->update_time = g_get_real_time ();

        return TRUE;
}

gint64
nd_notification_get_update_time (NdNotification *notification)
{
        g_return_val_if_fail (ND_IS_NOTIFICATION (notification), 0);

        return notification->update_time;
}

void
nd_notification_set_is_queued (NdNotification *notification,
                               gboolean        is_queued)
{
        g_return_if_fail (ND_IS_NOTIFICATION (notification));

        notification->is_queued = is_queued;
}

gboolean
nd_notification_get_is_queued (NdNotification *notification)
{
        g_return_val_if_fail (ND_IS_NOTIFICATION (notification), FALSE);

        return notification->is_queued;
}

gboolean
nd_notification_get_is_closed (NdNotification *notification)
{
        g_return_val_if_fail (ND_IS_NOTIFICATION (notification), FALSE);

        return notification->is_closed;
}

static gboolean
hint_to_boolean (NdNotification *notification,
                 const gchar    *hint_name)
{
        GVariant *value;

        g_return_val_if_fail (ND_IS_NOTIFICATION (notification), FALSE);

        value = g_hash_table_lookup (notification->hints, hint_name);
        if (value == NULL)
                return FALSE;

        if (g_variant_is_of_type (value, G_VARIANT_TYPE_INT32)) {
                return (g_variant_get_int32 (value) != 0);
        } else if (g_variant_is_of_type (value, G_VARIANT_TYPE_DOUBLE)) {
                return (g_variant_get_double (value) != 0);
        } else if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING)) {
                return TRUE;
        } else if (g_variant_is_of_type (value, G_VARIANT_TYPE_BYTE)) {
                return (g_variant_get_byte (value) != 0);
        } else if (g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN)) {
                return g_variant_get_boolean (value);
        } else {
                g_assert_not_reached ();
        }

        return FALSE;
}

gboolean
nd_notification_get_is_transient (NdNotification *notification)
{
        return hint_to_boolean (notification, "transient");
}

gboolean
nd_notification_get_is_resident (NdNotification *notification)
{
        return hint_to_boolean (notification, "resident");
}

gboolean
nd_notification_get_action_icons (NdNotification *notification)
{
        return hint_to_boolean (notification, "action-icons");
}

guint32
nd_notification_get_id (NdNotification *notification)
{
        g_return_val_if_fail (ND_IS_NOTIFICATION (notification), -1);

        return notification->id;
}

GHashTable *
nd_notification_get_hints (NdNotification *notification)
{
        g_return_val_if_fail (ND_IS_NOTIFICATION (notification), NULL);

        return notification->hints;
}

char **
nd_notification_get_actions (NdNotification *notification)
{
        g_return_val_if_fail (ND_IS_NOTIFICATION (notification), NULL);

        return notification->actions;
}

const char *
nd_notification_get_sender (NdNotification *notification)
{
        g_return_val_if_fail (ND_IS_NOTIFICATION (notification), NULL);

        return notification->sender;
}

const char *
nd_notification_get_summary (NdNotification *notification)
{
        g_return_val_if_fail (ND_IS_NOTIFICATION (notification), NULL);

        return notification->summary;
}

const char *
nd_notification_get_body (NdNotification *notification)
{
        g_return_val_if_fail (ND_IS_NOTIFICATION (notification), NULL);

        return notification->body;
}

int
nd_notification_get_timeout (NdNotification *notification)
{
        g_return_val_if_fail (ND_IS_NOTIFICATION (notification), -1);

        return notification->timeout;
}

static GdkPixbuf *
scale_pixbuf (GdkPixbuf *pixbuf,
              int        max_width,
              int        max_height,
              gboolean   no_stretch_hint)
{
        int        pw;
        int        ph;
        float      scale_factor_x = 1.0;
        float      scale_factor_y = 1.0;
        float      scale_factor = 1.0;

        pw = gdk_pixbuf_get_width (pixbuf);
        ph = gdk_pixbuf_get_height (pixbuf);

        /* Determine which dimension requires the smallest scale. */
        scale_factor_x = (float) max_width / (float) pw;
        scale_factor_y = (float) max_height / (float) ph;

        if (scale_factor_x > scale_factor_y) {
                scale_factor = scale_factor_y;
        } else {
                scale_factor = scale_factor_x;
        }

        /* always scale down, allow to disable scaling up */
        if (scale_factor < 1.0f || !no_stretch_hint) {
                int scale_x;
                int scale_y;

                scale_x = (int) (pw * scale_factor);
                scale_y = (int) (ph * scale_factor);
                return gdk_pixbuf_scale_simple (pixbuf,
                                                scale_x,
                                                scale_y,
                                                GDK_INTERP_BILINEAR);
        } else {
                return g_object_ref (pixbuf);
        }
}

static void
free_pixels (guchar   *pixels,
             gpointer  user_data)
{
  g_free (pixels);
}

static GdkPixbuf *
_notify_daemon_pixbuf_from_data_hint (GVariant *icon_data,
                                      int       size)
{
        gboolean        has_alpha;
        int             bits_per_sample;
        int             width;
        int             height;
        int             rowstride;
        int             n_channels;
        GVariant       *data_variant;
        gsize           expected_len;
        guchar         *data;
        GdkPixbuf      *pixbuf;

        g_variant_get (icon_data,
                       "(iiibii@ay)",
                       &width,
                       &height,
                       &rowstride,
                       &has_alpha,
                       &bits_per_sample,
                       &n_channels,
                       &data_variant);

        expected_len = (height - 1) * rowstride + width
                * ((n_channels * bits_per_sample + 7) / 8);

        if (expected_len != g_variant_get_size (data_variant)) {
                g_warning ("Expected image data to be of length %" G_GSIZE_FORMAT
                           " but got a " "length of %" G_GSIZE_FORMAT,
                           expected_len,
                           g_variant_get_size (data_variant));
                return NULL;
        }

        data = (guchar *) g_memdup (g_variant_get_data (data_variant),
                                    g_variant_get_size (data_variant));

        pixbuf = gdk_pixbuf_new_from_data (data,
                                           GDK_COLORSPACE_RGB,
                                           has_alpha,
                                           bits_per_sample,
                                           width,
                                           height,
                                           rowstride,
                                           free_pixels,
                                           NULL);
        if (pixbuf != NULL && size > 0) {
                GdkPixbuf *scaled;
                scaled = scale_pixbuf (pixbuf, size, size, TRUE);
                g_object_unref (pixbuf);
                pixbuf = scaled;
        }

        return pixbuf;
}

static GdkPixbuf *
_notify_daemon_pixbuf_from_path (const char *path,
                                 int         size)
{
        GFile *file;
        GdkPixbuf *pixbuf = NULL;

        file = g_file_new_for_commandline_arg (path);
        if (g_file_is_native (file)) {
                char *realpath;

                realpath = g_file_get_path (file);
                pixbuf = gdk_pixbuf_new_from_file_at_size (realpath, size, size, NULL);
                g_free (realpath);
        }
        g_object_unref (file);

        if (pixbuf == NULL) {
                /* Load icon theme icon */
                GtkIconTheme *theme;
                GtkIconInfo  *icon_info;

                theme = gtk_icon_theme_get_default ();
                icon_info = gtk_icon_theme_lookup_icon (theme,
                                                        path,
                                                        size,
                                                        GTK_ICON_LOOKUP_USE_BUILTIN);

                if (icon_info != NULL) {
                        gint icon_size;

                        icon_size = MIN (size,
                                         gtk_icon_info_get_base_size (icon_info));

                        if (icon_size == 0)
                                icon_size = size;

                        pixbuf = gtk_icon_theme_load_icon (theme,
                                                           path,
                                                           icon_size,
                                                           GTK_ICON_LOOKUP_USE_BUILTIN,
                                                           NULL);

                        g_object_unref (icon_info);
                }
        }

        return pixbuf;
}

GdkPixbuf *
nd_notification_load_image (NdNotification *notification,
                            int             size)
{
        GVariant  *data;
        GdkPixbuf *pixbuf;

        pixbuf = NULL;

        if ((data = (GVariant *) g_hash_table_lookup (notification->hints, "image-data"))
            || (data = (GVariant *) g_hash_table_lookup (notification->hints, "image_data"))) {
                pixbuf = _notify_daemon_pixbuf_from_data_hint (data, size);
        } else if ((data = (GVariant *) g_hash_table_lookup (notification->hints, "image-path"))
                   || (data = (GVariant *) g_hash_table_lookup (notification->hints, "image_path"))) {
                if (g_variant_is_of_type (data, G_VARIANT_TYPE_STRING)) {
                        const char *path;
                        path = g_variant_get_string (data, NULL);
                        pixbuf = _notify_daemon_pixbuf_from_path (path, size);
                } else {
                        g_warning ("Expected image_path hint to be of type string");
                }
        } else if (*notification->app_icon != '\0') {
                pixbuf = _notify_daemon_pixbuf_from_path (notification->app_icon, size);
        } else if ((data = (GVariant *) g_hash_table_lookup (notification->hints, "icon_data"))) {
                g_warning("\"icon_data\" hint is deprecated, please use \"image_data\" instead");
                pixbuf = _notify_daemon_pixbuf_from_data_hint (data, size);
        }

        return pixbuf;
}

void
nd_notification_close (NdNotification            *notification,
                       NdNotificationClosedReason reason)
{
        g_return_if_fail (ND_IS_NOTIFICATION (notification));

        g_object_ref (notification);
        g_signal_emit (notification, signals[CLOSED], 0, reason);
        g_object_unref (notification);

        notification->is_closed = TRUE;
}

void
nd_notification_action_invoked (NdNotification  *notification,
                                const char      *action)
{
        g_return_if_fail (ND_IS_NOTIFICATION (notification));

        g_object_ref (notification);
        g_signal_emit (notification, signals[ACTION_INVOKED], 0, action);
        g_object_unref (notification);
}

NdNotification *
nd_notification_new (const char *sender)
{
        NdNotification *notification;

        notification = (NdNotification *) g_object_new (ND_TYPE_NOTIFICATION, NULL);
        notification->sender = g_strdup (sender);

        return notification;
}

gboolean
validate_markup (const gchar *markup)
{
  gchar *parsed_markup;
  GError *error;

  parsed_markup = NULL;
  error = NULL;

  if (!parse_markup (markup, &parsed_markup, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      return FALSE;
    }

  if (!pango_parse_markup (parsed_markup, -1, 0, NULL, NULL, NULL, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      g_free (parsed_markup);

      return FALSE;
    }

  g_free (parsed_markup);

  return TRUE;
}
