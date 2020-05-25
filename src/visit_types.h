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

#include <clang/AST/RecursiveASTVisitor.h>

#include "config.h"
#include "module.h"
#include "tag_type.h"
#include "types.h"

namespace ffi {
class ast_visitor : public clang::RecursiveASTVisitor<ast_visitor> {
 public:
  ast_visitor(config& cfg, module_contents& mod, clang::ASTContext& context,
              bool header_group)
      : header_group{header_group}, cfg{cfg}, mod{mod}, context{context} {}

  [[nodiscard]] bool check_decl(const clang::Decl* decl) const;
  [[nodiscard]] bool check_extern_c(const clang::Decl& decl) const;

  bool VisitVarDecl(clang::VarDecl* var);
  bool VisitEnumDecl(clang::EnumDecl* enm);
  bool VisitRecordDecl(clang::RecordDecl* record);
  bool VisitFunctionDecl(clang::FunctionDecl* function);
  bool VisitTypedefNameDecl(clang::TypedefNameDecl* alias);

  [[nodiscard]] std::optional<ctype> match_type(const clang::NamedDecl& decl,
                                                const clang::Type& type) const;
  [[nodiscard]] std::optional<entity> match_var_raw(
      const clang::VarDecl& decl) const;
  [[nodiscard]] std::optional<entity> match_var(
      const clang::VarDecl& decl) const;
  [[nodiscard]] std::optional<entity> match_param(
      const clang::ParmVarDecl& param) const;
  [[nodiscard]] std::optional<entity> match_function(
      const clang::FunctionDecl& decl) const;
  [[nodiscard]] std::optional<tag_decl> match_enum(
      const clang::EnumDecl& decl, llvm::StringRef def_name = {}) const;
  [[nodiscard]] std::optional<tag_decl> match_struct(
      const clang::RecordDecl& decl, llvm::StringRef def_name = {}) const;
  [[nodiscard]] std::optional<tag_decl> match_typedef(
      const clang::TypedefNameDecl& decl) const;

 private:
  bool header_group;
  config& cfg;
  module_contents& mod;
  clang::ASTContext& context;
};
}  // namespace ffi
