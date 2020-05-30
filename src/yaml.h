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

#include "config.h"
#include "function_type.h"
#include "module.h"
#include "name_converter.h"
#include "opaque_type.h"
#include "pointer_type.h"
#include "prim_types.h"
#include "tag_type.h"
#include "types.h"

template <typename T>
struct llvm::yaml::MappingTraits<std::unique_ptr<T>> {
  static void mapping(IO& io, std::unique_ptr<T>& p) {
    if (!io.outputting()) p.reset(new T);
    MappingTraits<T>::mapping(io, *p);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::function_type> {
  static void mapping(IO& io, ffi::function_type& func) {
    io.mapRequired("return_type", func.return_type);
    io.mapRequired("params", func.params);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::opaque_type> {
  static void mapping(IO& io, ffi::opaque_type& type) {
    io.mapRequired("alias", type.name);
    io.mapRequired("marshallable", type.marshallable);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::pointer_type> {
  static void mapping(IO& io, ffi::pointer_type& type) {
    io.mapRequired("pointee", type.pointee);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::ctype> {
  template <typename T>
  static constexpr ptrdiff_t Index = ffi::index<ffi::ctype::variant, T>;

  static void mapping(IO& io, ffi::ctype& type) {
    const auto idx = type.value.index();

    if (false) static_cast<void>(0);
#define TYPE(TypeName)                                          \
  else if (io.mapTag(#TypeName, idx == Index<ffi::TypeName>)) { \
    if (!io.outputting()) type.value.emplace<ffi::TypeName>();  \
    auto& clause = std::get<ffi::TypeName>(type.value);         \
    MappingTraits<ffi::TypeName>::mapping(io, clause);          \
  }
#include "types.def"
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::structure> {
  static void mapping(IO& io, ffi::structure& tag) {
    io.mapRequired("fields", tag.fields);
  }
};

template <>
struct llvm::yaml::CustomMappingTraits<ffi::enumeration> {
  static void inputOne(IO& io, StringRef key, ffi::enumeration& enm) {
    auto kstr = key.str();
    int value = 0;
    io.mapRequired(kstr.c_str(), value);
    enm.values.try_emplace(kstr, value);
  }

  static void output(IO& io, ffi::enumeration& enm) {
    for (auto& [k, v] : enm.values) io.mapRequired(k.c_str(), v);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::tag_type> {
  using vtype = decltype(std::declval<ffi::tag_type>().payload);
  template <typename T>
  static constexpr ptrdiff_t Index = ffi::index<vtype, T>;

  static void mapping(IO& io, ffi::tag_type& tag) {
    const auto idx = tag.payload.index();

    if (io.mapTag("structure", idx == Index<ffi::structure>)) {
      if (!io.outputting()) tag.payload.emplace<ffi::structure>();
      auto& clause = std::get<ffi::structure>(tag.payload);
      MappingTraits<ffi::structure>::mapping(io, clause);
    } else if (io.mapTag("enumeration", idx == Index<ffi::enumeration>)) {
      if (!io.outputting()) tag.payload.emplace<ffi::enumeration>();
      auto& clause = std::get<ffi::enumeration>(tag.payload);
      io.mapRequired("enumerators", clause);
    }
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::entity> {
  static void mapping(IO& io, ffi::entity& entity) {
    io.mapRequired("name", entity.first);
    io.mapRequired("type", entity.second);
  }
};
LLVM_YAML_IS_SEQUENCE_VECTOR(ffi::entity)

template <>
struct llvm::yaml::MappingTraits<ffi::module_contents> {
  static void mapping(IO& io, ffi::module_contents& mod) {
    io.mapRequired("entities", mod.entities);
    io.mapRequired("tags", mod.tags);
    io.mapRequired("imports", mod.imports);
  }
};

template <typename T, typename C>
struct llvm::yaml::CustomMappingTraits<std::map<std::string, T, C>> {
  static void inputOne(IO& io, StringRef key, std::map<std::string, T, C>& m) {
    auto kstr = key.str();
    io.mapRequired(kstr.c_str(), m[kstr]);
  }

  static void output(IO& io, std::map<std::string, T, C>& m) {
    for (auto& [k, v] : m) io.mapRequired(k.c_str(), v);
  }
};

template <typename T, typename C>
struct llvm::yaml::CustomMappingTraits<std::map<ffi::scoped_name, T, C>> {
  static void inputOne(IO& io, StringRef key,
                       std::map<ffi::scoped_name, T, C>& m) {
    const auto p = key.find('.');
    if (p == StringRef::npos) {
      // no scope provided
      ffi::scoped_name nm{{}, key.str()};
      io.mapRequired(nm.name.c_str(), m[nm]);
    } else if (key.size() <= p + 1 || key.find('.', p + 1) != StringRef::npos) {
      io.setError(fmt::format("invalid scoped name '{}'", key.str()));
    } else {
      ffi::scoped_name nm{key.substr(0, p).str(), key.substr(p + 1).str()};
      io.mapRequired(key.str().c_str(), m[nm]);
    }
  }

  static void output(IO& io, std::map<ffi::scoped_name, T, C>& m) {
    for (auto& [k, v] : m) io.mapRequired(fmt::to_string(k).c_str(), v);
  }
};

template <>
struct llvm::yaml::ScalarEnumerationTraits<ffi::name_case> {
  static void enumeration(IO& io, ffi::name_case& c) {
    io.enumCase(c, "preserving", ffi::name_case::preserving);
    io.enumCase(c, "CamelCase", ffi::name_case::camel);
    io.enumCase(c, "Snake_Case_Init_Upper", ffi::name_case::snake_init_upper);
    io.enumCase(c, "snake_case_all_lower", ffi::name_case::snake_all_lower);
    io.enumCase(c, "SNAKE_CASE_ALL_UPPER", ffi::name_case::snake_all_upper);
  }
};

template <>
struct llvm::yaml::ScalarEnumerationTraits<ffi::name_variant> {
  static void enumeration(IO& io, ffi::name_variant& c) {
    io.enumCase(c, "preserving", ffi::name_variant::preserving);
    io.enumCase(c, "module_name", ffi::name_variant::module_name);
    io.enumCase(c, "type_ctor", ffi::name_variant::type_ctor);
    io.enumCase(c, "data_ctor", ffi::name_variant::data_ctor);
    io.enumCase(c, "variable", ffi::name_variant::variable);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::name_converter> {
  static void mapping(IO& io, ffi::name_converter& m) {
    io.mapOptional("remove_prefix", m.remove_prefix);
    io.mapOptional("remove_suffix", m.remove_suffix);
    io.mapOptional("add_prefix", m.add_prefix);
    io.mapOptional("add_suffix", m.add_suffix);
    io.mapOptional("output_case", m.output_case);
    io.mapOptional("output_variant", m.output_variant);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::name_converter_bundle> {
  static void mapping(IO& io, ffi::name_converter_bundle& bundle) {
    io.mapOptional("all", bundle.for_all);
    io.mapOptional("module", bundle.for_module);
    io.mapOptional("type", bundle.for_type);
    io.mapOptional("ctor", bundle.for_ctor);
    io.mapOptional("var", bundle.for_var);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::name_resolver> {
  static void mapping(IO& io, ffi::name_resolver& resolver) {
    io.mapOptional("type_ctors", resolver.type_ctors);
    io.mapOptional("data_ctors", resolver.data_ctors);
    io.mapOptional("variables", resolver.variables);
  }
};

template <>
struct llvm::yaml::MappingTraits<ffi::config> {
  static void mapping(IO& io, ffi::config& cfg) {
    io.mapOptional("allow_custom_fixed_size_int",
                   cfg.allow_custom_fixed_size_int);
    io.mapOptional("assume_extern_c", cfg.assume_extern_c);
    io.mapOptional("warn_no_c_linkage", cfg.warn_no_c_linkage);
    io.mapOptional("warn_no_external_formal_linkage",
                   cfg.warn_no_external_formal_linkage);
    io.mapOptional("void_ptr_as_any_ptr", cfg.void_ptr_as_any_ptr);
    io.mapOptional("allow_rank_n_types", cfg.allow_rank_n_types);
    io.mapOptional("generate_storable_instances",
                   cfg.generate_storable_instances);
    io.mapOptional("name_converters", cfg.converters);
    io.mapOptional("file_name_converters", cfg.file_name_converters);
    io.mapOptional("library_name", cfg.library_name);
    io.mapOptional("root_directory", cfg.root_directory);
    io.mapOptional("output_directory", cfg.output_directory);
    io.mapRequired("file_names", cfg.file_names);
    io.mapOptional("is_header_group", cfg.is_header_group);
    io.mapOptional("compiler_options", cfg.compiler_options);
    io.mapOptional("module_name_mapping", cfg.module_name_mapping);
    io.mapOptional("explicit_name_mapping", cfg.explicit_name_mapping);
  }
};
