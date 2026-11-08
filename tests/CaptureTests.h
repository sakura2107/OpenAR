#pragma once
#include "apps/cli/Capture.h"
#include <atomic>

namespace capture_tests {
class Controller final : public ar::control::IController {
  public:
    int captures = 0, inputs = 0, connects = 0, disconnects = 0;
    cv::Mat frame{1080, 1920, CV_8UC3, cv::Scalar(17, 91, 203)};
    std::function<void()> on_capture;
    std::string_view name() const override { return "mumu"; }
    bool supports_context() const override { return true; }
    bool connect() override { ++connects; return true; }
    bool disconnect() override { ++disconnects; return true; }
    bool click(cv::Point) override { ++inputs; throw std::runtime_error("Unexpected input"); }
    bool long_press(cv::Point, int32_t) override { ++inputs; throw std::runtime_error("Unexpected input"); }
    bool swipe(cv::Point, cv::Point, int32_t) override { ++inputs; throw std::runtime_error("Unexpected input"); }
    bool press_key(int32_t) override { ++inputs; throw std::runtime_error("Unexpected input"); }
    bool start_app(std::string_view) override { ++inputs; throw std::runtime_error("Unexpected lifecycle"); }
    bool stop_app(std::string_view) override { ++inputs; throw std::runtime_error("Unexpected lifecycle"); }
    cv::Mat screenshot() override { ++captures; if (on_capture) on_capture(); return frame; }
};
} // namespace capture_tests
