#include "log/ConsoleLogger.h"

#include <cstdio>

namespace ar::log {

void ConsoleLogger::log(Level level, std::string_view message) noexcept {
    if (!enabled(level))
        return;
    const char *color = "\033[0m";
    const char *tag = "";
    switch (level) {
    case Level::debug:
        color = "\033[34m";
        tag = "[DEBUG]";
        break;
    case Level::info:
        color = "\033[32m";
        tag = "[INFO]";
        break;
    case Level::warning:
        color = "\033[33m";
        tag = "[WARNING]";
        break;
    case Level::error:
        color = "\033[31m";
        tag = "[ERROR]";
        break;
    case Level::fatal:
        color = "\033[35m";
        tag = "[FATAL]";
        break;
    }
    std::printf("%s%s\033[0m : %.*s\n", color, tag, static_cast<int>(message.size()), message.data());
    std::fflush(stdout);
}

} // namespace ar::log
