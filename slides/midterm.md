---
title: auto-FFI Mid-Term Report
theme: metropolis
date: April 2020
author: Xie Ruifeng
institute: School of EECS, Peking University
raw_attribute: true
---

# Usage

## Command Line Interface

`auto-FFI` is a typical Clang Tool. It reads the configuration from the `<file>`s and writes the result to the specified output files.

```bash
auto-FFI [options] [<file> ...]
```

### Generic Options

Inherited from Clang Tool: `--help`, `--help-list`, `--version`.

### auto-FFI Options

| Option          | Description                                    |
| --------------- | ---------------------------------------------- |
| `--dump-config` | Dump configuration options to stdout and exit. |
| `--verbose`     | Print verbose output message.                  |
| `--yaml`        | Dump YAML for entities.                        |

## Configuration Files

`auto-FFI` uses YAML configuration files to guide its workflow. Here is an example config obtained from `auto-FFI --dump-config`.

```yaml
---
AllowCustomFixedSizeInt: false
AssumeExternC:   false
WarnNoCLinkage:  true
WarnNoExternalFormalLinkage: false
LibraryName:     Library
RootDirectory:   ''
OutputDirectory: ''
# All marshallers omitted to fit in one slide
...
```

## Name Marshallers

Haskell requires types to begin with an upper case letter, functions with a lower case letter or `'_'`, and by convention, Haskell programs have `PascalCase` types and `camelCase` functions.

`auto-FFI` uses name marshallers to convert names, e.g.

```yaml
ModuleMarshaller:
  removePrefix:    'hb-'
  removeSuffix:    '.h'
  addPrefix:       ''
  addSuffix:       ''
  outputCase:      PascalCase
```

## Demo

`LANGUAGE` pragmas and `import`s are omitted, output reformated.

`\scriptsize`{=latex}

:::::::::::::: {.columns}
::: {.column width="40%"}

```c
#include <stdint.h>

unsigned long a;
int_least16_t b;
uintptr_t c;

char ch;
signed char sch;
unsigned char uch;

typedef int* PINT;
PINT** ppi;

struct Record { int32_t x; };

int f(int x,
      Record* y,
      int (*z)(int));
```

:::
::: {.column width="60%"}

```haskell
module Test.LowLevel.Test where

foreign import ccall "a" a :: CInt
foreign import ccall "b" b :: CShort
foreign import ccall "c" c :: CULLong

foreign import ccall "ch" ch :: CChar
foreign import ccall "sch" sch :: CSChar
foreign import ccall "uch" uch :: CUChar

foreign import ccall "ppi" ppi
  :: Ptr (Ptr (Ptr CInt))

data Record = Record { x :: CInt }

foreign import ccall "f" f
  :: CInt -> Ptr Record
  -> FunPtr (CInt -> IO CInt) -> IO CInt
```

:::
::::::::::::::

# Background

## Haskell FFI Specification

According to Haskell report, we have the following correspondance:

`\scriptsize`{=latex}

| C symbol      | Haskell symbol | Constraint on concrete C type                           |
| ------------- | -------------- | ------------------------------------------------------- |
| `HsChar`      | `Char`         | integral type                                           |
| `HsInt`       | `Int`          | signed integral type, $\ge$ 30 bit                      |
| `HsInt8`      | `Int8`         | signed integral type, 8 bit; `int8_t` if available      |
| `HsInt16`     | `Int16`        | signed integral type, 16 bit; `int16_t` if available    |
| `HsInt32`     | `Int32`        | signed integral type, 32 bit; `int32_t` if available    |
| `HsInt64`     | `Int64`        | signed integral type, 64 bit; `int64_t` if available    |
| `HsWord8`     | `Word8`        | unsigned integral type, 8 bit; `uint8_t` if available   |
| `HsWord16`    | `Word16`       | unsigned integral type, 16 bit; `uint16_t` if available |
| `HsWord32`    | `Word32`       | unsigned integral type, 32 bit; `uint32_t` if available |
| `HsWord64`    | `Word64`       | unsigned integral type, 64 bit; `uint64_t` if available |
| `HsFloat`     | `Float`        | floating point type                                     |
| `HsDouble`    | `Double`       | floating point type                                     |
| `HsBool`      | `Bool`         | `int`                                                   |
| `HsPtr`       | `Ptr a`        | `(void *)`                                              |
| `HsFunPtr`    | `FunPtr a`     | `(void (*)(void))`                                      |
| `HsStablePtr` | `StablePtr a`  | `(void *)`                                              |

## Haskell FFI Specification Cont'd

Additionally, these FFI types are available in `Foreign.C.Types`:

`\tiny`{=latex}

| Haskell symbol | C symbol             | Remarks            |
| -------------- | -------------------- | ------------------ |
| `CSChar`       | `signed char`        |                    |
| `CUChar`       | `unsigned char`      |                    |
| `CWchar`       | `wchar_t`            |                    |
| `CShort`       | `short`              |                    |
| `CUShort`      | `unsigned short`     |                    |
| `CUInt`        | `unsigned int`       |                    |
| `CLong`        | `long`               |                    |
| `CULong`       | `unsigned long`      |                    |
| `CPtrdiff`     | `ptrdiff_t`          |                    |
| `CSize`        | `size_t`             |                    |
| `CSigAtomic`   | `sig_atomic_t`       | Not supported yet. |
| `CLLong`       | `long long`          |                    |
| `CULLong`      | `unsigned long long` |                    |
| `CIntPtr`      | `intptr_t`           |                    |
| `CUIntPtr`     | `uintptr_t`          |                    |
| `CIntMax`      | `intmax_t`           |                    |
| `CUIntMax`     | `uintmax_t`          |                    |
| `CClock`       | `clock_t`            | Not supported yet. |
| `CTime`        | `time_t`             | Not supported yet. |
| `CUSeconds`    | `useconds_t`         | Not supported yet. |
| `CSUSeconds`   | `suseconds_t`        | Not supported yet. |

# Project Structure

## Frontend: LLVM/Clang

### Input: Clang AST

The Clang AST is traversed recursively. Only top-level declarations are analysed and collected.

### Special Case: Fixed-Size Integers

The `(u)int<width>_t` types are all type aliases, and thus should be specially handled. By default, only type aliases from system headers are treated as candidate types. To relax this restriction, use the `AllowCustomFixedSizeInt` option.

## Internal Representation

- `Type`
  - `ScalarType` : C/C++ primitive types
  - `OpaqueType` : C/C++ `enum`/`struct`
    - `typeName : string`
  - `FunctionType` : C/C++ functions
    - `returnType : Type`
    - `params : [(string, Type)]`
  - `PointerType` : pointers
    - `pointee : Type`
- `Tag`
  - `Structure`
    - `fields : [(string, Type)]`
  - `Enumeration`
    - `underlying_type : Type`
    - `enumerators : [(string, int)]`

## Backend: Haskell CodeGen

### Functions (Entities)

```haskell
foreign import ccall "<name>" <name> :: <Type>
```

### Structure

```haskell
data <StructName> = <CtorName>
  { <fieldName> :: <fieldType>
  , <fieldName> :: <fieldType>
  }
```

### Enumeration

```haskell
newtype <EnumName> = <CtorName>
  { unwrap<EnumName> :: <UnderlyingType> }
pattern <EnumItem> :: <EnumName>
pattern <EnumItem> = <CtorName> <value>
```

# TODO List

## Bug Fix

Primitive types not properly-handled:

- `ptrdiff_t`
- `size_t`
- `(u)intptr_t`
- `(u)intmax_t`

These are GHC extensions, not mentioned by Haskell Report 2010. Perhaps we should add an option to control this support.

Field names clash with global functions. I have not come up with a proper solution. We may well add apostrophes to field names and global functions, but that sounds crazy. Maybe tag types should be defined in separate modules and be imported qualified.

## Instance Generation

- `Storable` for all tag types
  - Alignments should be calculated independently from Clang
  - Or we could make use of existing `Storable` instances
  - Maybe provide an option for this?
- `Enum` for `enum` types
  - Allow `enum`s as ADTs, controled by an option
- (HARD) `Eq` for implementable data types
- (HARD) `Show` for implementable data types

## Name Resolution

- Resolve dependencies between modules
- Allow specifying external dependencies
- Resolve mutual dependency
- Entities/tags with the same name are considered the same
- Potential conflict between modules

## Automatic Marshallers

These are fairly complex stuffs.

- Array marshaller: `peekArray`, `withArray`, etc.
- String marshaller: `peekCString`, `withCString`, etc.
- etc.

It's difficult to determine whether an integer is used as array/string length. This piece of information is usually only available from the documentation.
