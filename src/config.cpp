#include "config.h"

#include <clang/Basic/SourceManager.h>

bool ffi::validate_config(const config& cfg, clang::DiagnosticsEngine& diags) {
  if (cfg.allow_rank_n_types && !cfg.void_ptr_as_any_ptr) {
    const auto id = diags.getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "RankNTypes is only required by void_ptr_as_any_ptr, enabling "
        "allow_rank_n_types alone adds no extra functionality, but causes "
        "incompatibility on compilers without RankNTypes support.\n");
    diags.Report(id);
  }
  return true;
}
