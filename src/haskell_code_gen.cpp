/* This file is part of auto-FFI (https://github.com/Krantz-XRF/auto-FFI).
 * Copyright (C) 2020 Xie Ruifeng
 *
 * auto-FFI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * auto-FFI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with auto-FFI.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "haskell_code_gen.h"

#include <fmt/format.h>
#include <gsl/gsl_assert>
#include <inja/inja.hpp>

#include "inja_callback.h"
#include "json.h"
#include "templates.h"

namespace {
constexpr size_t npos = std::numeric_limits<size_t>::max();
}  // namespace

void ffi::haskell_code_gen::gen_module(const std::string& name,
                                       const module_contents& mod) {
  // Set up name converters
  if (auto p = cfg.file_name_converters.find(name);
      p != cfg.file_name_converters.cend())
    cfg.name_converters.for_all.forward_converter = &p->second;
  else
    cfg.name_converters.for_all.forward_converter = nullptr;
  resolver = &cfg.explicit_name_mapping[name];

  // Locate output file
  auto mname = cfg.name_converters.for_module.convert(name);
  auto parent_dir = format(FMT_STRING("{}/{}/LowLevel"), cfg.output_directory,
                           cfg.library_name);
  auto mod_file = format(FMT_STRING("{}/{}.hs"), parent_dir, mname);
  llvm::sys::fs::create_directories(parent_dir);
  std::ofstream ofs{mod_file, std::ios::out};
  if (!ofs) return spdlog::error("Cannot open file '{}'.\n", mod_file);

  // Generate module
  inja::Environment env;
  add_callback(env, "gen_type",
               [this](ctype_ref t) { return gen_type(*t.pointee); });
  add_callback(env, "gen_name", [this](name_variant v, std::string_view n) {
    return gen_name(v, n);
  });
  add_callback(env, "gen_variable", [this](std::string_view n) {
    return gen_name(name_variant::variable, n);
  });
  add_callback(env, "gen_data_ctor", [this](std::string_view n) {
    return gen_name(name_variant::data_ctor, n);
  });
  add_callback(env, "gen_type_ctor", [this](std::string_view n) {
    return gen_name(name_variant::type_ctor, n);
  });
  add_callback(env, "gen_module_name", [this](std::string_view n) {
    return gen_name(name_variant::module_name, n);
  });
  add_callback(env, "gen_scoped",
               [this](std::string_view n, std::string_view scope) {
                 return gen_name(name_variant::variable, n, scope);
               });
  try {
    auto output_template = [this, &env] {
      if (!cfg.custom_template.empty()) {
        env.set_trim_blocks(cfg.inja_set_trim_blocks);
        env.set_lstrip_blocks(cfg.inja_set_lstrip_blocks);
        return env.parse_template(cfg.custom_template);
      } else {
        env.set_trim_blocks(true);
        return env.parse(default_template_hs);
      }
    }();
    auto data = nlohmann::json::object({{"module", mod}, {"cfg", cfg}});
    data["module"]["name"] = name;
    spdlog::trace("JSON data for template output:\n{}\n", data.dump(2));
    env.render_to(ofs, output_template, data);
  } catch (const std::runtime_error& e) {
    spdlog::error(e.what());
  }
}

std::string_view ffi::haskell_code_gen::gen_name(name_variant v,
                                                 std::string_view name,
                                                 std::string_view scope) {
  return name_resolve(v, {scope, name});
}

std::string ffi::haskell_code_gen::gen_type(const ctype& type) {
  std::string result;
  llvm::raw_string_ostream stream{result};
  gen_type(stream, type, false);
  return result;
}

void ffi::haskell_code_gen::gen_type(llvm::raw_ostream& os, const ctype& type,
                                     bool paren) {
  const auto idx = type.value.index();
  paren = paren && requires_paren(idx) && !is_cstring(type);
  if (paren) os << "(";
  switch (idx) {
#define TYPE(TypeName)                                  \
  case index<ctype::variant, TypeName>:                 \
    gen_##TypeName(os, std::get<TypeName>(type.value)); \
    break;
#include "types.def"
  }
  if (paren) os << ")";
}

void ffi::haskell_code_gen::gen_function_type(llvm::raw_ostream& os,
                                              const function_type& func) {
  for (const auto& tp : func.params) {
    gen_type(os, tp.second);
    os << " -> ";
  }
  os << "IO ";
  gen_type(os, *func.return_type, true);
}

void ffi::haskell_code_gen::gen_scalar_type(llvm::raw_ostream& os,
                                            const scalar_type& scalar) {
  os << scalar.as_haskell();
}

void ffi::haskell_code_gen::gen_opaque_type(llvm::raw_ostream& os,
                                            const opaque_type& opaque) {
  const auto t = gen_name(name_variant::type_ctor, opaque.name);
  os << llvm::StringRef{t.data(), t.size()};
}

void ffi::haskell_code_gen::gen_pointer_type(llvm::raw_ostream& os,
                                             const pointer_type& pointer) {
  auto& pointee = *pointer.pointee;
  if (is_cchar(pointee)) {
    os << "CString";
    return;
  }

  if (is_function(pointee))
    os << "FunPtr ";
  else
    os << "Ptr ";
  gen_type(os, pointee, true);
}

bool ffi::haskell_code_gen::requires_paren(int tid) {
  return tid == index<ctype::variant, function_type> ||
         tid == index<ctype::variant, pointer_type>;
}

bool ffi::haskell_code_gen::is_cchar(const ctype& type) {
  if (std::holds_alternative<scalar_type>(type.value)) {
    const auto& scalar = std::get<scalar_type>(type.value);
    return scalar.sign == scalar_type::Unspecified &&
           scalar.qualifier == scalar_type::Char &&
           scalar.width == scalar_type::WidthNone;
  }
  return false;
}

bool ffi::haskell_code_gen::is_cstring(const ctype& type) {
  if (std::holds_alternative<pointer_type>(type.value)) {
    const auto& pointer = std::get<pointer_type>(type.value);
    return is_cchar(*pointer.pointee);
  }
  return false;
}

bool ffi::haskell_code_gen::is_void(const ctype& type) {
  if (std::holds_alternative<scalar_type>(type.value)) {
    const auto& scalar = std::get<scalar_type>(type.value);
    return scalar.qualifier == scalar_type::Void;
  }
  return false;
}

bool ffi::haskell_code_gen::is_function(const ctype& type) {
  return std::holds_alternative<function_type>(type.value);
}

std::string_view ffi::haskell_code_gen::name_resolve(name_variant v,
                                                     scoped_name_view n) const {
  Expects(v != name_variant::preserving);
  const auto nv = static_cast<size_t>(v) - 1;
  Expects(0 <= nv && nv < 4);
  auto& conv = *std::array{
      &cfg.name_converters.for_var,
      &cfg.name_converters.for_module,
      &cfg.name_converters.for_type,
      &cfg.name_converters.for_ctor,
  }[nv];
  auto& fwd = *std::array{
      &resolver->variables,
      &cfg.module_name_mapping,
      &resolver->type_ctors,
      &resolver->data_ctors,
  }[nv];
  auto& rev = *std::array{
      &resolver->rev_variables,
      &cfg.rev_modules,
      &resolver->rev_type_ctors,
      &resolver->rev_data_ctors,
  }[nv];
  if (const auto p = fwd.find(n); p != fwd.cend()) return p->second;
  auto [p, _] = fwd.emplace(n.materialize(), conv.convert(n.name));
  rev.emplace(p->second, p->first);
  spdlog::trace("name '{}' get converted to '{}'.", n, p->second);
  return p->second;
}
