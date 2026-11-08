#pragma once
#include "runtime/Session.h"
namespace ar::runtime {
struct ReplayStep {
    cv::Mat image;
    std::optional<Action> expected;
    uint32_t capture_delay_ms = 0, action_delay_ms = 0;
    int tolerance = 2;
    std::optional<ar::core::Error> capture_error, action_error;
};
class ReplayController final : public ar::control::IController {
  public:
    ReplayController(std::vector<ReplayStep> steps, std::shared_ptr<IClock> clock);
    static ar::core::Result<std::shared_ptr<ReplayController>> load(const std::filesystem::path &file,
                                                                    std::shared_ptr<IClock> clock);
    std::string_view name() const override { return "replay"; }
    bool connect() override;
    bool disconnect() override {
        connected_ = false;
        return true;
    }
    bool click(cv::Point point) override;
    bool long_press(cv::Point point, int32_t ms) override;
    bool swipe(cv::Point from, cv::Point to, int32_t ms) override;
    bool press_key(int32_t key) override;
    bool start_app(std::string_view package) override;
    bool stop_app(std::string_view package) override;
    cv::Mat screenshot() override;
    void set_context(std::function<bool()> cancelled, uint32_t timeout) override {
        cancelled_ = std::move(cancelled);
        timeout_ = timeout;
    }
    bool supports_context() const override { return true; }
    ar::core::Error last_error() const override { return error_; }
    const std::vector<Action> &actions() const { return actions_; }
    bool complete() const { return index_ + 1 == steps_.size() && !steps_[index_].expected; }

  private:
    bool apply(Action action);
    bool delay(uint32_t ms);
    std::vector<ReplayStep> steps_;
    std::vector<Action> actions_;
    std::shared_ptr<IClock> clock_;
    size_t index_ = 0;
    bool connected_ = false;
    std::function<bool()> cancelled_;
    uint32_t timeout_ = 30000;
    ar::core::Error error_ =
        ar::core::failure(ar::core::ErrorCode::backend_failed, "replay", "Replay operation failed");
};
const char *replay_schema();
} // namespace ar::runtime
