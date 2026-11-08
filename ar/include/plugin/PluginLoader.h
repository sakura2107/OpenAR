#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "log/Logger.h"

namespace ar::plugin {

// Loads every DLL in `dir` that exports the ARFW plugin ABI (see PluginApi.h)
// and registers its backends in the corresponding registries (matcher,
// controller, device controller, OCR engine). DLLs that are not valid plugins
// (missing exports, wrong ABI version, failed creation) are skipped with a
// warning; a failing plugin never prevents the others from loading.
//
// Call once at startup, e.g. load_plugins(exe_dir() / "plugins", &logger).
// Loaded modules stay alive for the rest of the process; plugins are NOT
// hot-unloaded. Returns the successfully loaded backend names.
std::vector<std::string> load_plugins(const std::filesystem::path &dir, ar::log::ILogger *logger = nullptr);
// Call after all startup plugin directories have been loaded.
void freeze_registries();

} // namespace ar::plugin
