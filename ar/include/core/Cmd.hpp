#pragma once
// Legacy source-compatible wrapper; new callers inspect ProcessResult.
#include "core/Process.h"
namespace ar::core {
inline bool run_process(const std::string &program, const std::string &args, std::string *text_out = nullptr,
                        std::vector<uint8_t> *bytes_out = nullptr, uint32_t timeout_ms = 30000) {
    ProcessOptions options;
    options.timeout = std::chrono::milliseconds(timeout_ms);
    auto result = run_process_command(std::filesystem::path(utf8_to_wide(program)), args, options);
    if (text_out) {
        *text_out = result.stdout_text();
        if (!result.stderr_text.empty())
            *text_out += result.stderr_text;
    }
    if (bytes_out)
        *bytes_out = std::move(result.stdout_bytes);
    return result.success();
}
} // namespace ar::core
