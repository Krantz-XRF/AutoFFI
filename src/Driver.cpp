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

#include "Driver.h"

#include <fmt/format.h>

#include "VisitTypes.h"

clang::FrontendAction* ffi::FFIDriver::create() {
  return new InfoCollectAction{cfg, modules};
}

ffi::InfoCollectAction::InfoCollectAction(config::Config& cfg,
                                          ModuleList& modules)
    : cfg{cfg}, modules{modules} {}

std::unique_ptr<clang::ASTConsumer> ffi::InfoCollectAction::CreateASTConsumer(
    clang::CompilerInstance& Compiler, llvm::StringRef InFile) {
  llvm::SmallString<0> path{InFile.begin(), InFile.end()};
  llvm::sys::path::replace_path_prefix(path, cfg.RootDirectory, "");
  auto file = llvm::sys::path::relative_path(path.str()).str();
  auto [p, inserted] = modules.try_emplace(file.data());
  return std::make_unique<InfoCollector>(cfg, p->first, p->second);
}

ffi::InfoCollector::InfoCollector(config::Config& cfg,
                                  std::string_view fileName,
                                  ModuleContents& currentModule)
    : cfg{cfg}, fileName{fileName}, currentModule{currentModule} {}

void ffi::InfoCollector::HandleTranslationUnit(clang::ASTContext& Context) {
  const auto pdecl = Context.getTranslationUnitDecl();
  const bool is_hg =
      cfg.IsHeaderGroup.cend() !=
      std::find(cfg.IsHeaderGroup.cbegin(), cfg.IsHeaderGroup.cend(), fileName);
  Visitor{cfg, Context, is_hg}.matchTranslationUnit(*pdecl, currentModule);
}
