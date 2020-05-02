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

#include "haskell_code_gen.h"

#include <fmt/format.h>

void ffi::haskell_code_gen::gen_module(const std::string& name,
                                       const module_contents& mod) noexcept {
  if (auto p = cfg.file_name_converters.find(name);
      p != cfg.file_name_converters.cend())
    cfg.converters.for_all.forward_converter = &p->second;
  else
    cfg.converters.for_all.forward_converter = nullptr;

  auto mname = cfg.converters.for_module.convert(name);
  auto parent_dir = format(FMT_STRING("{}/{}/LowLevel"), cfg.output_directory,
                           cfg.library_name);
  auto mod_file = format(FMT_STRING("{}/{}.hs"), parent_dir, mname);
  llvm::sys::fs::create_directories(parent_dir);
  std::error_code ec;
  llvm::raw_fd_ostream ofs{mod_file, ec};
  if (ec) {
    llvm::errs() << format(FMT_STRING("Cannot open file '{}'.\n"), mod_file);
    return;
  }
  os = &ofs;

  *os << "{-# LANGUAGE EmptyDataDecls #-}\n"
         "{-# LANGUAGE ForeignFunctionInterface #-}\n"
         "{-# LANGUAGE PatternSynonyms #-}\n";
  if (cfg.allow_rank_n_types) *os << "{-# LANGUAGE RankNTypes #-}\n";
  *os << "{-# OPTIONS_GHC -Wno-missing-pattern-synonym-signatures #-}\n"
         "{-# OPTIONS_GHC -Wno-unused-imports #-}\n";
  *os << "module ";
  gen_module_prefix();
  gen_module_name(name);
  *os << " where\n\n";

  *os << "import Foreign.C.Types\n"
         "import Foreign.C.String\n"
         "import Foreign.Storable\n"
         "import Foreign.Ptr\n"
         "import Foreign.Marshal.Alloc\n";
  *os << '\n';
  for (auto& imp : mod.imports) *os << "import " << imp << '\n';
  if (!mod.imports.empty()) *os << '\n';
  for (auto& [name, tag] : mod.tags) gen_tag(name, tag);
  for (const auto& entity : mod.entities) gen_entity(entity);
}

void ffi::haskell_code_gen::gen_module_prefix() noexcept {
  *os << cfg.library_name << ".LowLevel.";
}

void ffi::haskell_code_gen::gen_module_name(const std::string& name) noexcept {
  *os << cfg.converters.for_module.convert(name);
}

void ffi::haskell_code_gen::gen_entity_raw(const std::string& name,
                                           const ctype& type,
                                           bool use_forall) noexcept {
  gen_func_name(name);
  *os << " :: ";
  clear_fresh_variable();
  if (use_forall)
    explicit_for_all()->gen_type(type);
  else
    gen_type(type);
  *os << '\n';
}

void ffi::haskell_code_gen::gen_entity(const_entity& entity) noexcept {
  *os << "foreign import ccall \"" << entity.first << "\" ";
  gen_entity_raw(entity.first, entity.second);
}

void ffi::haskell_code_gen::gen_func_name(const std::string& name) noexcept {
  *os << cfg.converters.for_var.convert(name);
}

void ffi::haskell_code_gen::gen_type_name(const std::string& name) noexcept {
  *os << cfg.converters.for_type.convert(name);
}

void ffi::haskell_code_gen::gen_const_name(const std::string& name) noexcept {
  *os << cfg.converters.for_ctor.convert(name);
}

void ffi::haskell_code_gen::gen_type(const ctype& type, bool paren) noexcept {
  const auto idx = type.value.index();
  paren = paren && requires_paren(idx) && !is_cstring(type);
  if (paren) *os << "(";
  switch (idx) {
#define TYPE(TypeName)                              \
  case index<ctype::variant, TypeName>:             \
    gen_##TypeName(std::get<TypeName>(type.value)); \
    break;
#include "types.def"
  }
  if (paren) *os << ")";
}

void ffi::haskell_code_gen::gen_function_type(
    const function_type& func) noexcept {
  for (const auto& tp : func.params) {
    gen_type(tp.second);
    *os << " -> ";
  }
  *os << "IO ";
  gen_type(*func.return_type, true);
}

void ffi::haskell_code_gen::gen_scalar_type(
    const scalar_type& scalar) noexcept {
  *os << scalar.as_haskell();
}

void ffi::haskell_code_gen::gen_opaque_type(
    const opaque_type& opaque) noexcept {
  gen_type_name(opaque.name);
}

void ffi::haskell_code_gen::gen_pointer_type(
    const pointer_type& pointer) noexcept {
  auto& pointee = *pointer.pointee;
  if (is_cchar(pointee)) {
    *os << "CString";
    return;
  }

  if (is_function(pointee))
    *os << "FunPtr ";
  else
    *os << "Ptr ";
  if (is_void(pointee) && cfg.void_ptr_as_any_ptr)
    *os << next_fresh_variable();
  else
    gen_type(pointee, true);
}

void ffi::haskell_code_gen::gen_tag(const std::string& name,
                                    const tag_type& tag) noexcept {
  if (std::holds_alternative<enumeration>(tag.payload))
    gen_enum(name, std::get<enumeration>(tag.payload));
  else if (std::holds_alternative<structure>(tag.payload))
    gen_struct(name, std::get<structure>(tag.payload));
}

void ffi::haskell_code_gen::gen_enum(const std::string& name,
                                     const enumeration& enm) noexcept {
  *os << "newtype ";
  gen_type_name(name);
  *os << " = ";
  gen_const_name(name);
  *os << "{ unwrap";
  gen_type_name(name);
  *os << " :: ";
  gen_type(enm.underlying_type);
  *os << " }";
  *os << '\n';
  for (auto& [item, val] : enm.values) gen_enum_item(name, item, val);
  *os << '\n';
}

void ffi::haskell_code_gen::gen_enum_item(const std::string& name,
                                          const std::string& item,
                                          intmax_t val) noexcept {
  *os << "pattern ";
  gen_const_name(item);
  *os << " = ";
  gen_const_name(name);
  *os << ' ' << val << '\n';
}

void ffi::haskell_code_gen::gen_struct(const std::string& name,
                                       const structure& str) noexcept {
  *os << "data ";
  gen_type_name(name);
  *os << " = ";
  gen_const_name(name);
  if (!str.fields.empty()) {
    *os << "\n  { ";
    auto f = begin(str.fields);
    while (true) {
      gen_entity_raw(f->first, f->second, true);
      if (++f == end(str.fields)) break;
      *os << "  , ";
    }
    *os << "  }";
  }
  *os << "\n\n";
}

void ffi::haskell_code_gen::clear_fresh_variable() noexcept {
  fresh_variable.clear();
}

std::string ffi::haskell_code_gen::next_fresh_variable() noexcept {
  if (!fresh_variable.empty() && fresh_variable.back() != 'z')
    ++fresh_variable.back();
  else
    fresh_variable.push_back('a');
  return fresh_variable;
}

bool ffi::haskell_code_gen::requires_paren(int tid) noexcept {
  return tid == index<ctype::variant, function_type> ||
         tid == index<ctype::variant, pointer_type>;
}

bool ffi::haskell_code_gen::is_cchar(const ctype& type) noexcept {
  if (std::holds_alternative<scalar_type>(type.value)) {
    auto& scalar = std::get<scalar_type>(type.value);
    return scalar.sign == scalar_type::Unspecified &&
           scalar.qualifier == scalar_type::Char &&
           scalar.width == scalar_type::WidthNone;
  }
  return false;
}

bool ffi::haskell_code_gen::is_cstring(const ctype& type) noexcept {
  if (std::holds_alternative<pointer_type>(type.value)) {
    auto& pointer = std::get<pointer_type>(type.value);
    return is_cchar(*pointer.pointee);
  }
  return false;
}

bool ffi::haskell_code_gen::is_void(const ctype& type) noexcept {
  if (std::holds_alternative<scalar_type>(type.value)) {
    auto& scalar = std::get<scalar_type>(type.value);
    return scalar.qualifier == scalar_type::Void;
  }
  return false;
}

bool ffi::haskell_code_gen::is_function(const ctype& type) noexcept {
  return std::holds_alternative<function_type>(type.value);
}

auto ffi::haskell_code_gen::explicit_for_all() -> explicit_for_all_handler {
  return explicit_for_all_handler{*this};
}

ffi::haskell_code_gen::explicit_for_all_handler::explicit_for_all_handler(
    haskell_code_gen& g)
    : code_gen{g},
      os{buffer},
      backup_os{g.os},
      backup_void_ptr_as_any_ptr{g.cfg.void_ptr_as_any_ptr} {
  if (g.cfg.void_ptr_as_any_ptr)
    g.cfg.void_ptr_as_any_ptr = g.cfg.allow_rank_n_types;
  g.os = &os;
}

ffi::haskell_code_gen::explicit_for_all_handler::~explicit_for_all_handler() {
  if (!code_gen.fresh_variable.empty()) {
    *backup_os << "forall ";
    const auto last_fresh_var = std::move(code_gen.fresh_variable);
    code_gen.clear_fresh_variable();
    while (true) {
      auto x = code_gen.next_fresh_variable();
      *backup_os << x << ' ';
      if (x == last_fresh_var) break;
    }
    *backup_os << ". ";
  }
  *backup_os << os.str();
  code_gen.os = backup_os;
  code_gen.cfg.void_ptr_as_any_ptr = backup_void_ptr_as_any_ptr;
}

auto ffi::haskell_code_gen::explicit_for_all_handler::operator->() const
    -> haskell_code_gen* {
  return &code_gen;
}
