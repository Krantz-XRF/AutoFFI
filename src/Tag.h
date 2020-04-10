#pragma once

#include <string>
#include <variant>
#include <vector>

#include "Types.h"

namespace ffi {
struct Structure {};

struct Enumeration {
  Type underlying_type;
  std::vector<std::pair<std::string, intmax_t>> values;
};

struct Tag {
  uintptr_t unique_id;
  std::variant<Structure, Enumeration> payload;
};

using TagDecl = std::pair<std::string, Tag>;
}  // namespace ffi
