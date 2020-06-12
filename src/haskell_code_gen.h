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

  void gen_module(const std::string& name, const module_contents& mod);

  std::string_view gen_name(name_variant v, std::string_view name,
                            std::string_view scope = {});

  std::string gen_type(const ctype& type);

 protected:
  void gen_type(llvm::raw_ostream& os, const ctype& type, bool paren = false);
  void gen_function_type(llvm::raw_ostream& os, const function_type& func);
  void gen_scalar_type(llvm::raw_ostream& os, const scalar_type& scalar);
  void gen_opaque_type(llvm::raw_ostream& os, const opaque_type& opaque);
  void gen_pointer_type(llvm::raw_ostream& os, const pointer_type& pointer);

  static bool requires_paren(int tid);

  static bool is_cchar(const ctype& type);
  static bool is_cstring(const ctype& type);
  static bool is_void(const ctype& type);
  static bool is_function(const ctype& type);

  std::string_view name_resolve(name_variant v, scoped_name_view n) const;

 private:
  config& cfg;
  name_resolver* resolver{nullptr};
};
}  // namespace ffi
