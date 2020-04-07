#pragma once

#include <memory>

#include <clang/AST/ASTConsumer.h>
#include <clang/Tooling/Tooling.h>

#include "Config.h"
#include "Module.h"

namespace ffi {
using ModuleList = std::map<std::string, ffi::ModuleContents>;

struct FFIDriver : clang::tooling::FrontendActionFactory {
  clang::FrontendAction* create() override;
  config::Config cfg;
  ModuleList modules;
};

class InfoCollectAction : public clang::ASTFrontendAction {
 public:
  InfoCollectAction(config::Config& cfg, ModuleList& modules);
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& Compiler, llvm::StringRef InFile) override;

 private:
  config::Config& cfg;
  ModuleList& modules;
};

class InfoCollector : public clang::ASTConsumer {
 public:
  InfoCollector(config::Config& cfg, std::string_view fileName,
                ModuleContents& currentModule);
  void HandleTranslationUnit(clang::ASTContext& Context) override;

 private:
  config::Config& cfg;
  std::string_view fileName;
  ModuleContents& currentModule;
};
}  // namespace ffi
