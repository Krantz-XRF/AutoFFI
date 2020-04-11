#pragma once

#include <memory>
#include <string>
#include <vector>

namespace ffi {
struct Type;
using Entity = std::pair<std::string, Type>;

struct FunctionType {
  std::unique_ptr<Type> returnType;
  std::vector<Entity> params;
};
}  // namespace ffi
