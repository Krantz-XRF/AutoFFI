#pragma once

#include <memory>

namespace ffi
{
struct Type;
struct PointerType
{
    std::unique_ptr<Type> pointee;
};
} // namespace ffi
