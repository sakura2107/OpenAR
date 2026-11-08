#include "runtime/Tasker.h"
#include "runtime/Trace.h"
#include "vision/Matcher.h"
#include "core/Str.hpp"
#include <regex>
#include <semaphore>
#include <stdexcept>
namespace ar::runtime {
using ar::core::ErrorCode;
using ar::core::failure;
ar::core::Result<void> ExtensionContext::check() const {
    if (stop.stop_requested())
        return failure(ErrorCode::cancelled, "extension", "Task cancelled");
    if (clock.now_ms() >= deadline_ms)
        return failure(ErrorCode::timeout, "extension", "Operation deadline reached");
    return {};
}
uint32_t ExtensionContext::remaining_ms() const {
    return static_cast<uint32_t>(std::clamp<int64_t>(deadline_ms - clock.now_ms(), 0, UINT32_MAX));
}
namespace {
std::atomic<uint64_t> next_run{1};
std::counting_semaphore<2> gpu_slots{2};
struct GpuPermit {
    bool acquired = false;
    ar::core::Result<void> acquire(bool gpu, ExtensionContext &context) {
        if (!gpu)
            return {};
        for (;;) {
            auto checked = context.check();
            if (!checked)
                return checked;
            if (gpu_slots.try_acquire_for(std::chrono::milliseconds(5))) {
                acquired = true;
                return {};
            }
            // Advance virtual clocks too, so a saturated gate cannot defeat a replay deadline.
            auto wait = context.clock.sleep_ms(1, context.stop);
            if (!wait)
                return wait;
        }
    }
    ~GpuPermit() {
        if (acquired)
            gpu_slots.release();
    }
};
class Vision {
  public:
    Vision(const ResourceBundle &resource, const Extensions &extensions)
        : resource_(resource), extensions_(extensions) {}
    ar::core::Result<RecognitionMatch> recognize(const Recognition &rule, ExtensionContext &context) {
        auto checked = context.check();
        if (!checked)
            return checked.error();
        RecognitionMatch result;
        if (rule.type == RecognitionType::direct)
            result.found = true;
        else if (rule.type == RecognitionType::templ) {
            GpuPermit permit;
            auto allowed = permit.acquire(rule.backend.find("cuda") != std::string::npos, context);
            if (!allowed)
                return allowed.error();
            auto &matcher = matchers_[rule.backend];
            if (!matcher) {
                matcher = ar::vision::create_matcher(rule.backend);
                if (!matcher)
                    return failure(ErrorCode::dependency_missing, "match", "Matcher could not be initialized",
                                   rule.backend);
            }
            auto match = matcher->find_checked(context.frame.image, resource_.templates.at(rule.asset),
                                               rule.threshold, rule.roi ? &*rule.roi : nullptr);
            if (!match)
                return match.error();
            result.found = match.value().found;
            result.score = match.value().score;
            result.box = match.value().box;
            result.backend = rule.backend;
        } else if (rule.type == RecognitionType::ocr) {
            GpuPermit permit;
            auto allowed = permit.acquire(resource_.ocr_backend.find("gpu") != std::string::npos, context);
            if (!allowed)
                return allowed.error();
            if (!ocr_) {
                if (!resource_.ocr)
                    return failure(ErrorCode::resource_invalid, "ocr", "OCR model configuration missing");
                ocr_ = ar::vision::create_ocr(resource_.ocr_backend, *resource_.ocr);
                if (!ocr_)
                    return failure(ErrorCode::model_invalid, "ocr", "OCR initialization failed",
                                   resource_.ocr_backend);
            }
            // Cache per frame and ROI; multiple text conditions share one inference.
            const auto roi =
                rule.roi.value_or(cv::Rect(0, 0, context.frame.image.cols, context.frame.image.rows));
            const auto key = std::to_string(context.frame.id) + ":" + std::to_string(roi.x) + ":" +
                             std::to_string(roi.y) + ":" + std::to_string(roi.width) + ":" +
                             std::to_string(roi.height);
            if (ocr_frame_ != context.frame.id) {
                ocr_cache_.clear();
                ocr_frame_ = context.frame.id;
            }
            auto found = ocr_cache_.find(key);
            if (found == ocr_cache_.end()) {
                auto recognized = ocr_->recognize_checked(context.frame.image, &roi);
                if (!recognized)
                    return recognized.error();
                found = ocr_cache_.emplace(key, std::move(recognized.value())).first;
            }
            for (const auto &item : found->second) {
                bool matches = false;
                if (rule.text_mode == TextMode::exact)
                    matches = item.text == rule.text;
                else if (rule.text_mode == TextMode::contains)
                    matches = item.text.find(rule.text) != std::string::npos;
                else {
                    auto it = patterns_.find(rule.text);
                    if (it == patterns_.end())
                        it = patterns_.emplace(rule.text, std::regex(rule.text)).first;
                    matches = std::regex_search(item.text, it->second);
                }
                if (matches && item.score >= rule.threshold) {
                    result = {true, item.score, item.box, item.text, resource_.ocr_backend};
                    break;
                }
            }
        } else if (rule.type == RecognitionType::custom) {
            const auto it = extensions_.recognitions.find(rule.custom);
            if (it == extensions_.recognitions.end())
                return failure(ErrorCode::unsupported, "recognition",
                               "Unknown custom recognizer: " + rule.custom);
            context.params = rule.params;
            try {
                auto recognized = it->second(context);
                if (!recognized)
                    return recognized.error();
                result = recognized.value();
            } catch (const std::exception &e) {
                return failure(ErrorCode::backend_failed, "custom_recognition", e.what(), rule.custom);
            } catch (...) {
                return failure(ErrorCode::backend_failed, "custom_recognition", "Unknown exception",
                               rule.custom);
            }
        } else {
            result.found = rule.type == RecognitionType::all;
            for (const auto &child : rule.conditions) {
                auto match = recognize(child, context);
                if (!match)
                    return match.error();
                if (rule.type == RecognitionType::all && !match.value().found) {
                    result.found = false;
                    break;
                }
                if (match.value().found && result.box.empty()) {
                    result.box = match.value().box;
                    result.score = match.value().score;
                    result.backend = match.value().backend;
                    result.text = match.value().text;
                }
                if (rule.type == RecognitionType::any && match.value().found) {
                    result.found = true;
                    break;
                }
            }
        }
        checked = context.check();
        if (!checked)
            return checked.error();
        if (!std::isfinite(result.score) ||
            (!result.box.empty() && !ar::core::valid_roi(result.box, context.frame.image.size())))
            return failure(ErrorCode::backend_failed, "recognition",
                           "Recognizer returned invalid score or box");
        if (rule.negate) {
            result.found = !result.found;
            result.box = {};
        }
        return result;
    }

  private:
    const ResourceBundle &resource_;
    const Extensions &extensions_;
    std::map<std::string, std::unique_ptr<ar::vision::IImageMatcher>> matchers_;
    std::unique_ptr<ar::vision::IOcrEngine> ocr_;
    uint64_t ocr_frame_ = 0;
    std::map<std::string, std::vector<ar::vision::OcrItem>> ocr_cache_;
    std::map<std::string, std::regex> patterns_;
};
} // namespace
RunReport TaskRunner::run(DeviceSession &session, const ResourceBundle &resource, std::string entry,
                          const Extensions &extensions, std::stop_token stop, const TaskOptions &options,
                          IEventSink *sink) {
    RunReport report;
    const auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    report.run_id = std::to_string(epoch) + "-" + std::to_string(GetCurrentProcessId()) + "-" +
                    std::to_string(next_run.fetch_add(1));
    const auto start = session.clock().now_ms();
    const auto limits = options.limits.value_or(resource.limits);
    const int64_t task_deadline = start + limits.timeout_ms;
    std::unique_ptr<TraceWriter> trace;
    std::optional<Frame> frame;
    std::optional<RecognitionMatch> last_match;
    auto emit = [&](std::string type, const std::string &node, std::optional<ar::core::Error> error = {},
                    std::string details = {}) {
        Event event{report.run_id,
                    session.config().device_id,
                    resource.name,
                    resource.version,
                    node,
                    std::move(type),
                    std::move(details),
                    frame ? frame->id : 0,
                    session.clock().now_ms(),
                    last_match,
                    std::move(error)};
        if (trace)
            trace->emit(event);
        if (sink)
            sink->emit(event);
    };
    auto fail = [&](ar::core::Error error) {
        report.error = std::move(error);
        report.status = report.error->code == ErrorCode::cancelled ? RunStatus::cancelled : RunStatus::failed;
        emit("failed", report.last_node, report.error);
        if (trace && options.save_failure_snapshot && frame) {
            auto snapshot = trace->snapshot(*frame, last_match);
            if (!snapshot)
                report.diagnostic_error = snapshot.error();
        }
        if (trace && trace->error())
            report.diagnostic_error = trace->error();
        report.elapsed_ms = session.clock().now_ms() - start;
        return report;
    };
    try {
        if (!options.trace_directory.empty()) {
            report.trace_directory = options.trace_directory / ar::core::utf8_to_wide(report.run_id);
            trace = std::make_unique<TraceWriter>(report.trace_directory);
            if (trace->error())
                return fail(*trace->error());
        }
        auto lease = session.acquire_run();
        if (!lease)
            return fail(lease.error());
        if (entry.empty())
            entry = resource.entry;
        if (!resource.nodes.contains(entry))
            return fail(failure(ErrorCode::resource_invalid, "task", "Unknown entry: " + entry));
        if (!limits.timeout_ms || !limits.poll_ms || !limits.max_transitions ||
            session.config().reference_size != resource.reference_size)
            return fail(failure(ErrorCode::invalid_argument, "task",
                                "Invalid limits or mismatched session reference dimensions"));
        emit("started", entry);
        Vision vision(resource, extensions);
        std::map<std::string, uint32_t> visits;
        std::vector<std::string> candidates{entry};
        const Node *owner = &resource.nodes.at(entry);
        int64_t stage_deadline = std::min(task_deadline, start + owner->timeout_ms);
        auto recover = [&](const Node &node, const ar::core::Error &error) -> bool {
            if (error.code == ErrorCode::cancelled || session.clock().now_ms() >= task_deadline ||
                node.on_error.empty() || report.recoveries >= limits.max_recoveries)
                return false;
            ++report.recoveries;
            ++report.transitions;
            emit("recovery", node.name, error);
            candidates = node.on_error;
            owner = &node;
            stage_deadline = std::min(task_deadline, session.clock().now_ms() + node.timeout_ms);
            return true;
        };
        for (;;) {
            if (stop.stop_requested())
                return fail(failure(ErrorCode::cancelled, "task", "Task cancelled"));
            if (session.clock().now_ms() >= task_deadline)
                return fail(failure(ErrorCode::timeout, "task", "Task deadline reached"));
            if (report.transitions >= limits.max_transitions)
                return fail(failure(ErrorCode::budget_exceeded, "task", "Transition budget exhausted"));
            if (session.clock().now_ms() >= stage_deadline) {
                auto error =
                    failure(ErrorCode::timeout, "recognition", "No successor matched before deadline");
                if (recover(*owner, error))
                    continue;
                return fail(error);
            }
            const auto remaining = static_cast<uint32_t>(
                std::min<int64_t>(UINT32_MAX, stage_deadline - session.clock().now_ms()));
            auto captured = session.capture(stop, remaining);
            if (!captured) {
                if (recover(*owner, captured.error()))
                    continue;
                return fail(captured.error());
            }
            frame = std::move(captured.value());
            ++report.frames;
            last_match.reset();
            const Node *matched = nullptr;
            bool routed = false;
            for (const auto &name : candidates) {
                const auto &node = resource.nodes.at(name);
                report.last_node = name;
                ExtensionContext context{session, *frame, session.clock(), stop, stage_deadline, {}};
                auto recognized = vision.recognize(node.recognition, context);
                if (!recognized) {
                    if (recover(*owner, recognized.error())) {
                        routed = true;
                        break;
                    }
                    return fail(recognized.error());
                }
                last_match = recognized.value();
                emit("recognized", name);
                if (last_match->found) {
                    matched = &node;
                    break;
                }
            }
            if (routed)
                continue;
            if (!matched) {
                const auto remaining_wait = stage_deadline - session.clock().now_ms();
                if (remaining_wait <= 0)
                    continue;
                auto wait = session.clock().sleep_ms(std::min<int64_t>(limits.poll_ms, remaining_wait), stop);
                if (!wait)
                    return fail(wait.error());
                continue;
            }
            if (++visits[matched->name] > matched->max_visits)
                return fail(failure(ErrorCode::budget_exceeded, "task",
                                    "Node visit budget exhausted: " + matched->name));
            ++report.transitions;
            const auto action_deadline =
                std::min(task_deadline, session.clock().now_ms() + matched->timeout_ms);
            ExtensionContext context{session, *frame,          session.clock(),
                                     stop,    action_deadline, matched->action_params};
            static constexpr const char *action_names[] = {"None",      "Click", "LongPress", "Swipe",
                                                           "Key",       "Wait",  "StartApp",  "StopApp",
                                                           "Reconnect", "Custom"};
            const auto &requested = matched->action;
            std::string action_details = action_names[static_cast<int>(requested.type)];
            auto target = requested.target;
            if (!target && last_match && !last_match->box.empty())
                target = cv::Point(last_match->box.x + last_match->box.width / 2,
                                   last_match->box.y + last_match->box.height / 2);
            if (target)
                action_details +=
                    " target=(" + std::to_string(target->x) + "," + std::to_string(target->y) + ")";
            if (requested.type == ActionType::swipe)
                action_details +=
                    " to=(" + std::to_string(requested.to.x) + "," + std::to_string(requested.to.y) + ")";
            action_details += " duration_ms=" + std::to_string(requested.duration_ms);
            if (requested.type == ActionType::key)
                action_details += " keycode=" + std::to_string(requested.keycode);
            if (!requested.package.empty())
                action_details += " package=" + requested.package;
            if (!requested.custom.empty())
                action_details += " custom=" + requested.custom;
            emit("action_started", matched->name, {}, std::move(action_details));
            ar::core::Result<void> action;
            if (matched->action.type == ActionType::custom) {
                const auto it = extensions.actions.find(matched->action.custom);
                if (it == extensions.actions.end())
                    action = failure(ErrorCode::unsupported, "action",
                                     "Unknown custom action: " + matched->action.custom);
                else
                    try {
                        action = it->second(context);
                    } catch (const std::exception &e) {
                        action = failure(ErrorCode::backend_failed, "custom_action", e.what(),
                                         matched->action.custom);
                    } catch (...) {
                        action = failure(ErrorCode::backend_failed, "custom_action", "Unknown exception",
                                         matched->action.custom);
                    }
            } else {
                std::optional<cv::Point> center;
                if (last_match && !last_match->box.empty())
                    center = cv::Point(last_match->box.x + last_match->box.width / 2,
                                       last_match->box.y + last_match->box.height / 2);
                action = session.perform(matched->action, &*frame, center, stop, context.remaining_ms());
            }
            if (action) {
                auto budget = context.check();
                if (!budget)
                    action = budget;
            }
            if (!action) {
                if (recover(*matched, action.error()))
                    continue;
                return fail(action.error());
            }
            emit("action_completed", matched->name);
            if (matched->next.empty()) {
                report.status = RunStatus::completed;
                report.elapsed_ms = session.clock().now_ms() - start;
                emit("completed", matched->name);
                if (trace && trace->error())
                    report.diagnostic_error = trace->error();
                return report;
            }
            candidates = matched->next;
            owner = matched;
            stage_deadline = std::min(task_deadline, session.clock().now_ms() + matched->timeout_ms);
        }
    } catch (const std::exception &e) {
        return fail(failure(ErrorCode::backend_failed, "task", e.what()));
    } catch (...) {
        return fail(failure(ErrorCode::backend_failed, "task", "Unknown extension exception"));
    }
}
Tasker::Tasker(std::shared_ptr<DeviceSession> session, std::shared_ptr<const ResourceBundle> resource,
               Extensions extensions)
    : session_(std::move(session)), resource_(std::move(resource)), extensions_(std::move(extensions)) {
    if (!session_ || !resource_)
        throw std::invalid_argument("Tasker needs session and resource snapshot");
}
std::shared_ptr<TaskJob> Tasker::post_task(std::string entry, TaskOptions options,
                                           std::shared_ptr<IEventSink> sink) const {
    auto job = std::make_shared<TaskJob>();
    auto promise = std::make_shared<std::promise<RunReport>>();
    job->result_ = promise->get_future().share();
    job->worker_ = std::jthread([session = session_, resource = resource_, extensions = extensions_,
                                 entry = std::move(entry), options = std::move(options),
                                 sink = std::move(sink), promise](std::stop_token stop) {
        promise->set_value(
            TaskRunner::run(*session, *resource, entry, extensions, stop, options, sink.get()));
    });
    return job;
}
} // namespace ar::runtime
