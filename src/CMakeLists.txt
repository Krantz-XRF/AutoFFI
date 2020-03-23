target_sources(AutoFFI PRIVATE
  # Driver Program
  "Config.def" "Config.h" "AutoFFI.cpp"
  # C Type Information
  "Types.def" "Types.h" "Entity.h"
  "Function.h" "OpaqueType.h" "PointerType.h" "Tag.h"
  "PrimTypes.def" "PrimTypes.h" "PrimTypes.cpp"
  # Haskell CodeGen
  "HaskellCodeGen.cpp" "HaskellCodeGen.h"
  "Marshaller.h" "Marshaller.cpp"
  "VisitTypes.h" "VisitTypes.cpp"
  "Module.h" "Module.cpp"
  # Clang Interface
  "MatcherWrapper.h"
  # LLVM YAML
  "YAML.h")
