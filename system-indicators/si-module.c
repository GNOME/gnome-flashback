/*
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

#include "config.h"

#include <glib/gi18n-lib.h>
#include <libgnome-panel/gp-module.h>

#include "si-applet.h"

static GpAppletInfo *
si_get_applet_info (const char *id)
{
  const char *name;
  const char *description;
  const char *icon;
  GpAppletInfo *info;

  name = _("System Indicators");
  description = _("This applet contains system indicators");
  icon = "applications-system";

  info = gp_applet_info_new (si_applet_get_type, name, description, icon);

  return info;
}

void
gp_module_load (GpModule *module)
{
  bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  gp_module_set_gettext_domain (module, GETTEXT_PACKAGE);

  gp_module_set_abi_version (module, GP_MODULE_ABI_VERSION);

  gp_module_set_id (module, "org.gnome.gnome-flashback.system-indicators");
  gp_module_set_version (module, PACKAGE_VERSION);

  gp_module_set_applet_ids (module, "system-indicators", NULL);

  gp_module_set_get_applet_info (module, si_get_applet_info);
}
