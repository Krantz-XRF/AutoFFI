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
#include <gsl/gsl_assert>

namespace {
constexpr size_t npos = std::numeric_limits<size_t>::max();
}  // namespace

void ffi::haskell_code_gen::gen_module(const std::string& name,
                                       const module_contents& mod) {
  if (auto p = cfg.file_name_converters.find(name);
      p != cfg.file_name_converters.cend())
    cfg.converters.for_all.forward_converter = &p->second;
  else
    cfg.converters.for_all.forward_converter = nullptr;
  resolver = &cfg.explicit_name_mapping[name];

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
         "{-# LANGUAGE PatternSynonyms #-}\n"
         "{-# LANGUAGE GeneralizedNewtypeDeriving #-}\n"
         "{-# LANGUAGE DerivingStrategies #-}\n";
  if (cfg.allow_rank_n_types) *os << "{-# LANGUAGE RankNTypes #-}\n";
  *os << "{-# OPTIONS_GHC -Wno-missing-pattern-synonym-signatures #-}\n"
         "{-# OPTIONS_GHC -Wno-unused-imports #-}\n";
  *os << "module ";
  gen_module_prefix();
  gen_name(name_variant::module_name, name);
  *os << " where\n\n";

  *os << "import Foreign.C.Types\n"
         "import Foreign.C.String\n"
         "import Foreign.Storable\n"
         "import Foreign.Ptr\n"
         "import Foreign.Marshal.Alloc\n";
  *os << '\n';
  for (const auto& imp : mod.imports) *os << "import " << imp << '\n';
  if (!mod.imports.empty()) *os << '\n';
  for (const auto& [name, tag] : mod.tags) gen_tag(name, tag);
  for (const auto& entity : mod.entities) gen_entity(entity);
}

void ffi::haskell_code_gen::gen_module_prefix() {
  *os << cfg.library_name << ".LowLevel.";
}

void ffi::haskell_code_gen::gen_name(name_variant v, const std::string& name,
                                     std::string_view scope) {
  scoped_name_view sname{scope, name};
  const auto nm = name_resolve(v, sname);
  *os << llvm::StringRef{nm.data(), nm.size()};
}

void ffi::haskell_code_gen::gen_entity_raw(const std::string& name,
                                           const ctype& type, bool use_forall,
                                           std::string_view scope) {
  gen_name(name_variant::variable, name, scope);
  *os << " :: ";
  clear_fresh_variable();
  if (use_forall)
    explicit_for_all()->gen_type(type);
  else
    gen_type(type);
  *os << '\n';
}

void ffi::haskell_code_gen::gen_entity(const_entity& entity,
                                       std::string_view scope) {
  *os << "foreign import ccall \"" << entity.first << "\" ";
  gen_entity_raw(entity.first, entity.second, false, scope);
}

void ffi::haskell_code_gen::gen_type(const ctype& type, bool paren) {
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

void ffi::haskell_code_gen::gen_function_type(const function_type& func) {
  for (const auto& tp : func.params) {
    gen_type(tp.second);
    *os << " -> ";
  }
  *os << "IO ";
  gen_type(*func.return_type, true);
}

void ffi::haskell_code_gen::gen_scalar_type(const scalar_type& scalar) {
  *os << scalar.as_haskell();
}

void ffi::haskell_code_gen::gen_opaque_type(const opaque_type& opaque) {
  gen_name(name_variant::type_ctor, opaque.name);
}

void ffi::haskell_code_gen::gen_pointer_type(const pointer_type& pointer) {
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
                                    const tag_type& tag) {
  if (std::holds_alternative<enumeration>(tag.payload))
    gen_enum(name, std::get<enumeration>(tag.payload));
  else if (std::holds_alternative<structure>(tag.payload))
    gen_struct(name, std::get<structure>(tag.payload));
}

void ffi::haskell_code_gen::gen_enum(const std::string& name,
                                     const enumeration& enm) {
  *os << "newtype ";
  gen_name(name_variant::type_ctor, name);
  *os << " = ";
  gen_name(name_variant::data_ctor, name);
  *os << "{ unwrap";
  gen_name(name_variant::type_ctor, name);
  *os << " :: ";
  gen_type(enm.underlying_type);
  *os << " }\n";
  *os << "  deriving newtype (Storable)\n";
  for (const auto& [item, val] : enm.values) gen_enum_item(name, item, val);
  *os << '\n';
}

void ffi::haskell_code_gen::gen_enum_item(const std::string& name,
                                          const std::string& item,
                                          intmax_t val) {
  *os << "pattern ";
  gen_name(name_variant::data_ctor, item);
  *os << " = ";
  gen_name(name_variant::data_ctor, name);
  *os << ' ' << val << '\n';
}

void ffi::haskell_code_gen::gen_struct(const std::string& name,
                                       const structure& str) {
  // type declaration
  *os << "data ";
  gen_name(name_variant::type_ctor, name);
  *os << " = ";
  gen_name(name_variant::data_ctor, name);
  if (!str.fields.empty()) {
    *os << "\n  { ";
    auto f = begin(str.fields);
    while (true) {
      gen_entity_raw(f->first, f->second, true, name);
      if (++f == end(str.fields)) break;
      *os << "  , ";
    }
    *os << "  }\n";
  }
  // All pointers are of the same size and alignment
  // Here, we temporarily turn off void_pointer_as_any_pointer
  const auto backup_vpaap = cfg.void_ptr_as_any_ptr;
  cfg.void_ptr_as_any_ptr = false;
  // Storable instance
  constexpr auto util_bindings =
      "    let sizeAlign x = (sizeOf x, alignment x)\n"
      "        makeSize = foldl' (\\sz (sx, ax) -> alignTo sz ax + sx) 0\n"
      "        alignTo s a = ((s + a - 1) `quot` a) * a\n"
      "    in ";
  const auto gen_members = [this, &str](llvm::StringRef fun, size_t n = npos) {
    n = std::min(n, str.fields.size());
    *os << '[';
    if (n > 0) {
      size_t i = 0;
      while (true) {
        *os << fun << " (undefined :: ";
        gen_type(str.fields[i].second);
        *os << ")";
        if (++i == n) break;
        *os << ", ";
      }
    }
    *os << ']';
  };
  *os << "instance Storable ";
  gen_name(name_variant::type_ctor, name);
  *os << " where";
  *os << "\n  sizeOf _ =\n" << util_bindings << "makeSize ";
  gen_members("sizeAlign");
  *os << "\n  alignment _ = maximum ";
  gen_members("alignment");
  *os << "\n  peek p =\n" << util_bindings;
  if (str.fields.empty()) {
    *os << "pure ";
    gen_name(name_variant::data_ctor, name);
  } else {
    gen_name(name_variant::data_ctor, name);
    *os << "\n    <$> ";
    size_t i = 0;
    while (true) {
      *os << "peekByteOff p (makeSize ";
      gen_members("sizeAlign", i);
      *os << " `alignTo` alignment (undefined :: ";
      gen_type(str.fields[i].second);
      *os << "))";
      if (++i == str.fields.size()) break;
      *os << "\n    <*> ";
    }
  }
  *os << "\n  poke p r =\n" << util_bindings;
  if (str.fields.empty())
    *os << "pure ()\n";
  else {
    *os << "do";
    for (size_t i = 0; i < str.fields.size(); ++i) {
      *os << "\n    pokeByteOff p (makeSize ";
      gen_members("sizeAlign", i);
      *os << " `alignTo` alignment (undefined :: ";
      gen_type(str.fields[i].second);
      *os << ")) (";
      gen_name(name_variant::variable, str.fields[i].first, name);
      *os << " r)";
    }
  }
  *os << "\n\n";
  // Recover void_pointer_as_any_pointer
  cfg.void_ptr_as_any_ptr = backup_vpaap;
}

void ffi::haskell_code_gen::clear_fresh_variable() { fresh_variable.clear(); }

std::string ffi::haskell_code_gen::next_fresh_variable() {
  if (!fresh_variable.empty() && fresh_variable.back() != 'z')
    ++fresh_variable.back();
  else
    fresh_variable.push_back('a');
  return fresh_variable;
}

bool ffi::haskell_code_gen::requires_paren(int tid) {
  return tid == index<ctype::variant, function_type> ||
         tid == index<ctype::variant, pointer_type>;
}

bool ffi::haskell_code_gen::is_cchar(const ctype& type) {
  if (std::holds_alternative<scalar_type>(type.value)) {
    const auto& scalar = std::get<scalar_type>(type.value);
    return scalar.sign == scalar_type::Unspecified &&
           scalar.qualifier == scalar_type::Char &&
           scalar.width == scalar_type::WidthNone;
  }
  return false;
}

bool ffi::haskell_code_gen::is_cstring(const ctype& type) {
  if (std::holds_alternative<pointer_type>(type.value)) {
    const auto& pointer = std::get<pointer_type>(type.value);
    return is_cchar(*pointer.pointee);
  }
  return false;
}

bool ffi::haskell_code_gen::is_void(const ctype& type) {
  if (std::holds_alternative<scalar_type>(type.value)) {
    const auto& scalar = std::get<scalar_type>(type.value);
    return scalar.qualifier == scalar_type::Void;
  }
  return false;
}

bool ffi::haskell_code_gen::is_function(const ctype& type) {
  return std::holds_alternative<function_type>(type.value);
}

std::string_view ffi::haskell_code_gen::name_resolve(name_variant v,
                                                     scoped_name_view n) const {
  Expects(v != name_variant::preserving);
  const auto nv = static_cast<size_t>(v) - 1;
  Expects(0 <= nv && nv < 4);
  auto& conv = *std::array{
      &cfg.converters.for_var,
      &cfg.converters.for_module,
      &cfg.converters.for_type,
      &cfg.converters.for_ctor,
  }[nv];
  auto& fwd = *std::array{
      &resolver->variables,
      &cfg.module_name_mapping,
      &resolver->type_ctors,
      &resolver->data_ctors,
  }[nv];
  auto& rev = *std::array{
      &resolver->rev_variables,
      &cfg.rev_modules,
      &resolver->rev_type_ctors,
      &resolver->rev_data_ctors,
  }[nv];
  if (const auto p = fwd.find(n); p != fwd.cend()) return p->second;
  auto [p, _] = fwd.emplace(n.materialize(), conv.convert(n.name));
  rev.emplace(p->second, p->first);
  spdlog::trace("name '{}' get converted to '{}'.", n, p->second);
  return p->second;
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
