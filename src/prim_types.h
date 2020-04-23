/* This file is part of auto-FFI (https://github.com/Krantz-XRF/auto-FFI).
 * Copyright (C) 2020 Xie Ruifeng
 *
 * auto-FFI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * auto-FFI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with auto-FFI.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <clang/AST/Type.h>
#include <llvm/ObjectYAML/YAML.h>

namespace ffi {
using czstring = const char*;

constexpr czstring name_of(clang::BuiltinType::Kind k);

enum class Class {
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

struct scalar_type {
  enum Signedness : int8_t {
#define PRIM_TYPE_SIGN(Name) Name,
#define PRIM_TYPE_SIGN_SYNONYM(Name, Target) Name = Target,
#include "prim_types.def"
  };
  enum Qualifier : int8_t {
#define PRIM_TYPE_QUALIFIER(Name) Name,
#include "prim_types.def"
  };
  enum Width : int8_t {
#define PRIM_TYPE_WIDTH(Value) Width##Value,
#include "prim_types.def"
  };
  Signedness sign;
  Qualifier qualifier;
  Width width;

  [[nodiscard]] std::string as_haskell() const noexcept;

  static std::optional<scalar_type> from_clang(
      clang::BuiltinType::Kind type) noexcept;
  static std::optional<scalar_type> from_name(std::string_view type) noexcept;
};

constexpr scalar_type::Signedness signedness_of(
    clang::BuiltinType::Kind) noexcept;

constexpr czstring to_string(scalar_type::Signedness s) noexcept;
constexpr czstring to_string(scalar_type::Qualifier q) noexcept;
constexpr czstring to_string(scalar_type::Width w) noexcept;
}  // namespace ffi

constexpr ffi::czstring ffi::name_of(clang::BuiltinType::Kind k) {
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

constexpr ffi::Class ffi::class_of(clang::BuiltinType::Kind k) noexcept {
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

constexpr bool ffi::is_open_cl(clang::BuiltinType::Kind k) noexcept {
  return class_of(k) == Class::OpenCL;
}

constexpr bool ffi::is_signed(clang::BuiltinType::Kind k) noexcept {
  return class_of(k) == Class::Signed;
}

constexpr bool ffi::is_unsigned(clang::BuiltinType::Kind k) noexcept {
  return class_of(k) == Class::Unsigned;
}

constexpr bool ffi::is_floating(clang::BuiltinType::Kind k) noexcept {
  return class_of(k) == Class::Floating;
}

constexpr bool ffi::is_placeholder(clang::BuiltinType::Kind k) noexcept {
  return class_of(k) == Class::Placeholder;
}

constexpr ffi::scalar_type::Signedness ffi::signedness_of(
    clang::BuiltinType::Kind k) noexcept {
  return is_signed(k) ? scalar_type::Signed : scalar_type::Unsigned;
}

constexpr ffi::czstring ffi::to_string(scalar_type::Signedness s) noexcept {
  constexpr const char* names[]{
#define PRIM_TYPE_SIGN(Name) #Name,
#include "prim_types.def"
  };
  return names[s];
}

constexpr ffi::czstring ffi::to_string(scalar_type::Qualifier q) noexcept {
  constexpr const char* names[]{
#define PRIM_TYPE_QUALIFIER(Name) #Name,
#include "prim_types.def"
  };
  return names[q];
}

constexpr ffi::czstring ffi::to_string(scalar_type::Width w) noexcept {
  constexpr const char* names[]{
#define PRIM_TYPE_WIDTH(Value) #Value,
#include "prim_types.def"
  };
  return names[w];
}

template <>
struct llvm::yaml::ScalarEnumerationTraits<ffi::scalar_type::Signedness> {
  static void enumeration(IO& io, ffi::scalar_type::Signedness& s) {
#define PRIM_TYPE_SIGN(Name) io.enumCase(s, #Name, ffi::scalar_type::Name);
#include "prim_types.def"
  }
};

template <>
struct llvm::yaml::ScalarEnumerationTraits<ffi::scalar_type::Qualifier> {
  static void enumeration(IO& io, ffi::scalar_type::Qualifier& q) {
#define PRIM_TYPE_QUALIFIER(Name) io.enumCase(q, #Name, ffi::scalar_type::Name);
#include "prim_types.def"
  }
};

template <>
struct llvm::yaml::ScalarEnumerationTraits<ffi::scalar_type::Width> {
  static void enumeration(IO& io, ffi::scalar_type::Width& w) {
#define PRIM_TYPE_WIDTH(Name) \
  io.enumCase(w, #Name, ffi::scalar_type::Width##Name);
#include "prim_types.def"
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::scalar_type> {
  static void mapping(IO& io, ffi::scalar_type& scalar) {
    io.mapRequired("sign", scalar.sign);
    io.mapRequired("qualifier", scalar.qualifier);
    io.mapRequired("width", scalar.width);
  }
};
