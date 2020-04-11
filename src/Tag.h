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
