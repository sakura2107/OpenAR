#pragma once
// Internal header: "adb" backend of IController.
#include <string>

#include "control/AdbClient.hpp"
#include "control/Controller.h"

namespace ar::control {

class AdbController final : public IController {
  public:
    explicit AdbController(const ControllerConfig &config);
    ~AdbController() override = default;

    std::string_view name() const override { return "adb"; }

    bool connect() override;
    bool disconnect() override;
    bool click(cv::Point point) override;
    bool long_press(cv::Point point, int32_t ms) override;
    bool swipe(cv::Point from, cv::Point to, int32_t ms) override;
    bool press_key(int32_t keycode) override;
    cv::Mat screenshot() override;
    void set_context(std::function<bool()> cancelled, uint32_t timeout_ms) override {
        cancelled_ = std::move(cancelled);
        timeout_ms_ = timeout_ms;
        adb_.set_context(cancelled_, timeout_ms);
    }
    ar::core::Error last_error() const override { return adb_.last_error(); }
    bool supports_context() const override { return true; }
    bool start_app(std::string_view package) override {
        return ensure_connected() && adb_.app(serial_, package, true);
    }
    bool stop_app(std::string_view package) override {
        return ensure_connected() && adb_.app(serial_, package, false);
    }

  private:
    bool ensure_connected();

    AdbClient adb_;
    std::function<bool()> cancelled_;
    uint32_t timeout_ms_ = 30000;
    std::string serial_;
    bool connected_ = false;
    ar::log::ILogger *logger_ = nullptr;
};

} // namespace ar::control
