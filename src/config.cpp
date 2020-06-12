#include "config.h"

#include <set>

#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

bool ffi::validate_config(const config& cfg, spdlog::logger& logger) {
  if (cfg.generate_storable_instances)
    logger.warn(
        "Clang provides no interface to determine whether a struct is packed "
        "or not, so we pretend all structs are not packed and generate normal "
        "Storable instances for them. If you use the generated Storable "
        "instances on a packed struct, the behaviour is undefined.");
  return true;
}

namespace {
const std::set<std::string_view> haskell_keywords{
    "as",        "case",   "class",  "data",    "default", "deriving",
    "do",        "else",   "forall", "foreign", "hiding",  "if",
    "in",        "import", "infix",  "infixl",  "infixr",  "instance",
    "let",       "mdo",    "module", "newtype", "of",      "proc",
    "qualified", "rec",    "then",   "type",    "where",
};
}  // namespace

int ffi::name_clashes(const name_resolver::rev_name_map& m,
                      spdlog::logger& logger, std::string_view kind,
                      std::string_view scope) {
  auto conflict{0};
  for (auto it = begin(m); it != end(m);) {
    const auto cur = it->first;
    const bool key_clash = haskell_keywords.find(cur) != end(haskell_keywords);
    // no clash
    if (const auto n = next(it);
        !key_clash && (n == end(m) || n->first != cur)) {
      ++it;
      continue;
    }
    // clash with a keyword or with each other
    fmt::memory_buffer buf;
    format_to(buf, "the following {} names in '{}' all convert to '{}':", kind,
              scope, cur);
    for (; it != end(m) && it->first == cur; ++it)
      format_to(buf, "\n- {}", it->second);
    logger.error(to_string(buf));
    if (key_clash) logger.info("'{}' is a Haskell keyword.", cur);
    ++conflict;
  }
  return conflict;
}
