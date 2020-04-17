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

#include <optional>

#include <clang/AST/AST.h>

#include "Config.h"
#include "Module.h"
#include "Tag.h"
#include "Types.h"

namespace ffi {
class Visitor {
 public:
  Visitor(config::Config& cfg, clang::ASTContext& context, bool header_group)
      : header_group{header_group}, cfg{cfg}, context{context} {}

  bool checkDecl(const clang::Decl* decl) const;
  bool checkExternC(const clang::Decl& decl) const;

  void matchTranslationUnit(const clang::TranslationUnitDecl& decl,
                            ModuleContents& mod) const;
  std::optional<Entity> matchEntity(const clang::Decl& decl) const;
  std::optional<TagDecl> matchTag(const clang::Decl& decl) const;
  std::optional<Type> matchType(const clang::NamedDecl& decl,
                                const clang::Type& type) const;
  std::optional<Entity> matchVarRaw(const clang::VarDecl& decl) const;
  std::optional<Entity> matchVar(const clang::VarDecl& decl) const;
  std::optional<Entity> matchParam(const clang::ParmVarDecl& param) const;
  std::optional<Entity> matchFunction(const clang::FunctionDecl& decl) const;
  std::optional<TagDecl> matchEnum(const clang::EnumDecl& decl,
                                   llvm::StringRef defName = {}) const;
  std::optional<TagDecl> matchStruct(const clang::RecordDecl& decl,
                                     llvm::StringRef defName = {}) const;
  std::optional<TagDecl> matchTypedef(const clang::TypedefNameDecl& decl) const;

 private:
  bool header_group;
  config::Config& cfg;
  clang::ASTContext& context;
};
}  // namespace ffi
