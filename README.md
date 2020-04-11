# auto-FFI

## Overview

A tool to generate Haskell FFI binding for C/C++ API.

It reads the configuration from the `<file>`s and writes the result to the specified output files.

## Usage

```text
auto-FFI [options] [<file> ...]
```

## Options

- Generic Options:
  | Option        | Description                                                     |
  | ------------- | --------------------------------------------------------------- |
  | `--help`      | Display available options (--help-hidden for more)              |
  | `--help-list` | Display list of available options (--help-list-hidden for more) |
  | `--version`   | Display the version of this program                             |
- auto-FFI Options:
  | Option          | Description                                    |
  | --------------- | ---------------------------------------------- |
  | `--dump-config` | Dump configuration options to stdout and exit. |
  | `--verbose`     | Print verbose output message.                  |
  | `--yaml`        | Dump YAML for entities.                        |

## Configuration File

The configuration file of auto-FFI use the YAML format. To begin, run `auto-FFI --dump-config > config.yaml` to get an example configuration file. The option names should be self-explanatory.
