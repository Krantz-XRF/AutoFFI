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

#include "json.h"

void ffi::to_json(nlohmann::json& j, const config& cfg) {
  j = nlohmann::json::object({
#define CONFIG(item) {#item, cfg.item},
#define CONFIG_EXTRA(item)
#include "config.def"
  });
}

void ffi::from_json(const nlohmann::json& j, config& cfg) {
#define CONFIG(item) j.at(#item).get_to(cfg.item);
#define CONFIG_EXTRA(item)
#include "config.def"
}

void ffi::to_json(nlohmann::json& j, const module_contents& mod) {
  j = nlohmann::json::object({
      {"imports", mod.imports},
      {"entities", mod.entities},
  });
  auto& structs = j["structs"] = nlohmann::json::array();
  auto& enums = j["enums"] = nlohmann::json::array();
  for (const auto& t : mod.tags)
    if (std::holds_alternative<structure>(t.second.payload))
      structs.push_back(nlohmann::json::object({
          {"name", t.first},
          {"fields", std::get<structure>(t.second.payload)},
      }));
    else {
      const auto& enm = std::get<enumeration>(t.second.payload);
      enums.push_back(nlohmann::json::object({
          {"name", t.first},
          {"underlying_type", tref(enm.underlying_type)},
          {"enumerators", enm.values},
      }));
    }
}

void ffi::to_json(nlohmann::json& j, const structure& tag) {
  j = nlohmann::json::array();
  for (const auto& [name, type] : tag.fields)
    j.push_back(nlohmann::json::object({
        {"name", name},
        {"type", tref(type)},
    }));
}

void ffi::to_json(nlohmann::json& j, const_entity& val) {
  j = nlohmann::json::object({
      {"name", val.first},
      {"type", tref(val.second)},
  });
}

void ffi::to_json(nlohmann::json& j, const ctype_ref& t) {
  const auto address = reinterpret_cast<uintptr_t>(t.pointee);
  j = nlohmann::json::object({{"internal_pointer", address}});
}

void ffi::from_json(const nlohmann::json& j, ctype_ref& t) {
  const auto address = j.at("internal_pointer").get<uintptr_t>();
  t.pointee = reinterpret_cast<const ctype*>(address);
}
