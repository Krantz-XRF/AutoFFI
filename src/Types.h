#pragma once

#include <variant>

#include <llvm/ObjectYAML/YAML.h>

#include "Function.h"
#include "OpaqueType.h"
#include "PointerType.h"
#include "PrimTypes.h"

namespace ffi {
struct Type {
  template <typename T, typename... Ts>
  using variant_drop_1 = std::variant<Ts...>;

  using type = variant_drop_1<void
#define TYPE(TypeName) , TypeName
#include "Types.def"
                              >;
  type value;
};

using Entity = std::pair<std::string, Type>;
using ConstEntity = const std::pair<const std::string, Type>;

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

bool is_marshallable(const Type& type) noexcept;
}  // namespace ffi
