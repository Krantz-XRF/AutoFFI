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

#include <map>
#include <string>
#include <type_traits>

#include <llvm/ObjectYAML/YAML.h>

#include "Config.h"
#include "Function.h"
#include "Marshaller.h"
#include "Module.h"
#include "OpaqueType.h"
#include "PointerType.h"
#include "PrimTypes.h"
#include "Tag.h"
#include "Types.h"

template <typename T>
struct llvm::yaml::MappingTraits<std::unique_ptr<T>> {
  static void mapping(IO& io, std::unique_ptr<T>& p) {
    if (!io.outputting()) p.reset(new T);
    MappingTraits<T>::mapping(io, *p);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::FunctionType> {
  static void mapping(IO& io, ffi::FunctionType& func) {
    io.mapRequired("returnType", func.returnType);
    io.mapRequired("params", func.params);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::OpaqueType> {
  static void mapping(IO& io, ffi::OpaqueType& type) {
    io.mapRequired("alias", type.name);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::PointerType> {
  static void mapping(IO& io, ffi::PointerType& type) {
    io.mapRequired("pointee", type.pointee);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::Type> {
  template <typename T>
  static constexpr ptrdiff_t Index = ffi::index<ffi::Type::type, T>;

  static void mapping(IO& io, ffi::Type& type) {
    const auto idx = type.value.index();

    if (false) static_cast<void>(0);
#define TYPE(TypeName)                                          \
  else if (io.mapTag(#TypeName, idx == Index<ffi::TypeName>)) { \
    if (!io.outputting()) type.value.emplace<ffi::TypeName>();  \
    auto& clause = std::get<ffi::TypeName>(type.value);         \
    MappingTraits<ffi::TypeName>::mapping(io, clause);          \
  }
#include "Types.def"
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::Structure> {
  static void mapping(IO& io, ffi::Structure& tag) {
    io.mapRequired("fields", tag.fields);
  }
};

template <>
struct llvm::yaml::CustomMappingTraits<ffi::Enumeration> {
  static void inputOne(IO& io, StringRef key, ffi::Enumeration& enm) {
    auto kstr = key.str();
    int value = 0;
    io.mapRequired(kstr.c_str(), value);
    enm.values.try_emplace(kstr, value);
  }

  static void output(IO& io, ffi::Enumeration& enm) {
    for (auto& [k, v] : enm.values) io.mapRequired(k.c_str(), v);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::Tag> {
  using vtype = decltype(std::declval<ffi::Tag>().payload);
  template <typename T>
  static constexpr ptrdiff_t Index = ffi::index<vtype, T>;

  static void mapping(IO& io, ffi::Tag& tag) {
    const auto idx = tag.payload.index();

    if (io.mapTag("Structure", idx == Index<ffi::Structure>)) {
      if (!io.outputting()) tag.payload.emplace<ffi::Structure>();
      auto& clause = std::get<ffi::Structure>(tag.payload);
      MappingTraits<ffi::Structure>::mapping(io, clause);
    } else if (io.mapTag("Enumeration", idx == Index<ffi::Enumeration>)) {
      if (!io.outputting()) tag.payload.emplace<ffi::Enumeration>();
      auto& clause = std::get<ffi::Enumeration>(tag.payload);
      io.mapRequired("enumerators", clause);
    }
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::Entity> {
  static void mapping(IO& io, ffi::Entity& entity) {
    io.mapRequired("name", entity.first);
    io.mapRequired("type", entity.second);
  }
};
LLVM_YAML_IS_SEQUENCE_VECTOR(ffi::Entity)

template <>
struct llvm::yaml::MappingTraits<ffi::ModuleContents> {
  static void mapping(IO& io, ffi::ModuleContents& mod) {
    io.mapRequired("entities", mod.entities);
    io.mapRequired("tags", mod.tags);
    io.mapRequired("imports", mod.imports);
  }
};

template <typename T>
struct llvm::yaml::CustomMappingTraits<std::map<std::string, T>> {
  static void inputOne(IO& io, StringRef key, std::map<std::string, T>& elem) {
    auto kstr = key.str();
    io.mapRequired(kstr.c_str(), elem[kstr]);
  }

  static void output(IO& io, std::map<std::string, T>& elem) {
    for (auto& [k, v] : elem) io.mapRequired(k.c_str(), v);
  }
};

template <>
struct llvm::yaml::ScalarEnumerationTraits<ffi::Marshaller::Case> {
  static void enumeration(IO& io, ffi::Marshaller::Case& c) {
    io.enumCase(c, "snake_case", ffi::Marshaller::Case_snake_case);
    io.enumCase(c, "Snake_case", ffi::Marshaller::Case_Snake_case);
    io.enumCase(c, "SNAKE_CASE", ffi::Marshaller::Case_SNAKE_CASE);
    io.enumCase(c, "camelCase", ffi::Marshaller::Case_camelCase);
    io.enumCase(c, "PascalCase", ffi::Marshaller::Case_PascalCase);
    io.enumCase(c, "preserve", ffi::Marshaller::Case_preserve);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::Marshaller> {
  static void mapping(IO& io, ffi::Marshaller& m) {
    io.mapOptional("removePrefix", m.remove_prefix);
    io.mapOptional("removeSuffix", m.remove_suffix);
    io.mapOptional("addPrefix", m.add_prefix);
    io.mapOptional("addSuffix", m.add_suffix);
    io.mapOptional("outputCase", m.output_case);
  }
};

template <>
struct llvm::yaml::MappingTraits<config::Config> {
  static void mapping(IO& io, config::Config& cfg) {
#define FIELD_OPTIONAL(Type, Field, Default) io.mapOptional(#Field, cfg.Field);
#define FIELD_REQUIRED(Type, Field, Default) io.mapRequired(#Field, cfg.Field);
#include "Config.def"
  }
  static StringRef validate(IO& io, config::Config& cfg) {
    using Case = ffi::Marshaller::Case;
    if (cfg.Marshaller.output_case != ffi::Marshaller::Case_preserve ||
        !cfg.Marshaller.add_prefix.empty())
      return "The universal marshaller should not specify a case or add "
             "a prefix, for modules, types and functions require "
             "different cases.";
    else if (!std::all_of(cfg.FileMarshallers.cbegin(),
                          cfg.FileMarshallers.cend(), [](const auto& m) {
                            return m.second.output_case ==
                                       ffi::Marshaller::Case_preserve &&
                                   m.second.add_prefix.empty();
                          }))
      return "The file marshallers should not specify a case or add a "
             "prefix, for modules, types and functions require different "
             "cases.";
    else if (!cfg.ModuleMarshaller.is_type_case())
      return "The module marshaller should produce a name beginning with "
             "an upper case letter.";
    else if (!cfg.ConstMarshaller.is_type_case())
      return "The constructor marshaller should produce a name beginning "
             "with an upper case letter.";
    else if (!cfg.TypeMarshaller.is_type_case())
      return "The type marshaller should produce a name beginning with "
             "an upper case letter.";
    else if (!cfg.VarMarshaller.is_func_case())
      return "The variable marshaller should produce a name beginning "
             "with a lower case letter or '_'.";
    cfg.Marshaller.afterward = true;
    cfg.ModuleMarshaller.forward_marshaller = &cfg.Marshaller;
    cfg.TypeMarshaller.forward_marshaller = &cfg.Marshaller;
    cfg.ConstMarshaller.forward_marshaller = &cfg.Marshaller;
    cfg.VarMarshaller.forward_marshaller = &cfg.Marshaller;
    return StringRef{};
  }
};
