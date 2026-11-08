#include "runtime/Replay.h"
#include "core/Json.h"
namespace ar::runtime {
using ar::core::ErrorCode;
using ar::core::failure;
const char *replay_schema() {
    return R"json({"type":"object","additionalProperties":false,"required":["schema_version","steps"],"properties":{"schema_version":{"enum":[1]},"steps":{"type":"array","minItems":1,"maxItems":1000,"items":{"type":"object","additionalProperties":false,"required":["image"],"properties":{"image":{"type":"string","minLength":1},"capture_delay_ms":{"type":"integer","minimum":0,"maximum":3600000},"action_delay_ms":{"type":"integer","minimum":0,"maximum":3600000},"tolerance":{"type":"integer","minimum":0,"maximum":100},"capture_error":{"enum":["offline","backend_failed"]},"action_error":{"enum":["offline","backend_failed"]},"expect":{"type":"object","additionalProperties":false,"required":["type"],"properties":{"type":{"enum":["Click","LongPress","Swipe","Key","StartApp","StopApp"]},"target":{"$ref":"#/definitions/point"},"to":{"$ref":"#/definitions/point"},"duration_ms":{"type":"integer","minimum":0,"maximum":3600000},"keycode":{"type":"integer","minimum":0,"maximum":10000},"package":{"type":"string","minLength":1}}}}}}},"definitions":{"point":{"type":"array","minItems":2,"maxItems":2,"items":{"type":"integer","minimum":0,"maximum":8191}}}})json";
}
ReplayController::ReplayController(std::vector<ReplayStep> steps, std::shared_ptr<IClock> clock)
    : steps_(std::move(steps)), clock_(std::move(clock)) {
    if (steps_.empty() || !clock_)
        throw std::invalid_argument("Replay needs steps and clock");
    for (size_t i = 0; i < steps_.size(); ++i) {
        auto valid = ar::core::validate_image(steps_[i].image);
        if (!valid)
            throw std::invalid_argument(valid.error().message);
        steps_[i].image = steps_[i].image.clone();
        if (steps_[i].expected && i + 1 == steps_.size())
            throw std::invalid_argument("Expected action needs a following screenshot");
    }
}
ar::core::Result<std::shared_ptr<ReplayController>> ReplayController::load(const std::filesystem::path &file,
                                                                           std::shared_ptr<IClock> clock) {
    try {
        auto parsed = ar::core::read_json(file, replay_schema());
        if (!parsed)
            return parsed.error();
        const auto root = std::filesystem::canonical(file.parent_path());
        std::vector<ReplayStep> steps;
        size_t total = 0;
        for (const auto &value : parsed.value()["steps"].GetArray()) {
            const auto path =
                std::filesystem::weakly_canonical(root / ar::core::utf8_to_wide(value["image"].GetString()));
            auto r = root.begin(), p = path.begin();
            for (; r != root.end(); ++r, ++p)
                if (p == path.end() || *r != *p)
                    throw std::invalid_argument("Replay image escapes root");
            auto image = ar::core::read_image(path);
            if (!image)
                return image.error();
            total += image.value().total() * image.value().elemSize();
            if (total > 256 * 1024 * 1024)
                throw std::invalid_argument("Replay exceeds image byte budget");
            ReplayStep step;
            step.image = std::move(image.value());
            if (value.HasMember("capture_delay_ms"))
                step.capture_delay_ms = value["capture_delay_ms"].GetUint();
            if (value.HasMember("action_delay_ms"))
                step.action_delay_ms = value["action_delay_ms"].GetUint();
            if (value.HasMember("tolerance"))
                step.tolerance = value["tolerance"].GetInt();
            auto injected = [&](const char *key) -> std::optional<ar::core::Error> {
                if (!value.HasMember(key))
                    return {};
                return failure(std::string_view(value[key].GetString()) == "offline"
                                   ? ErrorCode::offline
                                   : ErrorCode::backend_failed,
                               "replay", std::string("Injected ") + key);
            };
            step.capture_error = injected("capture_error");
            step.action_error = injected("action_error");
            if (value.HasMember("expect")) {
                const auto &e = value["expect"];
                Action a;
                const std::string type = e["type"].GetString();
                a.type = type == "Click"       ? ActionType::click
                         : type == "LongPress" ? ActionType::long_press
                         : type == "Swipe"     ? ActionType::swipe
                         : type == "Key"       ? ActionType::key
                         : type == "StartApp"  ? ActionType::start_app
                                               : ActionType::stop_app;
                if (a.type == ActionType::click || a.type == ActionType::long_press ||
                    a.type == ActionType::swipe) {
                    if (!e.HasMember("target"))
                        throw std::invalid_argument("Expected input needs target");
                    a.target = cv::Point(e["target"][0].GetInt(), e["target"][1].GetInt());
                }
                if (a.type == ActionType::swipe) {
                    if (!e.HasMember("to"))
                        throw std::invalid_argument("Expected swipe needs to");
                    a.to = {e["to"][0].GetInt(), e["to"][1].GetInt()};
                }
                if (e.HasMember("duration_ms"))
                    a.duration_ms = e["duration_ms"].GetInt();
                if (a.type == ActionType::key) {
                    if (!e.HasMember("keycode"))
                        throw std::invalid_argument("Expected key needs keycode");
                    a.keycode = e["keycode"].GetInt();
                }
                if (a.type == ActionType::start_app || a.type == ActionType::stop_app) {
                    if (!e.HasMember("package"))
                        throw std::invalid_argument("Expected app action needs package");
                    a.package = e["package"].GetString();
                }
                step.expected = a;
            }
            steps.push_back(std::move(step));
        }
        return std::make_shared<ReplayController>(std::move(steps), std::move(clock));
    } catch (const std::exception &e) {
        return failure(ErrorCode::resource_invalid, "replay", e.what());
    }
}
bool ReplayController::delay(uint32_t ms) {
    if (cancelled_ && cancelled_()) {
        error_ = failure(ErrorCode::cancelled, "replay", "Cancelled");
        return false;
    }
    uint32_t remaining = std::min(ms, timeout_);
    while (remaining) {
        const auto slice = std::min<uint32_t>(remaining, 5);
        auto waited = clock_->sleep_ms(slice, {});
        if (!waited) {
            error_ = waited.error();
            return false;
        }
        remaining -= slice;
        if (cancelled_ && cancelled_()) {
            error_ = failure(ErrorCode::cancelled, "replay", "Cancelled");
            return false;
        }
    }
    if (cancelled_ && cancelled_()) {
        error_ = failure(ErrorCode::cancelled, "replay", "Cancelled");
        return false;
    }
    if (ms >= timeout_) {
        error_ = failure(ErrorCode::timeout, "replay", "Injected operation exceeded budget");
        return false;
    }
    return true;
}
bool ReplayController::connect() {
    if (!delay(0))
        return false;
    connected_ = true;
    return true;
}
cv::Mat ReplayController::screenshot() {
    if (!connected_) {
        error_ = failure(ErrorCode::offline, "replay", "Replay disconnected");
        return {};
    }
    auto &step = steps_[index_];
    if (!delay(step.capture_delay_ms))
        return {};
    if (step.capture_error) {
        error_ = *step.capture_error;
        step.capture_error.reset();
        return {};
    }
    return step.image.clone();
}
bool ReplayController::apply(Action action) {
    if (!connected_) {
        error_ = failure(ErrorCode::offline, "replay", "Replay disconnected");
        return false;
    }
    auto &step = steps_[index_];
    actions_.push_back(action);
    auto close = [&](cv::Point a, cv::Point b) {
        return std::abs(a.x - b.x) <= step.tolerance && std::abs(a.y - b.y) <= step.tolerance;
    };
    const auto &expected = step.expected;
    if (!expected || action.type != expected->type || action.duration_ms != expected->duration_ms ||
        action.keycode != expected->keycode || action.package != expected->package ||
        (expected->target && (!action.target || !close(*action.target, *expected->target))) ||
        (action.type == ActionType::swipe && !close(action.to, expected->to))) {
        error_ = failure(ErrorCode::backend_failed, "replay",
                         "Unexpected action at step " + std::to_string(index_));
        return false;
    }
    if (!delay(std::max<uint32_t>(action.duration_ms, step.action_delay_ms)))
        return false;
    ++index_; // An uncertain action may have taken effect. Recovery must inspect the new frame.
    if (step.action_error) {
        error_ = *step.action_error;
        step.action_error.reset();
        return false;
    }
    return true;
}
bool ReplayController::click(cv::Point p) {
    Action a;
    a.type = ActionType::click;
    a.target = p;
    return apply(a);
}
bool ReplayController::long_press(cv::Point p, int32_t ms) {
    Action a;
    a.type = ActionType::long_press;
    a.target = p;
    a.duration_ms = ms;
    return apply(a);
}
bool ReplayController::swipe(cv::Point p, cv::Point to, int32_t ms) {
    Action a;
    a.type = ActionType::swipe;
    a.target = p;
    a.to = to;
    a.duration_ms = ms;
    return apply(a);
}
bool ReplayController::press_key(int32_t key) {
    Action a;
    a.type = ActionType::key;
    a.keycode = key;
    return apply(a);
}
bool ReplayController::start_app(std::string_view p) {
    Action a;
    a.type = ActionType::start_app;
    a.package = p;
    return apply(a);
}
bool ReplayController::stop_app(std::string_view p) {
    Action a;
    a.type = ActionType::stop_app;
    a.package = p;
    return apply(a);
}
} // namespace ar::runtime
