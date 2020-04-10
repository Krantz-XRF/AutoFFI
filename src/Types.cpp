#include "Types.h"

bool ffi::is_marshallable(const Type& type) noexcept {
  return std::holds_alternative<ScalarType>(type.value) ||
         std::holds_alternative<PointerType>(type.value);
}
