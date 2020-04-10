#pragma once

#include <string>

#include <llvm/ObjectYAML/YAML.h>

#include "Types.h"

namespace ffi {
struct Entity {
  uintptr_t unique_id;
  std::string name;
  Type type;
};
}  // namespace ffi
