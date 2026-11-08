#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

#include <opencv2/opencv.hpp>

#include "core/Registry.h"
#include "log/Logger.h"
#include "core/Image.h"

namespace ar::control {

struct ControllerConfig {
    ar::log::ILogger *logger = nullptr;
    std::filesystem::path adb_path; // defaults to ".\adb.exe" when empty
    uint16_t adb_port = 5555;
    std::filesystem::path mumu_path; // MuMu install directory (holds external_renderer_ipc.dll)
    int mumu_index = 0;
    std::string serial; // USB serial or host:port; empty uses adb_port
    uint32_t command_timeout_ms = 30000;
};

// Input/screenshot interface for one device. Backends ("adb", "mumu", ...)
// register themselves; MuMu uses the emulator's IPC dll for touch/capture and
// adb for swipes, ADB drives everything through adb commands.
class IController {
  public:
    virtual ~IController() = default;

    virtual std::string_view name() const = 0;

    virtual bool connect() = 0;
    virtual bool disconnect() = 0;

    virtual bool click(cv::Point point) = 0;
    virtual bool long_press(cv::Point point, int32_t ms) = 0;
    virtual bool swipe(cv::Point from, cv::Point to, int32_t ms) = 0;
    virtual bool press_key(int32_t keycode) = 0; // Android KEYCODE_* values.

    // Full screen capture in BGR; empty Mat on failure.
    virtual cv::Mat screenshot() = 0;

    // A context belongs to the current operation; sessions serialize its use.
    virtual void set_context(std::function<bool()>, uint32_t) {}
    virtual bool supports_context() const { return false; }
    virtual ar::core::Error last_error() const {
        return ar::core::failure(ar::core::ErrorCode::backend_failed, "controller",
                                 "Controller operation failed", std::string(name()));
    }
    virtual bool start_app(std::string_view) { return false; }
    virtual bool stop_app(std::string_view) { return false; }
    virtual ar::core::Result<cv::Mat> capture_checked() {
        try {
            auto image = screenshot();
            if (image.empty())
                return last_error();
            auto valid = ar::core::validate_image(image);
            if (!valid)
                return valid.error();
            return image;
        } catch (const std::exception &e) {
            return ar::core::failure(ar::core::ErrorCode::backend_failed, "screenshot", e.what(),
                                     std::string(name()));
        }
    }
};

using ControllerRegistry = ar::core::Registry<IController, const ControllerConfig &>;

std::shared_ptr<IController> create_controller(std::string_view backend, const ControllerConfig &config = {});
std::vector<std::string_view> controller_backends();

} // namespace ar::control
