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

#include <nlohmann/json.hpp>

#include "config.h"
#include "module.h"

namespace ffi {
void to_json(nlohmann::json& j, const config& cfg);
void from_json(const nlohmann::json& j, config& cfg);

void to_json(nlohmann::json& j, const module_contents& mod);

void to_json(nlohmann::json& j, const structure& tag);
void to_json(nlohmann::json& j, const_entity& val);

struct ctype_ref {
  const ctype* pointee;
};
inline ctype_ref tref(const ctype& t) { return ctype_ref{&t}; }

void to_json(nlohmann::json& j, const ctype_ref& t);
void from_json(const nlohmann::json& j, ctype_ref& t);

NLOHMANN_JSON_SERIALIZE_ENUM(name_variant,
                             {{name_variant::preserving, "preserving"},
                              {name_variant::variable, "variable"},
                              {name_variant::module_name, "module_name"},
                              {name_variant::type_ctor, "type_ctor"},
                              {name_variant::data_ctor, "data_ctor"}})
}  // namespace ffi
