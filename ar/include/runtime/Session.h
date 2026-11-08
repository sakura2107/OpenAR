#pragma once
#include <atomic>
#include <mutex>
#include <optional>
#include "control/Controller.h"
#include "device/DeviceController.h"
#include "runtime/Clock.h"
namespace ar::runtime {
struct SessionConfig {
    std::string device_id;
    cv::Size reference_size;
    cv::Rect viewport;                    // In the rotated screenshot; empty means whole screen.
    int rotation = 0;                     // Canonical clockwise rotation, 0/90/180/270.
    bool allow_legacy_controller = false; // Explicitly opt out of cooperative cancellation support.
};
struct Frame {
    uint64_t id = 0, generation = 0;
    uint64_t session_id = 0;
    std::string device_id;
    int64_t captured_ms = 0;
    cv::Mat image; // Canonical coordinates, owned pixel data.
    cv::Mat original;
};
class CoordinateMap {
  public:
    static ar::core::Result<CoordinateMap> create(cv::Size original, const SessionConfig &config);
    ar::core::Result<cv::Point> to_device(cv::Point point) const;
    cv::Mat normalize(const cv::Mat &original) const;
    cv::Size reference_size() const { return reference_; }

  private:
    cv::Size original_, reference_;
    cv::Rect viewport_;
    int rotation_ = 0;
};
enum class ActionType { none, click, long_press, swipe, key, wait, start_app, stop_app, reconnect, custom };
struct Action {
    ActionType type = ActionType::none;
    std::optional<cv::Point> target;
    cv::Point to;
    int32_t duration_ms = 0, keycode = 0;
    std::string package, custom;
};
class DeviceSession {
  public:
    DeviceSession(std::shared_ptr<ar::control::IController> controller, SessionConfig config,
                  std::shared_ptr<IClock> clock = std::make_shared<SteadyClock>(),
                  std::unique_ptr<ar::device::IDeviceController> lifecycle = {});
    ~DeviceSession();
    ar::core::Result<std::unique_lock<std::mutex>> acquire_run();
    ar::core::Result<Frame> capture(std::stop_token stop, uint32_t timeout_ms);
    ar::core::Result<void> perform(const Action &action, const Frame *frame, std::optional<cv::Point> matched,
                                   std::stop_token stop, uint32_t timeout_ms);
    ar::core::Result<void> validate(const Frame &frame) const;
    IClock &clock() const { return *clock_; }
    const SessionConfig &config() const { return config_; }
    ar::device::IDeviceController *lifecycle() const { return lifecycle_.get(); }

  private:
    ar::core::Result<void> connect(std::stop_token stop, uint32_t timeout_ms);
    ar::core::Result<void> check(std::stop_token stop, uint32_t timeout_ms) const;
    std::shared_ptr<ar::control::IController> controller_;
    std::unique_ptr<ar::device::IDeviceController> lifecycle_;
    SessionConfig config_;
    std::shared_ptr<IClock> clock_;
    mutable std::mutex operation_mutex_;
    std::shared_ptr<std::mutex> run_mutex_;
    bool connected_ = false;
    uint64_t generation_ = 0, frame_id_ = 0;
    const uint64_t instance_id_;
    std::optional<CoordinateMap> map_;
};
} // namespace ar::runtime
