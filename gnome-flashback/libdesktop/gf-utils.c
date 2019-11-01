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
#include "gf-utils.h"

char *
gf_build_attributes_list (const char *first,
                          ...)
{
  GString *attributes;
  va_list args;
  const char *attribute;

  attributes = g_string_new (first);
  va_start (args, first);

  while ((attribute = va_arg (args, const char *)) != NULL)
    g_string_append_printf (attributes, ",%s", attribute);

  va_end (args);

  return g_string_free (attributes, FALSE);
}
