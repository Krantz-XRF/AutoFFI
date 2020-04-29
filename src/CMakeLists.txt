target_sources(auto-ffi PRIVATE
  # Driver Program
  "config.h" "main.cpp" "driver.h" "driver.cpp"
  # C Type Information
  "types.def" "types.h" "types.cpp"
  "function_type.h" "opaque_type.h" "pointer_type.h" "tag_type.h"
  "prim_types.def" "prim_types.h" "prim_types.cpp"
  # Haskell CodeGen
  "haskell_code_gen.cpp" "haskell_code_gen.h"
  "name_converter.h" "name_converter.cpp"
  "visit_types.h" "visit_types.cpp"
  "module.h" "module.cpp"
  # LLVM YAML
  "yaml.h")
