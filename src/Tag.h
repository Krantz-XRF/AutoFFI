/* This file is part of auto-FFI (https://github.com/Krantz-XRF/auto-FFI).
 * Copyright (C) 2020 Xie Ruifeng
 *
 * auto-FFI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * auto-FFI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with auto-FFI.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>
#include <variant>
#include <vector>

#include "Types.h"

namespace ffi {
struct Structure {
  std::vector<Entity> fields;
};

struct Enumeration {
  Type underlying_type;
  std::map<std::string, intmax_t> values;
};

struct Tag {
  std::variant<Structure, Enumeration> payload;
};

using TagDecl = std::pair<std::string, Tag>;
}  // namespace ffi
