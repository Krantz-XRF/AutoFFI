#pragma once

#include <string>

#include <llvm/ObjectYAML/YAML.h>

#include "Types.h"

namespace ffi {
struct Entity {
  std::string name;
  Type type;
};
}  // namespace ffi
