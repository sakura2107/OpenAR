#pragma once
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include "runtime/Trace.h"
#include "vision/Matcher.h"

namespace ar::loop {
using namespace std::chrono_literals;
using ar::core::Result;
using ar::runtime::DeviceSession;
using ar::runtime::Frame;

struct Roi {
    std::optional<cv::Rect> bounds;
    std::string full_screen_reason;
    std::map<std::string, cv::Rect> overrides;
    Roi &server(std::string name, cv::Rect value) {
        overrides[std::move(name)] = value;
        return *this;
    }
};
inline Roi roi(int x, int y, int w, int h) {
    return {cv::Rect(x, y, w, h)};
}
inline Roi full_screen(std::string reason) {
    return {{}, std::move(reason)};
}
enum class ConditionKind { image, text, number, any, all, negated, completed, custom };
struct Condition {
    ConditionKind kind = ConditionKind::image;
    std::string value;
    Roi region;
    float threshold = .94f;
    std::vector<Condition> children;
    std::function<bool(const cv::Mat &)> predicate; // pure ROI-only recognizer, replayable.
};
Condition see(std::string asset, Roi region = {}, float threshold = .94f);
Condition gone(std::string asset, Roi region = {}, float threshold = .94f);
Condition ocr_text(std::string text, Roi region = {}, float threshold = .5f);
// Strict numeric grammar (commas, fraction, multiplier, percent, range), no repair.
Condition ocr_number(std::string value, Roi region = {}, float threshold = .5f);
Condition any(std::initializer_list<Condition> children);
Condition all(std::initializer_list<Condition> children);
Condition not_(Condition child);
// Nonvisual, task-local checkpoint; no image is searched. Survives recovery.
Condition completed(std::string phase);
Condition custom(std::string name, Roi region, std::function<bool(const cv::Mat &)> predicate);
template <class... C> Condition any(Condition a, Condition b, C... c) {
    return any({a, b, c...});
}
template <class... C> Condition all(Condition a, Condition b, C... c) {
    return all({a, b, c...});
}

struct Action {
    ar::runtime::Action input;
    bool side_effect = false;
    std::optional<Condition> confirmation;
    std::chrono::milliseconds confirmation_timeout = 5s;
    std::chrono::milliseconds settle_timeout = 2s, unknown_delay = 0ms;
    uint32_t attempt_limit = 0; // zero uses the phase limit (default five).
    // Explicitly repeat a non-effect global acknowledgement while its transient
    // notice stays visible. Ordinary navigation attempt limits stay unchanged.
    std::chrono::milliseconds global_retry_interval = 0ms;
    Action &retry_while_present(std::chrono::milliseconds interval = 2s) {
        global_retry_interval = interval;
        return *this;
    }
    bool strict_navigation_confirmation = false;
    Action &require_confirmation() {
        strict_navigation_confirmation = true;
        return *this;
    }
    std::string budget_key;
    uint32_t task_attempt_limit = 0;
    Action &task_budget(std::string key, uint32_t count) {
        budget_key = std::move(key);
        task_attempt_limit = count;
        return *this;
    }
    std::string fixed_target_reason;
    Action &settle(std::chrono::milliseconds timeout) {
        settle_timeout = timeout;
        return *this;
    }
    Action &after_unknown(std::chrono::milliseconds timeout) {
        unknown_delay = timeout;
        return *this;
    }
    Action &max_attempts(uint32_t count) {
        attempt_limit = count;
        return *this;
    }
    Action &effect() {
        side_effect = true;
        return *this;
    }
    Action &expect(Condition condition, std::chrono::milliseconds timeout = 5s) {
        confirmation = std::move(condition);
        confirmation_timeout = timeout;
        return *this;
    }
};
Action tap();
// Explicit exception for a delayed, confirmed recovery fallback. Startup validation
// requires reason, positive unknown delay, expect(), bounded attempts and a valid point.
Action tap_at(cv::Point point, std::string reason);
Action key(int keycode);
struct Rule {
    std::string name;
    Condition condition;
    Action action;
};
enum class Status { success, already_done, failed, cancelled, outcome_unknown, blocked };
const char *status_name(Status status);
struct Report {
    Status status = Status::failed;
    std::string reason, run_id;
    uint32_t frames = 0, actions = 0, recoveries = 0;
    std::filesystem::path trace_directory;
};
struct ReplayDecision {
    uint64_t frame_id = 0;
    std::string phase, recorded, selected;
    bool equal = false;
};
struct ProjectionReport {
    uint32_t frames = 0, actions = 0, recoveries = 0;
    bool complete = false;
    std::string reason;
};
class Context;
class Phase {
  public:
    explicit Phase(std::string name) : name_(std::move(name)) {}
    Phase &goal(Condition condition) {
        goal_ = std::move(condition);
        return *this;
    }
    Phase &on(Condition condition, Action action);
    Phase &on(std::string name, Condition condition, Action action);
    Phase &timeout(std::chrono::milliseconds value) {
        timeout_ = value;
        return *this;
    }
    Phase &max_frames(uint32_t value) {
        max_frames_ = value;
        return *this;
    }
    Phase &stuck_after(uint32_t value) {
        stuck_after_ = value;
        return *this;
    }
    Phase &unknown_after(std::chrono::milliseconds value) {
        unknown_after_ = value;
        return *this;
    }
    // Opt in only for work whose completion remains valid after anchor recovery.
    // Navigation prerequisites must normally run again after recovery.
    Phase &checkpoint() {
        checkpoint_ = true;
        return *this;
    }
    Report run(Context &, std::stop_token stop = {}) const;
    const std::string &name() const { return name_; }

  private:
    friend class Context;
    friend class Task;
    friend struct Engine;
    std::string name_;
    std::optional<Condition> goal_;
    std::vector<Rule> rules_;
    std::chrono::milliseconds timeout_ = 30s;
    uint32_t max_frames_ = 500, stuck_after_ = 5;
    std::chrono::milliseconds unknown_after_ = 8s;
    bool checkpoint_ = false;
};
struct Options {
    std::filesystem::path trace_root = "out/loop-traces";
    bool full_frames = false; // default thumbnails; exact decision replay needs full_frames.
    std::chrono::milliseconds stable_max = 800ms, poll = 80ms, operation_timeout = 1500ms;
    double stable_difference = .008;
};
class Context {
  public:
    Context(DeviceSession &, std::filesystem::path templates, std::string server, Options options = {});
    ~Context();
    Context(const Context &) = delete;
    Context &global(std::string name, Condition condition, Action action);
    Context &recover(Phase phase);
    Context &guard(std::string name, Condition condition); // stop, no action/recovery.
    Context &busy(Condition condition);                    // suppress all input, pause unknown budget.
    Context &events(std::function<void(const std::string &)> callback);
    Context &ocr(ar::vision::OcrParams params, std::string backend = "paddle-ocr-cpu");
    // Add in-memory template for deterministic offline tests.
    Context &asset(std::string name, const cv::Mat &image);
    DeviceSession &session;

  private:
    friend struct Engine;
    friend class Phase;
    friend class Task;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
class Task {
  public:
    explicit Task(std::string name) : name_(std::move(name)) {}
    Task &done(Condition condition) {
        done_ = std::move(condition);
        return *this;
    }
    Task &phase(Phase phase) {
        phases_.push_back(std::move(phase));
        return *this;
    }
    Task &timeout(std::chrono::milliseconds value) {
        timeout_ = value;
        return *this;
    }
    Task &retries(uint32_t value) {
        retries_ = value;
        return *this;
    }
    Report run(Context &, std::stop_token stop = {}) const;
    // Re-evaluate recorded full-resolution frames, no capture or input calls.
    Result<uint32_t> replay(Context &, const std::filesystem::path &trace,
                            std::function<void(const ReplayDecision &)> observer = {}) const;
    // Counterfactual: use the old images/timestamps, ignore old phase/decisions.
    // Does not claim the future images would be unchanged after different inputs.
    Result<ProjectionReport> project(Context &, const std::filesystem::path &trace,
                                     std::function<void(const std::string &)> observer = {}) const;

  private:
    friend struct Engine;
    std::string name_;
    std::optional<Condition> done_;
    std::vector<Phase> phases_;
    std::chrono::milliseconds timeout_ = 120s;
    uint32_t retries_ = 2;
};
} // namespace ar::loop
