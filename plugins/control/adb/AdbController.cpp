#include "AdbController.h"

#include <format>

namespace ar::control {

namespace {
// adb prints these markers on stdout when something goes wrong.
bool output_indicates_failure(const std::string &output) {
    return output.find("not found") != std::string::npos || output.find("error") != std::string::npos;
}
} // namespace

AdbController::AdbController(const ControllerConfig &config)
    : adb_(config.adb_path, config.logger),
      serial_(config.serial.empty() ? std::format("127.0.0.1:{}", config.adb_port) : config.serial),
      logger_(config.logger) {
    set_context({}, config.command_timeout_ms);
}

bool AdbController::connect() {
    std::string output;
    if (!adb_.connect(serial_, output))
        return false;
    if (output.find("(10016)") != std::string::npos || output.find("error") != std::string::npos) {
        AR_LOG(logger_, ar::log::Level::error,
               std::format("Cannot connect {}. Please check this host !", serial_));
        return false;
    }
    connected_ = true;
    return true;
}

bool AdbController::disconnect() {
    // ADB transports are shared by other sessions/tools; release local state.
    connected_ = false;
    return true;
}

bool AdbController::ensure_connected() {
    return connected_ || connect();
}

bool AdbController::click(cv::Point point) {
    if (!ensure_connected())
        return false;
    std::string output;
    return adb_.run(
               std::format("-s {} shell input tap {} {}", ar::core::quote_arg(serial_), point.x, point.y),
               output) &&
           !output_indicates_failure(output);
}

bool AdbController::long_press(cv::Point point, int32_t ms) {
    if (!ensure_connected())
        return false;
    std::string output;
    return adb_.run(std::format("-s {} shell input swipe {} {} {} {} {}", ar::core::quote_arg(serial_),
                                point.x, point.y, point.x, point.y, ms),
                    output) &&
           !output_indicates_failure(output);
}

bool AdbController::swipe(cv::Point from, cv::Point to, int32_t ms) {
    if (!ensure_connected())
        return false;
    std::string output;
    return adb_.run(std::format("-s {} shell input swipe {} {} {} {} {}", ar::core::quote_arg(serial_),
                                from.x, from.y, to.x, to.y, ms),
                    output) &&
           !output_indicates_failure(output);
}

bool AdbController::press_key(int32_t keycode) {
    if (!ensure_connected())
        return false;
    std::string output;
    return adb_.run(std::format("-s {} shell input keyevent {}", ar::core::quote_arg(serial_), keycode),
                    output) &&
           !output_indicates_failure(output);
}

cv::Mat AdbController::screenshot() {
    if (!ensure_connected())
        return {};
    cv::Mat image;
    if (!adb_.screencap(serial_, image)) {
        connected_ = false;
        return {};
    }
    return image;
}

} // namespace ar::control
