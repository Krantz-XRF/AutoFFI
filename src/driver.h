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

#include <memory>

#include <clang/AST/ASTConsumer.h>
#include <clang/Tooling/Tooling.h>

#include "config.h"
#include "module.h"

namespace ffi {
using module_list = std::map<std::string, ffi::module_contents>;

struct ffi_driver final : clang::tooling::FrontendActionFactory {
  clang::FrontendAction* create() override;
  ffi::config cfg;
  module_list modules;
};

class info_collect_action final : public clang::ASTFrontendAction {
 public:
  info_collect_action(ffi::config& cfg, module_list& modules);
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& compiler, llvm::StringRef in_file) override;

 private:
  ffi::config& cfg;
  module_list& modules;
};

class info_collector final : public clang::ASTConsumer {
 public:
  info_collector(ffi::config& cfg, std::string_view file_name,
                 module_contents& current_module);
  void HandleTranslationUnit(clang::ASTContext& context) override;

 private:
  ffi::config& cfg;
  std::string_view file_name;
  module_contents& current_module;
};
}  // namespace ffi
