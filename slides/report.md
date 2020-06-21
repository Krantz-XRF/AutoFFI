---
title: auto-FFI Final Report
date: June 2020
author: Xie Ruifeng
institute: School of EECS, Peking University
raw_attribute: true
toc: true
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

`\scriptsize`{=latex}

```yaml
allow_custom_fixed_size_int: false
assume_extern_c: false
warn_no_c_linkage: true
warn_no_external_formal_linkage: false
void_ptr_as_any_ptr: true
allow_rank_n_types: false
generate_storable_instances: true
file_name_converters: {}
library_name:    Library
root_directory:  ''
output_directory: ''
file_names:      []
is_header_group: []
module_name_mapping: {}
explicit_name_mapping: {}
# all name converters omitted to fit in one slide
```

## Name Converters

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

## Name Converters Done Right

### Four Case Conventions

`\tiny`{=latex}

| Input          | CamelCase    | snake_all_lower | Snake_Init_Upper | SNAKE_ALL_UPPER |
| -------------- | ------------ | --------------- | ---------------- | --------------- |
| `some_id_1`    | `SomeId1`    | `some_id_1`     | `Some_Id_1`      | `SOME_ID_1`     |
| `some_id1`     | `SomeId1`    | `some_id1`      | `Some_Id1`       | `SOME_ID1`      |
| `some_ID_Here` | `SomeIdHere` | `some_id_here`  | `Some_Id_Here`   | `SOME_ID_HERE`  |
| `FT_Buffer`    | `FtBuffer`   | `ft_buffer`     | `Ft_Buffer`      | `FT_BUFFER`     |

`\normalsize`{=latex}
Name clashes may happen here. This would be discussed later.

### Name Variants

With lower initials: `variable`.

With upper initials: `module_name`, `type_ctor`, `data_ctor`.

Modifications by name variants are done after case convertions.

## Demo I: Variables

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
```

:::
::::::::::::::

## Demo II: Functions

`LANGUAGE` pragmas and `import`s are omitted, output reformated.

:::::::::::::: {.columns}
::: {.column width="40%"}

```c
int f(
  int x,
  Record* y,
  int (*z)(int));
```

:::
::: {.column width="60%"}

```haskell
foreign import ccall "f" f
  :: CInt
  -> Ptr Record
  -> FunPtr (CInt -> IO CInt)
  -> IO CInt
```

:::
::::::::::::::

## Demo III: Structures and Enumerations

`LANGUAGE` pragmas and `import`s are omitted, output reformated.

:::::::::::::: {.columns}
::: {.column width="30%"}

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

:::
::: {.column width="70%"}

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

:::
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

## Demo III: `Storable` for Structures Cont'd

### Structure Definition

```haskell
data Record = Record { x :: CInt, p :: Ptr CPtrdiff }
```

### Storable Instance

`\scriptsize`{=latex}

```haskell
instance Storable Record where
  sizeOf _ =
    let {- sizeAlign, makeSize, alignTo -} in makeSize
    [ sizeAlign (undefined :: Int32)
    , sizeAlign (undefined :: Ptr CPtrdiff) ]
  alignment _ = maximum
    [ alignment (undefined :: Int32)
    , alignment (undefined :: Ptr CPtrdiff) ]
  peek p =
    let {- sizeAlign, makeSize, alignTo -} in Record
    <$> peekByteOff p (makeSize [])
    <*> peekByteOff p (makeSize [sizeAlign (undefined :: Int32)])
  poke p r =
    let {- sizeAlign, makeSize, alignTo -} in do
    pokeByteOff p (makeSize []) (x r)
    pokeByteOff p (makeSize [sizeAlign (undefined :: Int32)]) (p r)
```

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

# Project Structure

## Frontend: LLVM/Clang

### Input: Clang AST

The Clang AST is traversed recursively.

Previously, only top-level declarations are analysed and collected. It turns out the C namespace is flat, so nested structures are not indeed nested. Solved by performing a deep analysis on the Clang AST.

### Special Case: Fixed-Size Integers

The `(u)int<width>_t` types are all type aliases, and thus should be specially handled. By default, only type aliases from system headers are treated as candidate types. To relax this restriction, use the `allow_custom_fixed_size_int` option.

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

## Name Clash Handling{.allowframebreaks}

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

`\scriptsize`{=latex}

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

# Future Works

## Custom Templates

Currently, the code generation is hard coded in the program. It would be nice if the output template is customizable by using a string template engine.

Below are the candidate template engines. It seems `Inja` is the most suitable one, and more investigations planned.

`\scriptsize`{=latex}

| Candidates                                                      | PROS                                                                           | CONS                                        |
| --------------------------------------------------------------- | ------------------------------------------------------------------------------ | ------------------------------------------- |
| [Inja](https://github.io/pantor/inja/)                          | Seems lightweight, still actively developed                                    | Use JSON for data, prehaps requires copying |
| [Jinja2CppLight](https://github.com/hughperkins/Jinja2CppLight) | Claims to be lightweight, no dependency                                        | No update since May 2018                    |
| [Jinja2Cpp](https://github.com/jinja2cpp/Jinja2Cpp)             | Full featured, error reporting, Jinja2 compatibility, still actively developed | Depends on **Boost**, and some others       |
| [CTemplate](https://github.com/OlafvdSpek/ctemplate)            | Google product                                                                 | Google product, **`autogen` workflow**      |

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

It's difficult to determine whether an integer is used as array/string length. This piece of information is usually only available from the documentation.
