target_embed_files(auto-ffi
  NAMESPACE ffi HEADER templates.h
  INPUT default_template.hs)

target_sources(auto-ffi PRIVATE
  # Driver Program
  "config.h" "config.cpp" "main.cpp" "driver.h" "driver.cpp"
  # C Type Information
  "types.def" "types.h" "types.cpp"
  "function_type.h" "opaque_type.h" "pointer_type.h" "tag_type.h"
  "prim_types.def" "prim_types.h" "prim_types.cpp"
  # Haskell CodeGen
  "haskell_code_gen.cpp" "haskell_code_gen.h"
  "name_converter.h" "name_converter.cpp"
  "visit_types.h" "visit_types.cpp"
  "module.h" "module.cpp"
  # Inja Related
  "inja_callback.h"
  # LLVM YAML/Nlohmann JSON
  "yaml.h" "json.h" "json.cpp")
