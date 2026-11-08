#pragma once
#include <string_view>

namespace ar::log {

enum class Level : int {
    debug = 0,
    info = 1,
    warning = 2,
    error = 3,
    fatal = 4,
};

// Logging interface. Framework modules take a pointer to this interface;
// the concrete sink (console today, file later) lives on the caller side.
class ILogger {
  public:
    virtual ~ILogger() = default;
    virtual void log(Level level, std::string_view message) noexcept = 0;
    // Cheap pre-check so callers can skip std::format work entirely when the
    // level is filtered out: `if (logger->enabled(Level::debug)) ...`.
    virtual bool enabled(Level level) const noexcept = 0;
};

} // namespace ar::log

// AR_LOG(logger, ar::log::Level::info, std::format("loaded {} items", count))
// Pass the message expression (a std::format(...) call or a plain literal);
// it is only evaluated when the logger is set and the level passes the filter.
#define AR_LOG(logger, level, msg)                                                                           \
    do {                                                                                                     \
        auto *ar_log_sink_ = (logger);                                                                       \
        if (ar_log_sink_ && ar_log_sink_->enabled(level))                                                    \
            ar_log_sink_->log(level, msg);                                                                   \
    } while (false)
