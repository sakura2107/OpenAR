#pragma once
#include "runtime/Session.h"
#include <map>
#include <random>
#include <set>
namespace ar::loop {
struct Transition {
    cv::Rect click;
    int keycode = 0;
    std::string target, label;
};
struct SimState {
    cv::Mat image;
    std::vector<Transition> edges;
    std::string expires_to;
    uint32_t expires_after_ms = 0;
};
struct Faults {
    uint32_t delayed_frames = 0, ignored_actions = 0, lost_acknowledgements = 0;
    std::string loading_state; // displayed while a transition is delayed.
    bool stuck_loading = false, animate = false;
    uint32_t transition_delay_ms = 0, black_loading_ms = 0;
    cv::Mat loading_overlay;
    cv::Point overlay_origin;
    std::string idle_state;
    uint64_t hide_ui_on_capture = 0;
    std::string popup_state;
    double popup_probability = 0;
    uint32_t seed = 1;
    std::set<uint64_t> popup_on_capture;
};
// Offline only. Input/capture still run through the same DeviceSession contract.
class Simulator final : public ar::control::IController {
  public:
    Simulator(std::map<std::string, SimState> states, std::string initial,
              std::shared_ptr<ar::runtime::IClock> clock);
    std::string_view name() const override { return "loop-simulator"; }
    bool connect() override {
        connected_ = true;
        return true;
    }
    bool disconnect() override {
        connected_ = false;
        return true;
    }
    cv::Mat screenshot() override;
    bool click(cv::Point p) override;
    bool press_key(int32_t code) override;
    bool long_press(cv::Point, int32_t) override { return false; }
    bool swipe(cv::Point, cv::Point, int32_t) override { return false; }
    bool supports_context() const override { return true; }
    void set_context(std::function<bool()> cancelled, uint32_t timeout) override {
        cancelled_ = std::move(cancelled);
        timeout_ = timeout;
    }
    ar::core::Error last_error() const override { return error_; }
    Faults faults;
    uint32_t capture_ms = 2, action_ms = 1;
    uint64_t captures = 0, actions = 0;
    std::map<std::string, uint32_t> applied, attempted;
    std::function<void(Simulator &)> before_capture, after_action;
    const std::string &state() const { return state_; }
    void state(std::string value);

  private:
    bool act(cv::Point point, int key);
    bool delay(uint32_t ms);
    std::map<std::string, SimState> states_;
    std::string state_, resume_, pending_;
    std::shared_ptr<ar::runtime::IClock> clock_;
    std::mt19937 rng_{1};
    uint32_t remaining_ = 0, seed_ = 1;
    int64_t pending_started_ms_ = 0, pending_until_ms_ = 0;
    int64_t state_expires_ms_ = 0;
    bool connected_ = false;
    std::function<bool()> cancelled_;
    uint32_t timeout_ = 1000;
    ar::core::Error error_ =
        ar::core::failure(ar::core::ErrorCode::backend_failed, "simulator", "Injected/unexpected operation");
};
} // namespace ar::loop
