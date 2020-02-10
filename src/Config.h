#pragma once

#include <llvm/ObjectYAML/YAML.h>

#include "Marshaller.h"

namespace config
{
struct Config
{
#define FIELD(Type, Field, Default) Type Field{Default};
#include "Config.def"
};
} // namespace config
