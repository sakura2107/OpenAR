#pragma once
#include <filesystem>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace ar::core {

// Absolute path of the running executable (empty on failure).
inline std::filesystem::path exe_path() {
#ifdef _WIN32
    std::wstring buffer(32 * 1024, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0 || size >= buffer.size())
        return {};
    buffer.resize(size);
    return std::filesystem::path(std::move(buffer));
#else
    return {};
#endif
}

inline std::filesystem::path exe_dir() {
    return exe_path().parent_path();
}

// Walks upward from the executable directory looking for a directory that
// contains `marker` (e.g. "models"). Makes the demo independent of the
// current working directory. Returns an empty path when nothing is found.
inline std::filesystem::path find_root_containing(const std::string &marker, int max_depth = 8) {
    std::error_code ec;
    auto dir = exe_dir();
    for (int depth = 0; depth <= max_depth; ++depth) {
        if (std::filesystem::exists(dir / marker, ec))
            return dir;
        const auto parent = dir.parent_path();
        if (parent == dir)
            break;
        dir = parent;
    }
    return {};
}

} // namespace ar::core
