#include <array>
#include <iostream>
#include <map>

#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>

#include <lava/assert.h>

#include <fmt/format.h>

#include "Config.h"
#include "Driver.h"
#include "HaskellCodeGen.h"
#include "YAML.h"

namespace tool = clang::tooling;
namespace cl = llvm::cl;

namespace {
cl::OptionCategory category{"auto-FFI Options"};
cl::opt<bool> Help{"h", cl::desc{"Alias for -help"}, cl::Hidden};
cl::opt<bool> DumpConfig{
    "dump-config", cl::desc{"Dump configuration options to stdout and exit."},
    cl::cat{category}};
cl::opt<bool> Yaml{"yaml", cl::desc{"Dump YAML for entities."},
                   cl::cat{category}};
cl::opt<bool> Verbose{"verbose", cl::desc{"Print verbose output message."},
                      cl::cat{category}};
cl::list<std::string> ConfigFiles{cl::Positional, cl::desc{"[<file> ...]"},
                                  cl::cat{category}};
const char version[]{"auto-FFI 2020"};
}  // namespace

int main(int argc, const char* argv[]) {
  // LLVM initialization
  llvm::InitLLVM X(argc, argv);
  cl::HideUnrelatedOptions(category);

  // auto-FFI options
  cl::SetVersionPrinter([](llvm::raw_ostream& os) { os << version << '\n'; });
  cl::ParseCommandLineOptions(
      argc, argv,
      "A tool to generate Haskell FFI binding for C/C++ API.\n\n"
      "It reads the configuration from the <file>s and writes the result to "
      "the specified output files.\n");

  // Help message
  if (Help) {
    cl::PrintHelpMessage();
    return 0;
  }

  ffi::FFIDriver driver;

  // Configurations
  if (DumpConfig) {
    llvm::yaml::Output output{llvm::outs()};
    output << driver.cfg;
    return 0;
  }

  // No input, print help and exit
  if (ConfigFiles.empty()) {
    cl::PrintHelpMessage();
    return 0;
  }

  // Run on config files
  for (const auto& cfgFile : ConfigFiles) {
    auto contents = llvm::MemoryBuffer::getFile(cfgFile);
    if (auto ec = contents.getError()) {
      llvm::errs() << format(
          FMT_STRING("Failed to load configuration file \"{}\": {}\n"), cfgFile,
          ec.message());
      continue;
    }
    llvm::yaml::Input input{contents.get()->getBuffer()};
    input >> driver.cfg;
    if (input.error()) continue;

    // Load CWD
    llvm::SmallString<128> currentPath;
    llvm::sys::fs::current_path(currentPath);
    if (!driver.cfg.RootDirectory.empty())
      llvm::sys::fs::set_current_path(driver.cfg.RootDirectory);

    // Compiler options
    tool::FixedCompilationDatabase compilations{
        driver.cfg.RootDirectory.empty() ? "." : driver.cfg.RootDirectory,
        driver.cfg.CompilerOptions};

    tool::ClangTool tool{compilations, driver.cfg.FileNames};

    if (const auto status = tool.run(&driver)) return status;

    if (Yaml) {
      llvm::yaml::Output output{llvm::outs()};
      output << driver.modules;
    }

    ffi::HaskellCodeGen codeGen{driver.cfg};
    for (auto& [name, mod] : driver.modules) codeGen.genModule(name, mod);

    // Recover CWD
    llvm::sys::fs::set_current_path(currentPath);
  }

  return 0;
}
