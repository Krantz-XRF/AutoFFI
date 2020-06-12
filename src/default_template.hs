{-# LANGUAGE EmptyDataDecls #-}
{-# LANGUAGE ForeignFunctionInterface #-}
{-# LANGUAGE PatternSynonyms #-}
{-# LANGUAGE GeneralizedNewtypeDeriving #-}
{-# LANGUAGE DerivingStrategies #-}
{-# OPTIONS_GHC -Wno-missing-pattern-synonym-signatures #-}
{-# OPTIONS_GHC -Wno-unused-imports #-}
module {{ gen_name("module_name", module.name) }} where

import Foreign.C.Types
import Foreign.C.String
import Foreign.Storable
import Foreign.Ptr
import Foreign.Marshal.Alloc

import Data.Int
import Data.Word
import Data.List

## for imp in module.imports
import {{ imp }}
## endfor

## for s in module.structs
data {{ gen_name("type_ctor", s.name) }} = {{ gen_name("data_ctor", s.name) }}
##   if length(s.fields)
##     for field in s.fields
  {% if loop.is_first %}{ {% else %}, {% endif %}{{ gen_scoped_name("variable", field.name, s.name) }} :: {{ gen_type(field.type) }}
##     endfor
  }
##   endif
##   if cfg.generate_storable_instances
instance Storable {{ gen_name("type_ctor", s.name) }} where
  sizeOf _ =
    let sizeAlign x = (sizeOf x, alignment x)
        makeSize = foldl' (\sz (sx, ax) -> alignTo sz ax + sx) 0
        alignTo s a = ((s + a - 1) `quot` a) * a
    in makeSize [{% for field in s.fields %}sizeAlign (undefined :: {{ gen_type(field.type) }}){% if not loop.is_last %}, {% endif %}{% endfor %}]
  alignment _ = maximum [{% for field in s.fields %}alignment (undefined :: {{ gen_type(field.type) }}){% if not loop.is_last %}, {% endif %}{% endfor %}]
  peek _p'0 =
    let sizeAlign x = (sizeOf x, alignment x)
        makeSize = foldl' (\sz (sx, ax) -> alignTo sz ax + sx) 0
        alignTo s a = ((s + a - 1) `quot` a) * a
        offsets = map makeSize $ inits [{% for field in s.fields %}sizeAlign (undefined :: {{ gen_type(field.type) }}){% if not loop.is_last %}, {% endif %}{% endfor %}]
    in Record
##     for _ in s.fields
    {% if loop.is_first %}<$>{% else %}<*>{% endif %} peekByteOff _p'0 (offsets !! {{ loop.index }})
##     endfor
  poke _p'0 _r'0 =
    let sizeAlign x = (sizeOf x, alignment x)
        makeSize = foldl' (\sz (sx, ax) -> alignTo sz ax + sx) 0
        alignTo s a = ((s + a - 1) `quot` a) * a
        offsets = map makeSize $ inits [{% for field in s.fields %}sizeAlign (undefined :: {{ gen_type(field.type) }}){% if not loop.is_last %}, {% endif %}{% endfor %}]
    in do
##     for field in s.fields
    pokeByteOff _p'0 (offsets !! {{ loop.index }}) ({{ gen_scoped_name("variable", field.name, s.name) }} _r'0)
##     endfor
##   endif
## endfor

## for e in module.enums
newtype {{ gen_name("type_ctor", e.name) }} = {{ gen_name("data_ctor", e.name) }}{ unwrap{{ gen_name("data_ctor", e.name) }} :: {{ gen_type(e.underlying_type) }} }
##   if cfg.generate_storable_instances
  deriving (Storable)
##   endif
##   for name, value in e.enumerators
pattern {{ gen_name("type_ctor", name) }} = {{ gen_name("data_ctor", e.name) }} {{ value }}
##   endfor
## endfor

## for e in module.entities
foreign import ccall "{{ e.name }}" {{ gen_name("variable", e.name) }} :: {{ gen_type(e.type) }}
## endfor
