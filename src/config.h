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
#include <unordered_map>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "name_converter.h"

namespace ffi {
struct name_converter_bundle {
  using cvt = name_converter;
  cvt for_all{name_case::preserving, name_variant::preserving, nullptr, true};
  cvt for_module{name_case::camel, name_variant::module_name, &for_all};
  cvt for_type{name_case::camel, name_variant::type_ctor, &for_all};
  cvt for_ctor{name_case::camel, name_variant::data_ctor, &for_all};
  cvt for_var{name_case::camel, name_variant::variable, &for_all};
};

struct scoped_name;

struct scoped_name_view {
  std::string_view scope{};
  std::string_view name{};

  scoped_name materialize() const;
};

struct scoped_name {
  std::string scope{};
  std::string name{};

  operator scoped_name_view() const { return {scope, name}; }
};

inline scoped_name scoped_name_view::materialize() const {
  return {std::string{scope}, std::string{name}};
}

inline bool operator<(scoped_name_view n1, scoped_name_view n2) {
  return n1.scope < n2.scope || (n1.scope == n2.scope && n1.name < n2.name);
}

// According to Haskell 2010 Report, there are six kinds of names in Haskell:
// those for variables and constructors denote values; those for type
// variables, type constructors, and type classes refer to entities related to
// the type system; and module names refer to modules. There are two
// constraints on naming:
// - Names for variables and type variables are identifiers beginning with
// lowercase letters or underscore; the other four kinds of names are
// identifiers beginning with uppercase letters.
// - An identifier must not be used as the name of a type constructor and a
// class in the same scope.
struct name_resolver {
  using name_map = std::map<scoped_name, std::string, std::less<>>;
  name_map type_ctors;
  name_map data_ctors;
  name_map variables;
  // internal, should not be exported to config files
  using rev_name_map =
      std::unordered_multimap<std::string_view, scoped_name_view>;
  rev_name_map rev_type_ctors;
  rev_name_map rev_data_ctors;
  rev_name_map rev_variables;
};

struct config {
  bool allow_custom_fixed_size_int{false};
  bool assume_extern_c{false};
  bool warn_no_c_linkage{true};
  bool warn_no_external_formal_linkage{false};
  bool generate_storable_instances{true};
  name_converter_bundle name_converters;
  name_converter_map file_name_converters{};
  std::string library_name{"Library"};
  std::string root_directory{};
  std::string output_directory{};
  std::vector<std::string> file_names{};
  std::vector<std::string> is_header_group{};
  std::vector<std::string> compiler_options{};
  name_resolver::name_map module_name_mapping{};
  std::map<std::string, name_resolver, std::less<>> explicit_name_mapping{};
  bool inja_set_trim_blocks{false};
  bool inja_set_lstrip_blocks{false};
  std::string custom_template{};
  // internal, should not be exported to config files
  name_resolver::rev_name_map rev_modules;
};

bool validate_config(const config& cfg, spdlog::logger& logger);

int name_clashes(const name_resolver::rev_name_map& m, spdlog::logger& logger,
                 std::string_view kind, std::string_view scope);
}  // namespace ffi

template <>
struct fmt::formatter<ffi::scoped_name_view> : formatter<std::string_view> {
  template <typename FormatContext>
  auto format(ffi::scoped_name_view nm, FormatContext& context) {
    return formatter<std::string_view>::format(
        nm.scope.empty() ? nm.name : fmt::format("{}.{}", nm.scope, nm.name),
        context);
  }
};

template <>
struct fmt::formatter<ffi::scoped_name> : formatter<ffi::scoped_name_view> {};
