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

#include <map>
#include <string>
#include <string_view>

namespace ffi {
struct marshaller {
  enum Case {
    Case_snake_case,
    Case_Snake_case,
    Case_SNAKE_CASE,
    Case_camelCase,
    Case_PascalCase,
    Case_preserve,
  };

  using pss = std::pair<std::string_view, std::string_view>;
  static pss from_snake_case(std::string_view);
  static pss from_CamelCase(std::string_view);

  static void to_snake_case(std::string&, std::string_view);
  static void to_SNAKE_CASE(std::string&, std::string_view);
  static void to_PascalCase(std::string&, std::string_view);

  [[nodiscard]] bool is_type_case() const;
  [[nodiscard]] bool is_func_case() const;

  Case output_case{Case_preserve};
  std::string remove_prefix;
  std::string remove_suffix;
  std::string add_prefix;
  std::string add_suffix;
  marshaller* forward_marshaller{nullptr};
  bool afterward{false};

  [[nodiscard]] std::string transform_raw(std::string_view) const;
  [[nodiscard]] std::string transform(std::string_view) const;
};

using marshaller_map = std::map<std::string, marshaller>;
}  // namespace ffi
