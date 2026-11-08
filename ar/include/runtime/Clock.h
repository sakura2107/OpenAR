#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include "core/Result.h"
namespace ar::runtime {
class IClock {
  public:
    virtual ~IClock() = default;
    virtual int64_t now_ms() const = 0;
    virtual ar::core::Result<void> sleep_ms(int64_t ms, std::stop_token stop) = 0;
};
class SteadyClock final : public IClock {
  public:
    int64_t now_ms() const override {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
    ar::core::Result<void> sleep_ms(int64_t ms, std::stop_token stop) override {
        if (ms < 0)
            return ar::core::failure(ar::core::ErrorCode::invalid_argument, "sleep", "Negative duration");
        std::mutex mutex;
        std::condition_variable_any cv;
        std::unique_lock lock(mutex);
        cv.wait_for(lock, stop, std::chrono::milliseconds(ms), [] { return false; });
        if (stop.stop_requested())
            return ar::core::failure(ar::core::ErrorCode::cancelled, "sleep", "Task cancelled");
        return {};
    }
};
class ManualClock final : public IClock {
  public:
    int64_t now_ms() const override { return value_.load(); }
    ar::core::Result<void> sleep_ms(int64_t ms, std::stop_token stop) override {
        if (stop.stop_requested())
            return ar::core::failure(ar::core::ErrorCode::cancelled, "sleep", "Task cancelled");
        if (ms < 0)
            return ar::core::failure(ar::core::ErrorCode::invalid_argument, "sleep", "Negative duration");
        value_.fetch_add(ms);
        return {};
    }

  private:
    std::atomic<int64_t> value_{0};
};
} // namespace ar::runtime
