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

#include "driver.h"

#include <fmt/format.h>

#include "visit_types.h"

clang::FrontendAction* ffi::ffi_driver::create() {
  return new info_collect_action{cfg, modules};
}

ffi::info_collect_action::info_collect_action(config& cfg, module_list& modules)
    : cfg{cfg}, modules{modules} {}

std::unique_ptr<clang::ASTConsumer> ffi::info_collect_action::CreateASTConsumer(
    clang::CompilerInstance& compiler, llvm::StringRef in_file) {
  llvm::SmallString<0> path{in_file.begin(), in_file.end()};
  llvm::sys::path::replace_path_prefix(path, cfg.root_directory, "");
  auto file = llvm::sys::path::relative_path(path.str()).str();
  auto [p, inserted] = modules.try_emplace(file.data());
  return std::make_unique<info_collector>(cfg, p->first, p->second);
}

ffi::info_collector::info_collector(config& cfg, std::string_view file_name,
                                    module_contents& current_module)
    : cfg{cfg}, file_name{file_name}, current_module{current_module} {}

void ffi::info_collector::HandleTranslationUnit(clang::ASTContext& context) {
  const auto is_hg = cfg.is_header_group.cend() !=
                     std::find(cfg.is_header_group.cbegin(),
                               cfg.is_header_group.cend(), file_name);
  ast_visitor visitor{cfg, current_module, context, is_hg};
  visitor.TraverseDecl(context.getTranslationUnitDecl());
}
