#pragma once
#include "log/Logger.h"

namespace ar::log {

// Colored console sink (the behaviour of the old printf-based ILogger, now
// behind the interface, with level filtering actually applied).
class ConsoleLogger final : public ILogger {
  public:
    explicit ConsoleLogger(Level min_level = Level::debug) noexcept : min_level_(min_level) {}

    void log(Level level, std::string_view message) noexcept override;
    bool enabled(Level level) const noexcept override { return level >= min_level_; }

  private:
    Level min_level_;
};

} // namespace ar::log
