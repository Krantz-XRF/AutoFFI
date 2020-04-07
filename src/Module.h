#pragma once

#include <map>
#include <string>
#include <vector>

#include "Entity.h"
#include "Tag.h"

namespace ffi {
struct ModuleContents {
  std::vector<Entity> entities;
  std::map<std::string, Tag> tags;
  std::vector<std::string> imports;
};

using Module = std::pair<std::string, ModuleContents>;
}  // namespace ffi
