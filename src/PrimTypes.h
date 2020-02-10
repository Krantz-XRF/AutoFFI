#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <clang/AST/Type.h>
#include <llvm/ObjectYAML/YAML.h>

namespace ffi
{
using czstring = const char*;

constexpr czstring name_of(clang::BuiltinType::Kind k);

enum class Class
{
    OpenCL,
    General,
    Signed,
    Unsigned,
    Floating,
    Placeholder,
};

constexpr Class class_of(clang::BuiltinType::Kind) noexcept;
constexpr bool is_open_cl(clang::BuiltinType::Kind) noexcept;
constexpr bool is_signed(clang::BuiltinType::Kind) noexcept;
constexpr bool is_unsigned(clang::BuiltinType::Kind) noexcept;
constexpr bool is_floating(clang::BuiltinType::Kind) noexcept;
constexpr bool is_placeholder(clang::BuiltinType::Kind) noexcept;

struct ScalarType
{
    enum Signedness : int8_t
    {
#define PRIM_TYPE_SIGN(Name) Name,
#define PRIM_TYPE_SIGN_SYNONYM(Name, Target) Name = Target,
#include "PrimTypes.def"
    };
    enum Qualifier : int8_t
    {
#define PRIM_TYPE_QUALIFIER(Name) Name,
#include "PrimTypes.def"
    };
    enum Width : int8_t
    {
#define PRIM_TYPE_WIDTH(Value) Width##Value,
#include "PrimTypes.def"
    };
    Signedness sign;
    Qualifier qualifier;
    Width width;

    std::string as_haskell() const noexcept;

    static std::optional<ScalarType> from_clang(
        clang::BuiltinType::Kind type) noexcept;
    static std::optional<ScalarType> from_name(std::string_view type) noexcept;
};

constexpr ScalarType::Signedness signedness_of(clang::BuiltinType::Kind) noexcept;

constexpr czstring to_string(ScalarType::Signedness s) noexcept;
constexpr czstring to_string(ScalarType::Qualifier q) noexcept;
constexpr czstring to_string(ScalarType::Width w) noexcept;
} // namespace ffi

constexpr ffi::czstring ffi::name_of(clang::BuiltinType::Kind k)
{
    constexpr std::array<const char*, clang::BuiltinType::LastKind + 1> names{
// OpenCL image types
#define IMAGE_TYPE(ImgType, Id, SingletonId, Access, Suffix) #ImgType,
#include "clang/Basic/OpenCLImageTypes.def"
// OpenCL extension types
#define EXT_OPAQUE_TYPE(ExtType, Id, Ext) #ExtType,
#include "clang/Basic/OpenCLExtensionTypes.def"
// All other builtin types
#define BUILTIN_TYPE(Id, SingletonId) #Id,
#define LAST_BUILTIN_TYPE(Id)
#include "clang/AST/BuiltinTypes.def"
    };
    return names[k];
}

constexpr ffi::Class ffi::class_of(clang::BuiltinType::Kind k) noexcept
{
    constexpr std::array<Class, clang::BuiltinType::LastKind + 1> kinds{
// OpenCL image types
#define IMAGE_TYPE(ImgType, Id, SingletonId, Access, Suffix) Class::OpenCL,
#include "clang/Basic/OpenCLImageTypes.def"
// OpenCL extension types
#define EXT_OPAQUE_TYPE(ExtType, Id, Ext) Class::OpenCL,
#include "clang/Basic/OpenCLExtensionTypes.def"
// All other builtin types
#define BUILTIN_TYPE(Id, SingletonId) Class::General,
#define SIGNED_TYPE(Id, SingletonId) Class::Signed,
#define UNSIGNED_TYPE(Id, SingletonId) Class::Unsigned,
#define FLOATING_TYPE(Id, SingletonId) Class::Floating,
#define PLACEHOLDER_TYPE(Id, SingletonId) Class::Placeholder,
#define LAST_BUILTIN_TYPE(Id)
#include "clang/AST/BuiltinTypes.def"
    };
    return kinds[k];
}

constexpr bool ffi::is_open_cl(clang::BuiltinType::Kind k) noexcept
{
    return class_of(k) == Class::OpenCL;
}

constexpr bool ffi::is_signed(clang::BuiltinType::Kind k) noexcept
{
    return class_of(k) == Class::Signed;
}

constexpr bool ffi::is_unsigned(clang::BuiltinType::Kind k) noexcept
{
    return class_of(k) == Class::Unsigned;
}

constexpr bool ffi::is_floating(clang::BuiltinType::Kind k) noexcept
{
    return class_of(k) == Class::Floating;
}

constexpr bool ffi::is_placeholder(clang::BuiltinType::Kind k) noexcept
{
    return class_of(k) == Class::Placeholder;
}

constexpr ffi::ScalarType::Signedness ffi::signedness_of(
    clang::BuiltinType::Kind k) noexcept
{
    return is_signed(k) ? ScalarType::Signed : ScalarType::Unsigned;
}

constexpr ffi::czstring ffi::to_string(ScalarType::Signedness s) noexcept
{
    constexpr const char* names[]{
#define PRIM_TYPE_SIGN(Name) #Name,
#include "PrimTypes.def"
    };
    return names[s];
}

constexpr ffi::czstring ffi::to_string(ScalarType::Qualifier q) noexcept
{
    constexpr const char* names[]{
#define PRIM_TYPE_QUALIFIER(Name) #Name,
#include "PrimTypes.def"
    };
    return names[q];
}

constexpr ffi::czstring ffi::to_string(ScalarType::Width w) noexcept
{
    constexpr const char* names[]{
#define PRIM_TYPE_WIDTH(Value) #Value,
#include "PrimTypes.def"
    };
    return names[w];
}

template <>
struct llvm::yaml::ScalarEnumerationTraits<ffi::ScalarType::Signedness>
{
    static void enumeration(IO& io, ffi::ScalarType::Signedness& s)
    {
#define PRIM_TYPE_SIGN(Name) io.enumCase(s, #Name, ffi::ScalarType::Name);
#include "PrimTypes.def"
    }
};

template <>
struct llvm::yaml::ScalarEnumerationTraits<ffi::ScalarType::Qualifier>
{
    static void enumeration(IO& io, ffi::ScalarType::Qualifier& q)
    {
#define PRIM_TYPE_QUALIFIER(Name) io.enumCase(q, #Name, ffi::ScalarType::Name);
#include "PrimTypes.def"
    }
};

template <>
struct llvm::yaml::ScalarEnumerationTraits<ffi::ScalarType::Width>
{
    static void enumeration(IO& io, ffi::ScalarType::Width& w)
    {
#define PRIM_TYPE_WIDTH(Name) io.enumCase(w, #Name, ffi::ScalarType::Width##Name);
#include "PrimTypes.def"
    }
};

template <>
struct llvm::yaml::MappingTraits<ffi::ScalarType>
{
    static void mapping(IO& io, ffi::ScalarType& scalar)
    {
        io.mapRequired("sign", scalar.sign);
        io.mapRequired("qualifier", scalar.qualifier);
        io.mapRequired("width", scalar.width);
    }
};
