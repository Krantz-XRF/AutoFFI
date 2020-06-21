# auto-FFI

[![language](https://img.shields.io/badge/C%2B%2B-17-blue)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![Build Status](https://travis-ci.org/Krantz-XRF/auto-FFI.svg?branch=master)](https://travis-ci.org/Krantz-XRF/auto-FFI)
[![license](https://img.shields.io/badge/license-AGPL--v3.0-blueviolet)](https://www.gnu.org/licenses/agpl-3.0.html)

auto-FFI is licensed under the GNU Affero General Public License v3.0.

## Overview

A tool to generate Haskell FFI binding for C/C++ API.

It reads the configuration from the `<file>`s and writes the result to the specified output files.

## Build

auto-FFI is developed and tested with [LLVM/Clang 9](https://llvm.org/) and [libfmt 6.2](https://github.com/fmtlib/fmt/releases/tag/6.2.0).

LLVM 10 changed signature for `clang::tooling::FrontendActionFactory::create`, so auto-FFI is currently not compatible with LLVM 10. (Transition to LLVM 10 is planned, though not prioritised.) Other versions of LLVM/Clang/libfmt are not tested.

### Build from Source

- Install LLVM, Clang, `libfmt`, `spdlog`, and `Inja`.
  - LLVM/Clang: Use the [pre-built binaries](https://releases.llvm.org/download.html#9.0.1) or compile from source.
    - e.g. In the Travis CI build script, I install from LLVM's official `apt` repo (Ubuntu/Debian only).
    - Note that the pre-built binary packages for Windows do not have C++ headers with them, and thus are not usable.
  - `libfmt`: Follow the instructions on [the official website](https://fmt.dev/latest/usage.html)
    - e.g. In the Travis CI build script, I clone [the GitHub repo](https://github.com/fmtlib/fmt), check out tag `6.2.0`, and use CMake to build the package.
  - `spdlog`: Follow the instructions [here](https://github.com/gabime/spdlog#install)
    - e.g. In the Travis CI build script, I clone [the GitHub repo](https://github.com/gabime/spdlog), check out tag `v1.5.0`, and use CMake to build the package.
  - `Inja`: Follow the instructions [here](https://pantor.github.io/inja)
    - e.g. In the Travis CI build script, I clone [the GitHub repo](https://github.com/pantor/inja), check out tag `v2.2.0`, and use CMake to build the package.
- Use CMake to build from source:
  - e.g. on UNIX-like systems (or MSYS2 on Windows):

  ```bash
  git clone git://github.com/Krantz-XRF/auto-FFI
  cd auto-FFI
  mkdir build && cd build
  cmake ..
  cmake --build .
  ```

  - e.g. on Windows: Visual Studio 2019 has CMake support.

### Troubleshooting

If you have multiple version of LLVM/Clang installed, or if CMake fails to find your installation, you may want to define `CMAKE_PREFIX_PATH` (or `LLVM_DIR` and `Clang_DIR`) when invoking CMake.

## Usage

```text
auto-FFI [options] [<file> ...]
```

## Options

- Generic Options:
  | Option        | Description                                                       |
  | ------------- | ----------------------------------------------------------------- |
  | `--help`      | Display available options (`--help-hidden` for more)              |
  | `--help-list` | Display list of available options (`--help-list-hidden` for more) |
  | `--version`   | Display the version of this program                               |
- auto-FFI Options:
  | Option          | Description                                    |
  | --------------- | ---------------------------------------------- |
  | `--dump-config` | Dump configuration options to stdout and exit. |
  | `--verbose`     | Print verbose output message.                  |
  | `--yaml`        | Dump YAML for entities.                        |

## Configuration File

The configuration file of auto-FFI use the YAML format. To begin, run `auto-FFI --dump-config > config.yaml` to get an example configuration file. The option names should be self-explanatory.

## Code Generation Template

The code generation template of auto-FFI can be found in [the source tree](https://github.com/Krantz-XRF/auto-FFI/blob/master/src/default_template.hs). Also, run `auto-FFI --dump-template > template.hs` will provide you the default template. For template grammar, refer to [documentation of Inja](https://github.com/pantor/inja).

## Read More

Please refer to [the Releases page](https://github.com/Krantz-XRF/auto-FFI/releases) for my presentation slides and reports.
