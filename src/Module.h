#pragma once

#include <map>
#include <string>
#include <vector>

#include "Tag.h"
#include "Types.h"

namespace ffi {
struct ModuleContents {
  std::map<std::string, Type> entities;
  std::map<std::string, Tag> tags;
  std::vector<std::string> imports;
};

using Module = std::pair<std::string, ModuleContents>;
}  // namespace ffi
