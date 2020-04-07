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
