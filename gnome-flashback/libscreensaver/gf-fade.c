/*
 * Copyright (C) 2004-2009 William Jon McCann
 * Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2020 Alberts Muktupāvels
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
 *
 * Authors:
 *     Alberts Muktupāvels <alberts.muktupavels@gmail.com>
 *     William Jon McCann <mccann@jhu.edu>
 */

#include "config.h"
#include "gf-fade.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>

#define XF86_MIN_GAMMA 0.1f

/* VidModeExtension version 2.0 or better is needed to do gamma.
 * 2.0 added gamma values; 2.1 added gamma ramps.
 */
#define XF86_VIDMODE_GAMMA_MIN_MAJOR 2
#define XF86_VIDMODE_GAMMA_MIN_MINOR 0
#define XF86_VIDMODE_GAMMA_RAMP_MIN_MAJOR 2
#define XF86_VIDMODE_GAMMA_RAMP_MIN_MINOR 1

typedef enum
{
  GF_FADE_TYPE_NONE,
  GF_FADE_TYPE_GAMMA_NUMBER,
  GF_FADE_TYPE_GAMMA_RAMP
} GfFadeType;

typedef struct
{
  int             size;
  unsigned short *r;
  unsigned short *g;
  unsigned short *b;
} GfGammaInfo;

struct _GfFade
{
  GObject           parent;

  GfFadeType        fade_type;

  gboolean          (* fade_setup)           (GfFade *self);

  gboolean          (* fade_set_alpha_gamma) (GfFade *self,
                                              double  alpha);

  void              (* fade_finish)          (GfFade *self);

  int               num_ramps;

  GfGammaInfo      *info;

  XF86VidModeGamma  vmg;

  GList            *tasks;

  double            alpha_per_iter;
  double            current_alpha;

  guint             timeout_id;
};

G_DEFINE_TYPE (GfFade, gf_fade, G_TYPE_OBJECT)

static void
screen_fade_finish (GfFade *self)
{
  int i;

  if (self->info == NULL)
    return;

  for (i = 0; i < self->num_ramps; i++)
    {
      g_clear_pointer (&self->info[i].r, g_free);
      g_clear_pointer (&self->info[i].g, g_free);
      g_clear_pointer (&self->info[i].b, g_free);
    }

  g_clear_pointer (&self->info, g_free);
}

static gboolean
setup_gamma_ramp (GfFade *self)
{
  GdkDisplay *display;
  Display *xdisplay;
  gboolean res;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  res = XF86VidModeGetGammaRampSize (xdisplay,
                                     XDefaultScreen (xdisplay),
                                     &self->info->size);

  if (!res || self->info->size <= 0)
    return FALSE;

  self->info->r = g_new0 (unsigned short, self->info->size);
  self->info->g = g_new0 (unsigned short, self->info->size);
  self->info->b = g_new0 (unsigned short, self->info->size);

  if (!(self->info->r && self->info->g && self->info->b))
    return FALSE;

  return XF86VidModeGetGammaRamp (xdisplay,
                                  XDefaultScreen (xdisplay),
                                  self->info->size,
                                  self->info->r,
                                  self->info->g,
                                  self->info->b);
}

static gboolean
gamma_fade_setup (GfFade *self)
{
  if (self->info != NULL)
    return TRUE;

  self->num_ramps = 1;
  self->info = g_new0 (GfGammaInfo, 1);

  if (self->fade_type == GF_FADE_TYPE_GAMMA_RAMP)
    {
      /* have ramps */

      if (setup_gamma_ramp (self))
        {
          g_debug ("Initialized gamma ramp fade");
          return TRUE;
        }

      self->fade_type = GF_FADE_TYPE_GAMMA_NUMBER;
    }

  if (self->fade_type == GF_FADE_TYPE_GAMMA_NUMBER)
    {
      GdkDisplay *display;
      Display *xdisplay;

      /* only have gamma parameter, not ramps. */

      display = gdk_display_get_default ();
      xdisplay = gdk_x11_display_get_xdisplay (display);

      if (XF86VidModeGetGamma (xdisplay, XDefaultScreen (xdisplay), &self->vmg))
        {
          g_debug ("Initialized gamma fade: %f %f %f",
                   (double) self->vmg.red,
                   (double) self->vmg.green,
                   (double) self->vmg.blue);

          return TRUE;
        }
    }

  self->fade_type = GF_FADE_TYPE_NONE;
  return FALSE;
}

static void
xf86_whack_gamma (GfFade *self,
                  float  ratio)
{
  GdkDisplay *display;
  Display *xdisplay;

  if (self->info == NULL)
    return;

  if (ratio < 0.0f)
    ratio = 0.0;
  else if (ratio > 1.0f)
    ratio = 1.0;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  gdk_x11_display_error_trap_push (display);

  if (self->info->size == 0)
    {
      XF86VidModeGamma g2;

      /* we only have a gamma number, not a ramp. */

      g2.red = self->vmg.red * ratio;
      g2.green = self->vmg.green * ratio;
      g2.blue = self->vmg.blue * ratio;

      if (g2.red < XF86_MIN_GAMMA)
        g2.red = XF86_MIN_GAMMA;

      if (g2.green < XF86_MIN_GAMMA)
        g2.green = XF86_MIN_GAMMA;

      if (g2.blue < XF86_MIN_GAMMA)
        g2.blue = XF86_MIN_GAMMA;

      XF86VidModeSetGamma (xdisplay, XDefaultScreen (xdisplay), &g2);
    }
  else
    {
      unsigned short *r;
      unsigned short *g;
      unsigned short *b;
      int i;

      r = g_new0 (unsigned short, self->info->size);
      g = g_new0 (unsigned short, self->info->size);
      b = g_new0 (unsigned short, self->info->size);

      for (i = 0; i < self->info->size; i++)
        {
          r[i] = self->info->r[i] * ratio;
          g[i] = self->info->g[i] * ratio;
          b[i] = self->info->b[i] * ratio;
        }

      XF86VidModeSetGammaRamp (xdisplay,
                               XDefaultScreen (xdisplay),
                               self->info->size,
                               r, g, b);

      g_free (r);
      g_free (g);
      g_free (b);
    }

  gdk_x11_display_error_trap_pop_ignored (display);

  gdk_display_flush (display);
}

static gboolean
gamma_fade_set_alpha_gamma (GfFade *self,
                            double  alpha)
{
  xf86_whack_gamma (self, alpha);
  return TRUE;
}

static void
check_gamma_extension (GfFade *self)
{
  GdkDisplay *display;
  Display *xdisplay;
  int event;
  int error;
  gboolean res;
  int major;
  int minor;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  if (!XF86VidModeQueryExtension (xdisplay, &event, &error))
    return;

  gdk_x11_display_error_trap_push (display);

  res = XF86VidModeQueryVersion (xdisplay, &major, &minor);

  if (gdk_x11_display_error_trap_pop (display) != 0 || !res)
    return;

  g_debug ("Gamma extension version: major - %d, minor - %d", major, minor);

  if (major < XF86_VIDMODE_GAMMA_MIN_MAJOR ||
      (major == XF86_VIDMODE_GAMMA_MIN_MAJOR &&
       minor < XF86_VIDMODE_GAMMA_MIN_MINOR))
    return;

  self->fade_setup = gamma_fade_setup;
  self->fade_finish = screen_fade_finish;
  self->fade_set_alpha_gamma = gamma_fade_set_alpha_gamma;

  if (major < XF86_VIDMODE_GAMMA_RAMP_MIN_MAJOR ||
      (major == XF86_VIDMODE_GAMMA_RAMP_MIN_MAJOR &&
       minor < XF86_VIDMODE_GAMMA_RAMP_MIN_MINOR))
    {
      self->fade_type = GF_FADE_TYPE_GAMMA_NUMBER;
      return;
    }

  self->fade_type = GF_FADE_TYPE_GAMMA_RAMP;
}

static void
check_extensions (GfFade *self)
{
  if (self->fade_type == GF_FADE_TYPE_NONE)
    check_gamma_extension (self);

  g_debug ("Fade type: %d", self->fade_type);
}

static gboolean
set_alpha (GfFade  *self,
           gdouble  alpha)
{
  switch (self->fade_type)
    {
      case GF_FADE_TYPE_GAMMA_RAMP:
      case GF_FADE_TYPE_GAMMA_NUMBER:
        if (self->fade_set_alpha_gamma != NULL)
          self->fade_set_alpha_gamma (self, alpha);
        return TRUE;

      case GF_FADE_TYPE_NONE:
        break;

      default:
        g_warning ("Unknown fade type");
        break;
    }

  return FALSE;
}

static void
fade_stop (GfFade *self)
{
  if (self->timeout_id != 0)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
    }
}

static gboolean
fade_out_iter (GfFade *self)
{
  if (self->current_alpha < 0.01)
    return FALSE;

  self->current_alpha -= self->alpha_per_iter;

  return set_alpha (self, self->current_alpha);
}

static void
gf_fade_complete (GfFade *self)
{
  GList *l;

  if (self->tasks == NULL)
    return;

  fade_stop (self);

  for (l = self->tasks; l != NULL; l = l->next)
    g_task_return_boolean (G_TASK (l->data), TRUE);

  g_list_free_full (self->tasks, g_object_unref);
  self->tasks = NULL;
}

static gboolean
fade_out_cb (gpointer user_data)
{
  GfFade *self;

  self = GF_FADE (user_data);

  if (!fade_out_iter (self))
    {
      gf_fade_complete (self);
      self->timeout_id = 0;

      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
fade_start (GfFade *self,
            guint   timeout)
{
  fade_stop (self);

  if (self->fade_type != GF_FADE_TYPE_NONE)
    {
      guint steps_per_sec;
      guint num_steps;

      if (!self->fade_setup (self))
        {
          gf_fade_complete (self);
          return;
        }

      steps_per_sec = 60;
      num_steps = (timeout / 1000.0) * steps_per_sec;

      self->alpha_per_iter = 1.0 / (double) num_steps;

      self->timeout_id = g_timeout_add (1000 / steps_per_sec, fade_out_cb, self);
      g_source_set_name_by_id (self->timeout_id, "[gnome-flashback] fade_out_cb");
    }
  else
    {
      gf_fade_complete (self);
    }
}

static void
gf_fade_dispose (GObject *object)
{
  GfFade *self;

  self = GF_FADE (object);

  fade_stop (self);

  if (self->fade_finish != NULL)
    self->fade_finish (self);

  if (self->tasks != NULL)
    {
      g_list_free_full (self->tasks, g_object_unref);
      self->tasks = NULL;
    }

  G_OBJECT_CLASS (gf_fade_parent_class)->dispose (object);
}

static void
gf_fade_class_init (GfFadeClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->dispose = gf_fade_dispose;
}

static void
gf_fade_init (GfFade *self)
{
  self->fade_type = GF_FADE_TYPE_NONE;
  self->current_alpha = 1.0;

  check_extensions (self);
}

GfFade *
gf_fade_new (void)
{
  return g_object_new (GF_TYPE_FADE, NULL);
}

void
gf_fade_async (GfFade              *self,
               guint                timeout,
               GCancellable        *cancellable,
               GAsyncReadyCallback  callback,
               gpointer             user_data)
{
  GTask *task;

  fade_stop (self);

  task = g_task_new (self, cancellable, callback, user_data);
  self->tasks = g_list_prepend (self->tasks, task);

  fade_start (self, timeout);
}

gboolean
gf_fade_finish (GfFade        *self,
                GAsyncResult  *result,
                GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
gf_fade_reset (GfFade *self)
{
  g_debug ("Resetting fade");

  fade_stop (self);

  self->current_alpha = 1.0;
  set_alpha (self, self->current_alpha);

  if (self->fade_finish != NULL)
    self->fade_finish (self);
}
