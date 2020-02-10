#pragma once

#include <memory>
#include <vector>

namespace ffi
{
struct Type;
struct Entity;

struct FunctionType
{
    std::unique_ptr<Type> returnType;
    std::vector<Entity> params;
};
} // namespace ffi
