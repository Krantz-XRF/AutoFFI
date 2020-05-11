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

#include <llvm/Support/raw_ostream.h>

#include "config.h"
#include "module.h"

namespace ffi {
class haskell_code_gen {
 public:
  explicit haskell_code_gen(config& cfg) : cfg{cfg} {}

  void gen_module(const std::string& name, const module_contents& mod) noexcept;
  void gen_module_prefix() noexcept;

  void gen_name(name_variant v, const std::string& name,
                std::string_view scope = {}) noexcept;

  void gen_entity_raw(const std::string& name, const ctype& type,
                      bool use_forall = false,
                      std::string_view scope = {}) noexcept;
  void gen_entity(const_entity& entity, std::string_view scope = {}) noexcept;
  void gen_type(const ctype& type, bool paren = false) noexcept;
  void gen_function_type(const function_type& func) noexcept;
  void gen_scalar_type(const scalar_type& scalar) noexcept;
  void gen_opaque_type(const opaque_type& opaque) noexcept;
  void gen_pointer_type(const pointer_type& pointer) noexcept;

  void gen_tag(const std::string& name, const tag_type& tag) noexcept;
  void gen_enum(const std::string& name, const enumeration& enm) noexcept;
  void gen_enum_item(const std::string& name, const std::string& item,
                     intmax_t val) noexcept;
  void gen_struct(const std::string& name, const structure& str) noexcept;

  void clear_fresh_variable() noexcept;
  std::string next_fresh_variable() noexcept;

  static bool requires_paren(int tid) noexcept;

  static bool is_cchar(const ctype& type) noexcept;
  static bool is_cstring(const ctype& type) noexcept;
  static bool is_void(const ctype& type) noexcept;
  static bool is_function(const ctype& type) noexcept;

  std::string_view name_resolve(name_variant v, std::string_view s,
                                std::string_view scope = {}) const noexcept;

 protected:
  class explicit_for_all_handler {
   public:
    explicit explicit_for_all_handler(haskell_code_gen& g);
    ~explicit_for_all_handler();

    haskell_code_gen* operator->() const;

   private:
    haskell_code_gen& code_gen;
    std::string buffer;
    llvm::raw_string_ostream os;
    llvm::raw_ostream* backup_os;
    bool backup_void_ptr_as_any_ptr;
  };

  explicit_for_all_handler explicit_for_all();

 private:
  config& cfg;
  name_resolver* resolver{nullptr};
  llvm::raw_ostream* os{nullptr};
  std::string fresh_variable;
};
}  // namespace ffi
