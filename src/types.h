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

#include <variant>

#include "function_type.h"
#include "opaque_type.h"
#include "pointer_type.h"
#include "prim_types.h"

namespace ffi {
struct c_type {
  template <typename T, typename... Ts>
  using variant_drop_1 = std::variant<Ts...>;

  using variant = variant_drop_1<void
#define TYPE(TypeName) , TypeName
#include "types.def"
                                 >;
  variant value;
};

using entity = std::pair<std::string, c_type>;
using const_entity = const std::pair<const std::string, c_type>;

namespace internal {
template <typename L, typename A>
struct index;

template <template <typename...> class V, typename A, typename... Args>
struct index<V<A, Args...>, A> {
  static constexpr ptrdiff_t value = 0;
};

template <template <typename...> class V, typename B, typename... Args,
          typename A>
struct index<V<B, Args...>, A> {
  static constexpr ptrdiff_t value = 1 + index<V<Args...>, A>::value;
};
}  // namespace internal

template <typename L, typename A>
constexpr ptrdiff_t index = internal::index<L, A>::value;

bool is_marshallable(const c_type& type) noexcept;
}  // namespace ffi
