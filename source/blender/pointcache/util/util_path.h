/*
 * Copyright 2013, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef PTC_UTIL_PATH_H
#define PTC_UTIL_PATH_H

#include <string>

struct ID;

namespace PTC {
namespace Util {

/* XXX make these configurable, just copied from BKE_pointcache for now */
#define PTC_EXTENSION ".abc"
#define PTC_DIRECTORY "blendcache_"

std::string archive_path(const std::string &name, int index, const std::string &path, ID *id,
                         bool do_path, bool do_ext, bool is_external, bool ignore_libpath);

} /* namespace Util */
} /* namespace PTC */

#endif  /* PTC_UTIL_PATH_H */
