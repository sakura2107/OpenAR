#include "loop/Simulator.h"
namespace ar::loop {
Simulator::Simulator(std::map<std::string, SimState> states, std::string initial,
                     std::shared_ptr<ar::runtime::IClock> clock)
    : states_(std::move(states)), state_(std::move(initial)), clock_(std::move(clock)) {
    if (!clock_ || !states_.contains(state_))
        throw std::invalid_argument("Simulator needs clock and initial state");
    for (auto &[name, s] : states_) {
        if (!ar::core::validate_image(s.image))
            throw std::invalid_argument("Invalid simulated image");
        for (auto &e : s.edges)
            if (e.target != "$return" && !states_.contains(e.target))
                throw std::invalid_argument("Missing simulated target state");
        if (s.expires_to.empty() != (s.expires_after_ms == 0) ||
            (!s.expires_to.empty() && !states_.contains(s.expires_to)))
            throw std::invalid_argument("Timed simulated state needs a valid expiry target and duration");
    }
    state(state_);
}
void Simulator::state(std::string value) {
    if (!states_.contains(value))
        throw std::invalid_argument("Unknown simulated state");
    state_ = std::move(value);
    state_expires_ms_ = states_.at(state_).expires_after_ms
        ? clock_->now_ms() + states_.at(state_).expires_after_ms : 0;
}
bool Simulator::delay(uint32_t ms) {
    if (cancelled_ && cancelled_()) {
        error_ = ar::core::failure(ar::core::ErrorCode::cancelled, "simulator", "Cancelled");
        return false;
    }
    if (ms >= timeout_) {
        error_ = ar::core::failure(ar::core::ErrorCode::timeout, "simulator", "Operation timeout");
        return false;
    }
    auto result = clock_->sleep_ms(ms, {});
    return static_cast<bool>(result);
}
cv::Mat Simulator::screenshot() {
    if (!connected_ || !delay(capture_ms))
        return {};
    ++captures;
    if (before_capture)
        before_capture(*this);
    if (faults.hide_ui_on_capture == captures && !faults.idle_state.empty())
        state(faults.idle_state);
    if (state_expires_ms_ && clock_->now_ms() >= state_expires_ms_)
        state(states_.at(state_).expires_to);
    if (!pending_.empty() && !faults.stuck_loading) {
        if (remaining_ == 0 && clock_->now_ms() >= pending_until_ms_) {
            state(pending_);
            pending_.clear();
        } else if (remaining_)
            --remaining_;
    }
    if (seed_ != faults.seed) {
        seed_ = faults.seed;
        rng_.seed(seed_);
    }
    if (resume_.empty() && !faults.popup_state.empty() &&
        (faults.popup_on_capture.erase(captures) ||
         std::generate_canonical<double, 32>(rng_) < faults.popup_probability)) {
        if (!states_.contains(faults.popup_state))
            throw std::invalid_argument("Missing popup state");
        resume_ = state_;
        state(faults.popup_state);
    }
    const auto display =
        !pending_.empty() && !faults.loading_state.empty() && resume_.empty() ? faults.loading_state : state_;
    auto image = states_.at(display).image.clone();
    if (!pending_.empty() && resume_.empty() &&
        clock_->now_ms() >= pending_started_ms_ + faults.transition_delay_ms && faults.black_loading_ms)
        image.setTo(cv::Scalar::all(0));
    if (!pending_.empty() && resume_.empty() && !faults.loading_overlay.empty()) {
        const cv::Rect overlay(faults.overlay_origin, faults.loading_overlay.size());
        if ((overlay & cv::Rect({}, image.size())) != overlay)
            throw std::invalid_argument("Loading overlay outside simulated image");
        faults.loading_overlay.copyTo(image(overlay));
    }
    if (faults.animate)
        image += cv::Scalar::all((captures % 2) ? 20 : -20);
    return image;
}
bool Simulator::click(cv::Point p) {
    return act(p, 0);
}
bool Simulator::press_key(int32_t code) {
    return act({}, code);
}
bool Simulator::act(cv::Point p, int key) {
    if (!connected_ || !delay(action_ms))
        return false;
    ++actions;
    for (auto &e : states_.at(state_).edges)
        if ((key && e.keycode == key) || (!key && !e.keycode && e.click.contains(p))) {
            ++attempted[e.label];
            if (faults.ignored_actions) {
                --faults.ignored_actions;
                return true;
            }
            ++applied[e.label];
            if (e.target == "$return") {
                if (resume_.empty())
                    return false;
                state(resume_);
                resume_.clear();
            } else if (faults.delayed_frames || faults.stuck_loading || faults.transition_delay_ms ||
                       faults.black_loading_ms) {
                if (pending_.empty()) {
                    pending_ = e.target;
                    remaining_ = faults.delayed_frames;
                    pending_started_ms_ = clock_->now_ms();
                    pending_until_ms_ =
                        pending_started_ms_ + faults.transition_delay_ms + faults.black_loading_ms;
                }
            } else
                state(e.target);
            if (after_action)
                after_action(*this);
            if (faults.lost_acknowledgements) {
                --faults.lost_acknowledgements;
                return false;
            }
            return true;
        }
    return false;
}
} // namespace ar::loop
