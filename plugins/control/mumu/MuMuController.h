#pragma once
// Internal header: "mumu" backend of IController. Touch/key/capture go through
// the emulator's external_renderer_ipc.dll; swipes still run through adb
// (matching the original behaviour).
#include <string>
#include <optional>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "control/AdbClient.hpp"
#include "control/Controller.h"

namespace ar::control {

class MuMuController final : public IController {
  public:
    explicit MuMuController(const ControllerConfig &config);
    ~MuMuController() override;

    std::string_view name() const override { return "mumu"; }

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
    ar::core::Error last_error() const override { return sdk_error_.value_or(adb_.last_error()); }
    bool supports_context() const override { return true; }
    bool start_app(std::string_view package) override {
        sdk_error_.reset();
        return ensure_connected() && adb_.app(serial_, package, true);
    }
    bool stop_app(std::string_view package) override {
        sdk_error_.reset();
        return ensure_connected() && adb_.app(serial_, package, false);
    }

  private:
    using NemuConnectFunc = int (*)(const wchar_t *, int);
    using NemuCaptureFunc = int (*)(int, unsigned int, int, int *, int *, unsigned char *);
    using NemuTouchDownFunc = int (*)(int, int, int, int);
    using NemuTouchUpFunc = int (*)(int, int);
    using NemuDisconnectFunc = void (*)(int);

    bool load_library();
    bool ensure_connected();
    bool fail_sdk(std::string operation, std::string message, int64_t native = 0,
                  ar::core::ErrorCode code = ar::core::ErrorCode::backend_failed) {
        sdk_error_ = ar::core::failure(code, std::move(operation), std::move(message), "mumu", native);
        connected_ = false;
        return false;
    }

    AdbClient adb_;
    std::function<bool()> cancelled_;
    uint32_t timeout_ms_ = 30000;
    std::string serial_;
    std::wstring mumu_path_; // install dir of external_renderer_ipc.dll
    int index_ = 0;
    ar::log::ILogger *logger_ = nullptr;
    std::optional<ar::core::Error> sdk_error_;

    bool loaded_ = false;
    bool connected_ = false;
    int handle_ = 0;
    std::vector<uint8_t> frame_buffer_;

    HINSTANCE dll_ = nullptr;
    NemuConnectFunc nemu_connect_ = nullptr;
    NemuCaptureFunc nemu_capture_display_ = nullptr;
    NemuTouchDownFunc nemu_touch_down_ = nullptr;
    NemuTouchUpFunc nemu_touch_up_ = nullptr;
    NemuDisconnectFunc nemu_disconnect_ = nullptr;
};

} // namespace ar::control
