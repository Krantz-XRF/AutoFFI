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

#include <array>
#include <iostream>

#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>

#include <fmt/format.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "config.h"
#include "driver.h"
#include "haskell_code_gen.h"
#include "json.h"
#include "templates.h"
#include "yaml.h"

namespace cl = llvm::cl;

namespace {
cl::OptionCategory category{"auto-FFI Options"};
cl::opt<bool> help{"h", cl::desc{"Alias for -help"}, cl::Hidden};
cl::opt<bool> dump_config{
    "dump-config", cl::cat{category},
    cl::desc{"Dump configuration options to stdout and exit"}};
cl::opt<bool> dump_template{
    "dump-template", cl::cat{category},
    cl::desc{"Dump output template to stdout and exit"}};
cl::opt<bool> yaml{"yaml", cl::cat{category},
                   cl::desc{"Dump YAML for generated modules"}};
cl::opt<bool> json{"json", cl::cat{category},
                   cl::desc{"Dump JSON for generated modules"}};
cl::opt<std::string> verbose{
    "verbose", cl::cat{category}, cl::init("info"), cl::value_desc{"level"},
    cl::desc{"Verbosity: [trace, debug, info, warning, error, critical, off]"}};
cl::list<std::string> config_files{cl::Positional, cl::cat{category},
                                   cl::desc{"[<file> ...]"}};

std::optional<spdlog::level::level_enum> parse_verbosity(std::string_view s) {
  static constexpr std::array levels = SPDLOG_LEVEL_NAMES;
  const auto p = std::find(begin(levels), end(levels), s);
  if (p == end(levels)) return std::nullopt;
  return static_cast<spdlog::level::level_enum>(p - begin(levels));
}

const char version[]{
    "auto-FFI 2020\n"
    "Copyright (C) 2020 Xie Ruifeng.\n"
    "This is free software; see the source for copying conditions.  There is "
    "ABSOLUTELY NO WARRANTY."};
}  // namespace

int main(int argc, const char* argv[]) {
  // LLVM initialization
  llvm::InitLLVM init_llvm(argc, argv);
  HideUnrelatedOptions(category);

  // auto-FFI options
  cl::SetVersionPrinter([](llvm::raw_ostream& os) { os << version << '\n'; });
  cl::ParseCommandLineOptions(
      argc, argv,
      "A tool to generate Haskell FFI binding for C/C++ API.\n\n"
      "It reads the configuration from the <file>s and writes the result to "
      "the specified output files. For bug reports, please see:\n"
      "<https://github.com/Krantz-XRF/auto-FFI>.\n");

  // Help message
  if (help) {
    cl::PrintHelpMessage();
    return 0;
  }

  set_default_logger(spdlog::stderr_color_st("auto-ffi"));
  spdlog::set_pattern("%n: %^%l:%$ %v");
  if (const auto v = parse_verbosity(verbose)) spdlog::set_level(*v);

  ffi::ffi_driver driver;

  // Configurations
  if (dump_config) {
    llvm::yaml::Output output{llvm::outs()};
    output << driver.cfg;
    return 0;
  }

  // Template
  if (dump_template) {
    llvm::outs() << ffi::default_template_hs;
    return 0;
  }

  // No input, print help and exit
  if (config_files.empty()) {
    cl::PrintHelpMessage();
    return 0;
  }

  // error counter
  int total_errors{0};

  // Run on config files
  for (const auto& cfg_file : config_files) {
    auto contents = llvm::MemoryBuffer::getFile(cfg_file);
    if (auto ec = contents.getError()) {
      llvm::errs() << format(
          FMT_STRING("Failed to load configuration file \"{}\": {}\n"),
          cfg_file, ec.message());
      continue;
    }
    llvm::yaml::Input input{contents.get()->getBuffer()};
    input >> driver.cfg;
    if (input.error()) continue;

    auto logger = spdlog::stderr_color_st(cfg_file);

    // Diagnostics engine for configuration files
    if (!validate_config(driver.cfg, *logger)) {
      ++total_errors;
      continue;
    }

    // Load CWD
    llvm::SmallString<128> current_path;
    llvm::sys::fs::current_path(current_path);
    if (!driver.cfg.root_directory.empty())
      llvm::sys::fs::set_current_path(driver.cfg.root_directory);

    // Compiler options
    clang::tooling::FixedCompilationDatabase compilations{
        driver.cfg.root_directory.empty() ? "." : driver.cfg.root_directory,
        driver.cfg.compiler_options};

    clang::tooling::ClangTool tool{compilations, driver.cfg.file_names};

    if (const auto status = tool.run(&driver)) {
      logger->debug("Clang front-end fails with status {}.", status);
      ++total_errors;
      continue;
    }

    if (yaml) {
      llvm::yaml::Output output{llvm::outs()};
      output << driver.modules;
    }

    if (json) {
      nlohmann::json j = driver.modules;
      llvm::outs() << j.dump(2) << '\n';
    }

    ffi::haskell_code_gen code_gen{driver.cfg};
    for (auto& [name, mod] : driver.modules) code_gen.gen_module(name, mod);

    int nc{0};
    nc += ffi::name_clashes(driver.cfg.rev_modules, *logger, "module",
                            "(global)");
    for (auto [mod, m] : driver.cfg.explicit_name_mapping) {
      nc += ffi::name_clashes(m.rev_variables, *logger, "variable", mod);
      nc += ffi::name_clashes(m.rev_data_ctors, *logger, "data ctor", mod);
      nc += ffi::name_clashes(m.rev_type_ctors, *logger, "type ctor", mod);
    }
    logger->debug("Total name clash: {}.", nc);
    if (nc) ++total_errors;

    // Recover CWD
    llvm::sys::fs::set_current_path(current_path);
  }

  spdlog::debug("Total errors: {}\n", total_errors);
  return total_errors;
}
