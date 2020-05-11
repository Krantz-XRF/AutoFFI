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
enum class name_case : uint8_t {
  preserving,
  camel,
  snake_all_lower,
  snake_init_upper,
  snake_all_upper,
};

enum class name_variant : uint8_t {
  preserving,
  variable,
  module_name,
  type_ctor,
  data_ctor,
};

struct name_converter {
  name_case output_case{name_case::preserving};
  name_variant output_variant{name_variant::preserving};
  name_converter* forward_converter{nullptr};
  bool afterward{false};
  std::string remove_prefix{};
  std::string remove_suffix{};
  std::string add_prefix{};
  std::string add_suffix{};

  [[nodiscard]] std::string convert(std::string_view) const noexcept;
};

using name_converter_map = std::map<std::string, name_converter, std::less<>>;
}  // namespace ffi
