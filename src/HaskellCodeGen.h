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

#include "Config.h"
#include "Module.h"

namespace ffi {
class HaskellCodeGen {
 public:
  HaskellCodeGen(config::Config& cfg) : cfg{cfg} {}

  void genModule(const std::string& name, const ModuleContents& mod) noexcept;
  void genModulePrefix() noexcept;
  void genModuleName(const std::string& name) noexcept;

  void genFuncName(const std::string& name) noexcept;
  void genTypeName(const std::string& name) noexcept;
  void genConstName(const std::string& name) noexcept;

  void genEntityRaw(const std::string& name, const Type& type) noexcept;
  void genEntity(ConstEntity& entity) noexcept;
  void genType(const Type& type, bool paren = false) noexcept;
  void genFunctionType(const FunctionType& func) noexcept;
  void genScalarType(const ScalarType& scalar) noexcept;
  void genOpaqueType(const OpaqueType& opaque) noexcept;
  void genPointerType(const PointerType& pointer) noexcept;

  void genTag(const std::string& name, const Tag& tag) noexcept;
  void genEnum(const std::string& name, const Enumeration& enm) noexcept;
  void genEnumItem(const std::string& name, const std::string& item,
                   intmax_t val) noexcept;
  void genStruct(const std::string& name, const Structure& str) noexcept;

  void clear_fresh_variable() noexcept;
  std::string next_fresh_variable() noexcept;

  static bool requiresParen(int tid) noexcept;

  static bool isCChar(const Type& type) noexcept;
  static bool isCString(const Type& type) noexcept;
  static bool isVoid(const Type& type) noexcept;
  static bool isFunction(const Type& type) noexcept;

 private:
  config::Config& cfg;
  llvm::raw_ostream* os;
  std::string fresh_variable;
};
}  // namespace ffi
