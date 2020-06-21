---
title: auto-FFI Final Report
subtitle: Automatic Haskell Binding Generator for C API
documentclass: scrartcl
geometry: [margin=2cm, bottom=3cm]
date: June 2020
author: Xie Ruifeng
raw_attribute: true
numbersections: true
header-includes: |
  \usepackage{multicol}
  \titlehead{School of EECS, Peking University \hfill ID: 1700012871}
---

# Introduction

## Motivation

The *Haskell Report* defines a foreign function interface (FFI) for importing functions written in a foreign language (typically C) and using them in the Haskell world. FFI is used to write bindings to foreign libraries in Haskell, and it is common practice to divide a library binding into 2 separate parts: importing C functions as a low level interface, and writing a Haskell flavour high level interface. Importing C functions is a repeated human labour, requiring little creativity but faithful translation.

`auto-FFI` is designed to aid low level interface writing. In fact, with only a little guidance from the programmer, `auto-FFI` is able to automatically generate foreign function imports, and corresbonding Haskell ADTs for C `enum`s and `struct`s.

## Overview

A C library usually consists of two parts: header files (available in the include path) and implementation (as a linkable static or shared library). Generating Haskell binding requires only information from the header files. We chose Clang for analysing C headers.

A typical a Haskell binding for library `A` usually uses such a module hierachy: `A.LowLevel.*` for low level bindings, and `A.*` for high level Haskell interface. `auto-FFI` follows this convention, and take care of the `A.LowLevel.*` part.

The design principle for `auto-FFI` is that the users can get started with only the `auto-FFI` executable (thus without looking up the documentation at all):

- `auto-FFI --help` lists all available options:

  | Option              | Description                                                    |
  | ------------------- | -------------------------------------------------------------- |
  | `--dump-config`     | Dump configuration options to stdout and exit                  |
  | `--dump-template`   | Dump output template to stdout and exit                        |
  | `--verbose=<level>` | Verbosity: [trace, debug, info, warning, error, critical, off] |
  | `--json`            | Dump JSON for generated modules                                |
  | `--yaml`            | Dump YAML for generated modules                                |

- `auto-FFI --dump-config` generates a default configuration to get started with;
- `auto-FFI --dump-template` generates a default backend template for advanced use;

## Background

### Basic Foreign Types

All C booleans, integers, floating point numbers, and characters have their Haskell counterparts. Due to some historical reasons, these C types are needlessly complicated.

C integers (e.g. `int`, `long long`, `unsigned`), by their nature, have platform-dependent width; but the C standard also requires the standard headers to define some fixed-size type alias for C integers, e.g. `int32_t`, `uintptr_t`, `int_least8_t`, `intmax_t`. Besides, C characters have unspecified signedness, thus in portable C programs, `char`, `signed char`, `unsigned char` should be treated as if they were 3 distinguished types; for Unicode support, C have wide characters `wchar_t` (and unlike in C++, `wchar_t` is not a keyword, but a type alias). Furthermore, the boolean type, `bool`, and boolean constants `true` and `false`, are **macros**.

To faithfully reflect the scenario in C[^1], Haskell have flexible-sized C integers (`CInt`, `CULong`, etc.), fixed-sized integers (`Int64`, `Word8`, `CIntMax`, etc.), three flavours of C narrow characters (`CChar`, `CSChar`, `CUChar`), wide characters (`CWchar`), booleans (`Bool`), and the two floating point numbers (`Floar`, `Double`).

[^1]: See the appendix for specification in *the Haskell Report*.

The challenge here for `auto-FFI` is to correctly detect all those C type aliases. Below we give an illustration for such a mess:

```c
// in standard system headers:
#define bool                int
typedef int                 int32_t;
typedef unsigned            uint32_t;

// the library decided to have a custom pointer type
typedef int32_t*  MyFancyPInt32;
// the library decided to have a custom fixed-size integer
// instead of #include <stdint.h>
typedef long      int64_t;
```

| C Declaration      | Canonical Type | Desired Type     | Haskell Counterpart | Remarks        |
| ------------------ | -------------- | ---------------- | ------------------- | -------------- |
| `uint32_t x;`      | `unsigned`     | `uint32_t`       | `Word32`            |                |
| `MyFancyPInt32 p;` | `int*`         | `int32_t*`       | `Ptr Int32`         |                |
| `int64_t y;`       | `long`         | `int64_t`/`long` | `Int64`/`CLong`     | Configurable   |
| `bool b;`          | `int`          | `bool`           | `Bool`              | Not Detectable |

We try our best to detect desired C types, and provides an option `allow_custom_fixed_size_int` to control whether type aliases defined in user headers should be treated the same way as in system headers.

### Pure and Impure Functions

All functions are treated impure. Thanks to `unsafePerformIO`, the pure functions, once noticed during writing the high level interface, can be made pure again easily. Consider the following example:

```c
double sin(double);
```

```haskell
foreign import ccall "sin" c_sin :: Double -> IO Double
sinPure :: Double -> Double
sinPure = unsafePerformIO . c_sin
```

# Interface Design

## Command Line Interface

We use configuration files for controlling details in the generation process. YAML is a flexible and human-readable format, and is adopted by `clang-format`, `clang-tidy`, etc. for configuration file format. `auto-FFI` accepts several YAML configuration files in one run, each corresponds to one library binding, and generate them all.

Below is the default configuration, obtained by running `auto-FFI --dump-config` (comments added for clarity). All these configurations will be explained in detail in the following sections.

```yaml
# auto-FFI workflow details
allow_custom_fixed_size_int: false
assume_extern_c: false
warn_no_c_linkage: true
warn_no_external_formal_linkage: false
generate_storable_instances: true
# name converters will be discussed later
name_converters: # all, module, type, ctor, var
file_name_converters: {}
# library specific information
library_name:    Library
root_directory:  ''
output_directory: ''
# name clash handling
module_name_mapping: {}
explicit_name_mapping: {}
# custom backend template
custom_template: ''
inja_set_trim_blocks: false
inja_set_lstrip_blocks: false
```

## Name Converters

### Purpose

Haskell requires type constructors, data constructors, and module names to begin with an uppercase letter, while functions (variables) should begin with a lowercase letter or an underscore. Besides, library authors might want to enforce naming conventions on the generated code. We provide name converters to ease entity renaming, and provide `module_name_mapping` and `explicit_name_mapping` for arbitray renaming (mainly for name clash handling).

### Usage

`auto-FFI` uses name converters to convert names, e.g.

```yaml
name_converters:
  module:
    remove_prefix:   'hb-'
    remove_suffix:   '.h'
    add_prefix:      ''
    add_suffix:      ''
    output_case:     CamelCase
    output_variant:  module_name
```

All name converters: `name_converters.(all|module|type|ctor|var)`.

The algorithm can be roughly described as:

- Break the name into several segments by underscores;
- Break each segment into several parts ...
  - Before the second uppercase letter;
  - Unless the whole segment are all uppercase;
- Convert each part separately;
- Combine all these parts together;

### Four Case Conventions

| Input          | CamelCase    | snake_all_lower | Snake_Init_Upper | SNAKE_ALL_UPPER |
| -------------- | ------------ | --------------- | ---------------- | --------------- |
| `some_id_1`    | `SomeId1`    | `some_id_1`     | `Some_Id_1`      | `SOME_ID_1`     |
| `some_id1`     | `SomeId1`    | `some_id1`      | `Some_Id1`       | `SOME_ID1`      |
| `some_ID_Here` | `SomeIdHere` | `some_id_here`  | `Some_Id_Here`   | `SOME_ID_HERE`  |
| `FT_Buffer`    | `FtBuffer`   | `ft_buffer`     | `Ft_Buffer`      | `FT_BUFFER`     |

Name clashes may happen here. This would be discussed later.

### Name Variants

With lower initials: `variable`.

With upper initials: `module_name`, `type_ctor`, `data_ctor`.

Modifications by name variants are done after case convertions.

## Name Clash Handling

### Circumstances

- Field names clash with global functions;
- `some_id` and `some_id_` converted to `CamelCase` clash with each other;
- Function/variable names clash with Haskell keywords;

### `auto-FFI` Behaviour

- Name clashes are detected and reported as errors
- No automatic fixes are performed
- Haskell code are still generated
- `auto-FFI` returns non-zero indicating failure

### Error message for illustration

For an input C header:

```c
// the following names clash with Haskell keywords
bool hiding;
int Data__;
int _data;
int DATA;

struct record { int32_t name_clash_here; };
void* name_clash_here;
```

`auto-FFI` will produce the following error message:

```text
test.yaml: error: the following variable names
  in 'test.h' all convert to 'data':
- DATA
- Data__
- _data
test.yaml: info: 'data' is a Haskell keyword.
test.yaml: error: the following variable names
  in 'test.h' all convert to 'nameClashHere':
- record.name_clash_here
- name_clash_here
test.yaml: error: the following variable names
  in 'test.h' all convert to 'hiding':
- hiding
test.yaml: info: 'hiding' is a Haskell keyword.
```

### Use Configuration to Fix Name Clashes

```yaml
explicit_name_mapping:
  test.h:
    variables:
      name_clash_here: nameClashHere
      record.name_clash_here: recordNameClashHere
      DATA:   "data1"
      Data__: "data2"
      _data:  "data3"
      hiding: "hiding_"
```

If the configuration does not fix some name clashes or introduce some new ones, errors are still generated in the next run.

# Demo

## Demo I: Variables

`LANGUAGE` pragmas and `import`s are omitted, output reformated.

:::::::::::::: {.columns .tex-multicols}
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
```

`\columnbreak`{=latex}

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
```

:::
::::::::::::::

## Demo II: Functions

`LANGUAGE` pragmas and `import`s are omitted, output reformated.

:::::::::::::: {.columns .tex-multicols}

```c
int f(
  int x,
  Record* y,
  int (*z)(int));
```

`\columnbreak`{=latex}

```haskell
foreign import ccall "f" f
  :: CInt
  -> Ptr Record
  -> FunPtr (CInt -> IO CInt)
  -> IO CInt
```

::::::::::::::

## Demo III: Structures and Enumerations

`LANGUAGE` pragmas and `import`s are omitted, output reformated.

:::::::::::::: {.columns .tex-multicols}

```c
struct record {
  int32_t x;
  ptrdiff_t* p;
};



enum some_enum {


  E_ENUM_ITEM_1,
  E_ENUM_ITEM_2,
};
```

`\columnbreak`{=latex}

```haskell
data Record = Record
  { x :: CInt
  , p :: Ptr CPtrdiff
  }
instance Storable Record where
  -- discussed in the next slide

newtype SomeEnum
  = SomeEnum{ unwrapSomeEnum :: CInt }
  deriving newtype (Storable)
pattern EEnumItem1 = SomeEnum 0
pattern EEnumItem2 = SomeEnum 1
```

::::::::::::::

## Demo III: `Storable` for Structures

### Structure Definition

```haskell
data Record = Record { x :: CInt, p :: Ptr CPtrdiff }
```

### Utility Functions

```haskell
sizeAlign :: Storable a => a -> (Int, Int)
sizeAlign x = (sizeOf x, alignment x)

makeSize :: [(Int, Int)] -> Int
makeSize = foldl' (\sz (sx, ax) -> alignTo sz ax + sx) 0

alignTo :: Int -> Int -> Int
alignTo s a = ((s + a - 1) `quot` a) * a
```

### Field Offsets

```haskell
offsets = map makeSize $ inits
  [ sizeAlign (undefined :: Int32)
  , sizeAlign (undefined :: Ptr CPtrdiff) ]
```

### Storable Instance

```haskell
instance Storable Record where
  sizeOf _ =
    let {- sizeAlign, makeSize, alignTo -}
    in makeSize
    [ sizeAlign (undefined :: Int32)
    , sizeAlign (undefined :: Ptr CPtrdiff) ]
  alignment _ = maximum
    [ alignment (undefined :: Int32)
    , alignment (undefined :: Ptr CPtrdiff) ]
  peek _p'0 =
    let {- sizeAlign, makeSize, alignTo, offsets -}
    in Record
    <$> peekByteOff _p'0 (offsets !! 0)
    <*> peekByteOff _p'0 (offsets !! 1)
  poke _p'0 _r'0 =
    let {- sizeAlign, makeSize, alignTo, offsets -}
    in do
    pokeByteOff _p'0 (offsets !! 0) (x _r'0)
    pokeByteOff _p'0 (offsets !! 1) (p _r'0)
```

# Project Structure

## Frontend: LLVM/Clang

The Clang AST is traversed recursively.

Previously, only top-level declarations are analysed and collected. It turns out the C namespace is flat, so nested structures are not indeed nested. Solved by performing a deep analysis on the Clang AST.

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

### The Inja String Template Engine

We use the Inja string template engine for backend code generation. This provides flexibility for custom code generation without modifying the `auto-FFI` source code. Also, this make the codegen process transparent (since the default codegen runs in the same way as custom ones), and provides a tool for reasoning on code generation.

The module structure is provided to the codegen template as a JSON object, which can be further processed by the following callbacks:

- `gen_type(type)`: convert the type from an internal representation to Haskell code;
- `gen_name(variant, name)`: convert a name using the name converter and explicit name mapping specified in the configuration file;
- `gen_variable(name)`: alias for `gen_name("variable", name)`;
- `gen_data_ctor(name)`: alias for `gen_name("data_ctor", name)`;
- `gen_type_ctor(name)`: alias for `gen_name("type_ctor", name)`;
- `gen_module_name(name)`: alias for `gen_name("module_name", name)`;
- `gen_scoped(name, scope)`": convert a struct field name;

To customize this part, one can first obtain the default template using `auto-FFI --dump-template`, modify the template file, and specify `custom_template: '/path/to/template'` in the configuration. The default template is `src/default_template.hs` in the source tree, and is embedded into the `auto-FFI` program executable as a string constant with a CMake module `cmake/embed_files.cmake`.

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
instance Storable <StructName> where {- omitted -}
```

### Enumeration

```haskell
newtype <EnumName> = <CtorName>
  { unwrap<EnumName> :: <UnderlyingType> }
  deriving (Storable)
pattern <EnumItem> :: <EnumName>
pattern <EnumItem> = <CtorName> <value>
```

# Evaluation

This project is a source to source compiler, which produces a source library as its output. I first write the binding myself, design the code generation workflow according to that binding, and compare the result with the desired output.

I test this project with a C header including all manually constructed edge cases. To test this project, run `auto-FFI` with a test configuration file `test.yaml`, load the generated module into GHCi, and play with the generated module.

# Future Works

## Instance Generation

The most useful (and complex) `Storable` can be automatically generated, despite that the implementation issues undefined behaviour on packed structs.

There are other potentially useful instances:

- `Enum` for `enum` types
  - Allow `enum`s as ADTs, controled by an option
- `Eq` for implementable data types
- `Show` for implementable data types

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

It's difficult to determine whether an integer is used as array/string length. This piece of information is usually only available from the documentation. We leave this for the high level binding, which should be hand-written with careful design.

# Acknowledgement

I would like to express my special thanks of gratitude to my teacher (Liu Xianhua) for his guidance during the development of this project. I will continue working on this project, and try to introduce it to the Haskell community.

`\appendix`{=latex}

# Haskell FFI Specification

According to *Haskell Report*, we have the following correspondance:

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

Additionally, these FFI types are available in `Foreign.C.Types`:

| Haskell symbol | C symbol             | Remarks        |
| -------------- | -------------------- | -------------- |
| `CSChar`       | `signed char`        |                |
| `CUChar`       | `unsigned char`      |                |
| `CWchar`       | `wchar_t`            |                |
| `CShort`       | `short`              |                |
| `CUShort`      | `unsigned short`     |                |
| `CUInt`        | `unsigned int`       |                |
| `CLong`        | `long`               |                |
| `CULong`       | `unsigned long`      |                |
| `CPtrdiff`     | `ptrdiff_t`          |                |
| `CSize`        | `size_t`             |                |
| `CSigAtomic`   | `sig_atomic_t`       | Not supported. |
| `CLLong`       | `long long`          |                |
| `CULLong`      | `unsigned long long` |                |
| `CIntPtr`      | `intptr_t`           |                |
| `CUIntPtr`     | `uintptr_t`          |                |
| `CIntMax`      | `intmax_t`           |                |
| `CUIntMax`     | `uintmax_t`          |                |
| `CClock`       | `clock_t`            | Not supported. |
| `CTime`        | `time_t`             | Not supported. |
| `CUSeconds`    | `useconds_t`         | Not supported. |
| `CSUSeconds`   | `suseconds_t`        | Not supported. |
