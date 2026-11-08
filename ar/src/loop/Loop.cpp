#include "loop/Loop.h"
#include "core/Json.h"
#include "core/IO.h"
#include <atomic>
#include <cctype>
#include <cwctype>
#include <regex>
#include <fstream>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace ar::loop {
using ar::core::ErrorCode;
using ar::core::failure;
using Json = rapidjson::Document;
using Value = rapidjson::Value;
using Alloc = rapidjson::Document::AllocatorType;
namespace {
std::string json(const Value &v) {
    rapidjson::StringBuffer b;
    rapidjson::Writer<rapidjson::StringBuffer> w(b);
    v.Accept(w);
    return {b.GetString(), b.GetSize()};
}
Value text(std::string_view s, Alloc &a) {
    return Value(s.data(), static_cast<rapidjson::SizeType>(s.size()), a);
}
void put(Value &v, const char *k, std::string_view s, Alloc &a) {
    v.AddMember(Value(k, a), text(s, a), a);
}
void put_count(Value &v, const std::string &key, uint32_t n, Alloc &a) {
    auto k = text(key, a);
    v.AddMember(k, n, a);
}
double wall() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
int64_t utc() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
cv::Rect rect(const Roi &r, const std::string &server, cv::Size size) {
    auto it = r.overrides.find(server);
    if (it != r.overrides.end())
        return it->second;
    return r.bounds.value_or(cv::Rect(0, 0, size.width, size.height));
}
std::string number(std::string s) {
    auto wide = ar::core::utf8_to_wide(s);
    s.clear();
    for (auto c : wide) {
        if (std::iswspace(c))
            continue;
        if (c >= 0xff01 && c <= 0xff5e)
            c -= 0xfee0;
        if (c == 0xd7 || c == L'X')
            c = L'x';
        if (c > 127)
            return {};
        s.push_back(static_cast<char>(c));
    }
    const std::string n = "(?:[0-9]{1,3}(?:,[0-9]{3})+|[0-9]+)(?:\\.[0-9]+)?";
    static const std::vector<std::pair<std::regex, std::string>> patterns = {
        {std::regex("(" + n + ")/(" + n + ")"), "fraction"},
        {std::regex("x(" + n + ")"), "multiplier"},
        {std::regex("([+-]?" + n + ")%"), "percent"},
        {std::regex("(" + n + ")-(" + n + ")"), "range"},
        {std::regex("([+-]?" + n + ")"), "number"}};
    for (const auto &[p, type] : patterns) {
        std::smatch m;
        if (!std::regex_match(s, m, p))
            continue;
        std::string result = type;
        for (size_t i = 1; i < m.size(); ++i) {
            auto v = m[i].str();
            v.erase(std::remove(v.begin(), v.end(), ','), v.end());
            bool negative = v.front() == '-';
            if (v.front() == '+' || negative)
                v.erase(0, 1);
            auto dot = v.find('.');
            auto integer = v.substr(0, dot), decimal = dot == std::string::npos ? "" : v.substr(dot + 1);
            auto first = integer.find_first_not_of('0');
            integer = first == std::string::npos ? "0" : integer.substr(first);
            while (!decimal.empty() && decimal.back() == '0')
                decimal.pop_back();
            result += ':';
            if (negative && (integer != "0" || !decimal.empty()))
                result += '-';
            result += integer;
            if (!decimal.empty())
                result += '.' + decimal;
        }
        return result;
    }
    return {};
}
struct Observation {
    bool matched = false;
    float score = 0;
    cv::Rect box;
    std::string value;
    std::vector<ar::vision::OcrItem> raw_ocr;
};
Value observation(const Observation &o, Alloc &a) {
    Value v(rapidjson::kObjectType);
    v.AddMember("matched", o.matched, a);
    v.AddMember("score", o.score, a);
    put(v, "text", o.value, a);
    Value box(rapidjson::kArrayType);
    for (int x : {o.box.x, o.box.y, o.box.width, o.box.height})
        box.PushBack(x, a);
    v.AddMember("box", box, a);
    if (!o.raw_ocr.empty()) {
        Value raw(rapidjson::kArrayType);
        for (auto &item : o.raw_ocr) {
            Value entry(rapidjson::kObjectType);
            put(entry, "text", item.text, a);
            entry.AddMember("score", item.score, a);
            Value r(rapidjson::kArrayType);
            for (int n : {item.box.x, item.box.y, item.box.width, item.box.height})
                r.PushBack(n, a);
            entry.AddMember("box", r, a);
            raw.PushBack(entry, a);
        }
        v.AddMember("raw_ocr", raw, a);
    }
    return v;
}
cv::Mat sample(const cv::Mat &im, const std::vector<cv::Rect> &regions) {
    cv::Mat out(static_cast<int>(std::max<size_t>(1, regions.size())) * 16, 32, CV_8UC1);
    if (regions.empty()) {
        cv::Mat gray;
        cv::cvtColor(im, gray, cv::COLOR_BGR2GRAY);
        cv::resize(gray, out, out.size());
        return out;
    }
    for (size_t i = 0; i < regions.size(); ++i) {
        cv::Mat gray;
        cv::cvtColor(im(regions[i]), gray, cv::COLOR_BGR2GRAY);
        cv::resize(gray, out(cv::Rect(0, static_cast<int>(i) * 16, 32, 16)), {32, 16});
    }
    return out;
}
double difference(const cv::Mat &a, const cv::Mat &b) {
    if (a.empty() || a.size() != b.size())
        return 1;
    return cv::norm(a, b, cv::NORM_L1) / (255.0 * a.total());
}
} // namespace
Condition see(std::string a, Roi r, float t) {
    return {ConditionKind::image, std::move(a), std::move(r), t};
}
Condition gone(std::string a, Roi r, float t) {
    return {ConditionKind::negated, "", {}, t, {see(std::move(a), std::move(r), t)}};
}
Condition ocr_text(std::string t, Roi r, float c) {
    return {ConditionKind::text, std::move(t), std::move(r), c};
}
Condition ocr_number(std::string t, Roi r, float c) {
    return {ConditionKind::number, std::move(t), std::move(r), c};
}
Condition any(std::initializer_list<Condition> c) {
    return {ConditionKind::any, "", {}, 0, c};
}
Condition all(std::initializer_list<Condition> c) {
    return {ConditionKind::all, "", {}, 0, c};
}
Condition not_(Condition c) {
    return {ConditionKind::negated, "", {}, 0, {std::move(c)}};
}
Condition completed(std::string name) {
    return {ConditionKind::completed, std::move(name)};
}
Condition custom(std::string name, Roi r, std::function<bool(const cv::Mat &)> predicate) {
    Condition c;
    c.kind = ConditionKind::custom;
    c.value = std::move(name);
    c.region = std::move(r);
    c.predicate = std::move(predicate);
    return c;
}
Action tap() {
    Action a;
    a.input.type = ar::runtime::ActionType::click;
    return a;
}
Action tap_at(cv::Point point, std::string reason) {
    auto a = tap();
    a.input.target = point;
    a.fixed_target_reason = std::move(reason);
    return a;
}
Action key(int code) {
    Action a;
    a.input.type = ar::runtime::ActionType::key;
    a.input.keycode = code;
    return a;
}
const char *status_name(Status s) {
    switch (s) {
    case Status::success:
        return "success";
    case Status::already_done:
        return "already_done";
    case Status::cancelled:
        return "cancelled";
    case Status::outcome_unknown:
        return "outcome_unknown";
    case Status::blocked:
        return "blocked";
    default:
        return "failed";
    }
}
Phase &Phase::on(Condition c, Action a) {
    return on("rule-" + std::to_string(rules_.size() + 1), std::move(c), std::move(a));
}
Phase &Phase::on(std::string name, Condition c, Action a) {
    rules_.push_back({std::move(name), std::move(c), std::move(a)});
    return *this;
}
struct Context::Impl {
    std::filesystem::path root;
    std::string server, run_id;
    Options options;
    std::vector<Rule> globals;
    std::vector<Condition> busy;
    std::vector<std::pair<std::string, Condition>> guards;
    std::optional<Phase> recovery;
    std::function<void(const std::string &)> callback;
    std::map<std::string, cv::Mat> assets;
    std::unique_ptr<ar::vision::IImageMatcher> matcher;
    std::optional<ar::vision::OcrParams> ocr_params;
    std::string ocr_backend;
    std::unique_ptr<ar::vision::IOcrEngine> ocr;
    uint64_t ocr_frame = 0;
    std::map<std::string, std::vector<ar::vision::OcrItem>> ocr_cache;
    std::unique_ptr<ar::runtime::TraceWriter> trace;
    std::optional<Frame> last;
    std::optional<ar::core::Error> event_error;
    std::map<std::string, std::string> effects; // attempted/confirmed/unknown; task lifetime.
    std::map<std::string, uint32_t> task_attempts;
    std::map<std::string, int64_t> global_retry_after;
    std::set<std::string> milestones;
    Report report;
};
Context::Context(DeviceSession &s, std::filesystem::path root, std::string server, Options o)
    : session(s), impl_(std::make_unique<Impl>()) {
    if (server.empty() || server.find_first_of("/\\.") != std::string::npos)
        throw std::invalid_argument("Invalid server name");
    if (o.poll.count() < 1 || o.stable_max.count() < 0 || o.operation_timeout.count() < 1 ||
        !std::isfinite(o.stable_difference) || o.stable_difference < 0 || o.stable_difference > 1)
        throw std::invalid_argument("Invalid loop options");
    impl_->root = std::filesystem::absolute(root);
    impl_->server = std::move(server);
    impl_->options = o;
}
Context::~Context() = default;
Context &Context::global(std::string n, Condition c, Action a) {
    if (a.side_effect)
        throw std::invalid_argument("Global rules cannot have side effects");
    impl_->globals.push_back({std::move(n), std::move(c), std::move(a)});
    return *this;
}
Context &Context::recover(Phase p) {
    impl_->recovery = std::move(p);
    return *this;
}
Context &Context::guard(std::string n, Condition c) {
    impl_->guards.emplace_back(std::move(n), std::move(c));
    return *this;
}
Context &Context::busy(Condition c) {
    impl_->busy.push_back(std::move(c));
    return *this;
}
Context &Context::events(std::function<void(const std::string &)> f) {
    impl_->callback = std::move(f);
    return *this;
}
Context &Context::ocr(ar::vision::OcrParams p, std::string backend) {
    impl_->ocr_params = std::move(p);
    impl_->ocr_backend = std::move(backend);
    return *this;
}
Context &Context::asset(std::string n, const cv::Mat &im) {
    if (im.empty())
        throw std::invalid_argument("Empty template");
    impl_->assets[std::move(n)] = im.clone();
    return *this;
}

struct Engine {
    struct PhaseState {
        const Rule *effect = nullptr, *settling = nullptr;
        int64_t effect_end = 0, settle_until = -1, last_time = -1, unknown_ms = 0, unknown_limit_ms = 0;
        bool busy = false, was_unknown = false;
        std::string settle_reason, last_rule;
        std::map<std::string, uint32_t> attempts;
    };
    PhaseState *active_state = nullptr;
    bool visual_known = false;
    Context &ctx;
    Context::Impl &i;
    std::stop_token stop;
    int64_t deadline;
    int64_t decision_ms = -1, action_started_ms = -1;
    const Rule *action_rule = nullptr;
    std::optional<cv::Point> action_target;
    std::optional<ar::core::Error> action_error;
    Engine(Context &c, std::stop_token s, int64_t d) : ctx(c), i(*c.impl_), stop(s), deadline(d) {}
    uint32_t budget() const {
        return static_cast<uint32_t>(
            std::max<int64_t>(0, std::min<int64_t>(i.options.operation_timeout.count(),
                                                   deadline - ctx.session.clock().now_ms())));
    }
    void check() {
        if (stop.stop_requested())
            throw failure(ErrorCode::cancelled, "loop", "Cancelled");
        if (!budget())
            throw failure(ErrorCode::timeout, "loop", "Task/phase deadline exceeded");
        if (i.event_error)
            throw *i.event_error;
        if (i.trace && i.trace->error())
            throw *i.trace->error();
    }
    cv::Mat &asset(const std::string &name) {
        auto found = i.assets.find(name);
        if (found != i.assets.end())
            return found->second;
        const auto relative = std::filesystem::path(ar::core::utf8_to_wide(name));
        if (relative.is_absolute() || name.find("..") != std::string::npos ||
            name.find('\\') != std::string::npos)
            throw std::invalid_argument("Invalid template name");
        auto path = i.root / ar::core::utf8_to_wide(i.server) / relative;
        path += L".png";
        auto im = ar::core::read_image(path);
        if (!im)
            throw std::invalid_argument("Missing template: " + name);
        return i.assets.emplace(name, std::move(im.value())).first->second;
    }
    void validate(const Condition &c) {
        if (c.kind == ConditionKind::completed) {
            if (c.value.empty())
                throw std::invalid_argument("Empty phase checkpoint");
            return;
        }
        if (c.kind == ConditionKind::all || c.kind == ConditionKind::any ||
            c.kind == ConditionKind::negated) {
            if (c.children.empty())
                throw std::invalid_argument("Empty condition group");
            for (auto &child : c.children)
                validate(child);
            return;
        }
        if (!c.region.bounds && c.region.full_screen_reason.empty())
            throw std::invalid_argument(
                "Every visual condition requires ROI or an explicit full-screen reason");
        const auto r = rect(c.region, i.server, ctx.session.config().reference_size);
        if (!ar::core::valid_roi(r, ctx.session.config().reference_size))
            throw std::invalid_argument("Condition ROI outside reference image");
        if (r == cv::Rect(0, 0, ctx.session.config().reference_size.width,
                          ctx.session.config().reference_size.height) &&
            c.region.full_screen_reason.empty())
            throw std::invalid_argument("Full-screen condition needs an explicit reason");
        for (auto &[server, r] : c.region.overrides)
            if (!ar::core::valid_roi(r, ctx.session.config().reference_size))
                throw std::invalid_argument("ROI override outside reference image");
        if (!std::isfinite(c.threshold) || c.threshold < 0 || c.threshold > 1)
            throw std::invalid_argument("Invalid recognition threshold");
        if (c.value.empty())
            throw std::invalid_argument("Condition value is empty");
        if (c.kind == ConditionKind::image) {
            auto &im = asset(c.value);
            if (im.cols > r.width || im.rows > r.height)
                throw std::invalid_argument("Template larger than ROI");
        } else if (c.kind == ConditionKind::custom) {
            if (!c.predicate)
                throw std::invalid_argument("Custom ROI condition needs pure predicate");
        } else if (!i.ocr_params)
            throw std::invalid_argument("OCR condition needs configured OCR");
        if (c.kind == ConditionKind::number && number(c.value).empty())
            throw std::invalid_argument("Invalid numeric expectation");
    }
    bool targetable(const Condition &c) {
        if (c.kind == ConditionKind::image || c.kind == ConditionKind::text ||
            c.kind == ConditionKind::number)
            return true;
        if (c.kind == ConditionKind::all)
            return std::any_of(c.children.begin(), c.children.end(),
                               [&](const Condition &v) { return targetable(v); });
        if (c.kind == ConditionKind::any)
            return !c.children.empty() && std::all_of(c.children.begin(), c.children.end(),
                                                      [&](const Condition &v) { return targetable(v); });
        return false;
    }
    void validate(const Rule &r, bool global = false) {
        if (r.name.empty())
            throw std::invalid_argument("Rule needs name");
        validate(r.condition);
        std::vector<cv::Rect> areas;
        regions(r.condition, areas);
        if (areas.empty())
            throw std::invalid_argument("Action rule requires a visual ROI");
        if (global && r.action.side_effect)
            throw std::invalid_argument("Global side effect rejected");
        if (r.action.settle_timeout.count() < 0 || r.action.settle_timeout.count() > 3600000 ||
            r.action.unknown_delay.count() < 0 || r.action.unknown_delay.count() > 3600000 ||
            r.action.attempt_limit > 5)
            throw std::invalid_argument("Invalid navigation wait/attempt limits");
        if (r.action.global_retry_interval.count() != 0 &&
            (!global || r.action.side_effect || r.action.input.target ||
             r.action.global_retry_interval.count() < 500 || r.action.global_retry_interval.count() > 30000))
            throw std::invalid_argument(
                "Repeated global acknowledgement needs a matched non-effect rule and 500..30000ms interval");
        if (r.action.strict_navigation_confirmation && (global || !r.action.confirmation))
            throw std::invalid_argument("Strict navigation confirmation needs a phase-local expect");
        if (r.action.budget_key.empty() != (r.action.task_attempt_limit == 0) ||
            r.action.task_attempt_limit > 100)
            throw std::invalid_argument("Task action budget needs a name and a finite positive limit");
        if (r.action.input.target) {
            auto point = *r.action.input.target;
            if (global || r.action.fixed_target_reason.empty() || r.action.unknown_delay.count() <= 0 ||
                !r.action.confirmation || !r.action.attempt_limit || r.action.side_effect ||
                !cv::Rect({}, ctx.session.config().reference_size).contains(point))
                throw std::invalid_argument(
                    "Fixed target requires a delayed, confirmed, bounded recovery fallback");
        }
        if (r.action.input.type != ar::runtime::ActionType::click &&
            r.action.input.type != ar::runtime::ActionType::key)
            throw std::invalid_argument("Loop action must be tap or key");
        if (r.action.input.type == ar::runtime::ActionType::click && !r.action.input.target &&
            !targetable(r.condition))
            throw std::invalid_argument("Tap needs a positive template/OCR target in every matching branch");
        if (r.action.side_effect && !r.action.confirmation)
            throw std::invalid_argument("Side effect requires expect(condition, timeout)");
        if (r.action.confirmation) {
            if (r.action.confirmation_timeout.count() < 1 || r.action.confirmation_timeout.count() > 3600000)
                throw std::invalid_argument("Invalid confirmation timeout");
            validate(*r.action.confirmation);
        }
    }
    void validate(const Phase &p) {
        if (p.name_.empty() || !p.goal_ || p.timeout_.count() < 1 || p.timeout_.count() > 3600000 ||
            !p.max_frames_ || p.max_frames_ > 1000000 || !p.stuck_after_ || p.stuck_after_ > 5 ||
            p.unknown_after_.count() < 1)
            throw std::invalid_argument("Phase needs goal and finite limits");
        validate(*p.goal_);
        std::set<std::string> names;
        for (auto &r : p.rules_) {
            validate(r);
            if (!names.insert(r.name).second)
                throw std::invalid_argument("Duplicate rule name");
        }
    }
    Observation eval(const Condition &c, const Frame &frame, bool require_current = true) {
        check();
        if (require_current) {
            auto valid = ctx.session.validate(frame);
            if (!valid)
                throw valid.error();
        }
        if (c.kind == ConditionKind::completed)
            return {i.milestones.contains(c.value), 1};
        if (c.kind == ConditionKind::any || c.kind == ConditionKind::all ||
            c.kind == ConditionKind::negated) {
            Observation result;
            result.matched = c.kind == ConditionKind::all;
            for (auto &child : c.children) {
                auto value = eval(child, frame, require_current);
                result.raw_ocr.insert(result.raw_ocr.end(), value.raw_ocr.begin(), value.raw_ocr.end());
                if (c.kind == ConditionKind::negated) {
                    value.matched = !value.matched;
                    value.box = {};
                    return value;
                }
                if (value.matched && result.box.empty()) {
                    result.box = value.box;
                    result.score = value.score;
                    result.value = value.value;
                }
                if (c.kind == ConditionKind::all && !value.matched) {
                    result.matched = false;
                    return result;
                }
                if (c.kind == ConditionKind::any && value.matched) {
                    result.matched = true;
                    return result;
                }
            }
            return result;
        }
        auto r = rect(c.region, i.server, frame.image.size());
        if (c.kind == ConditionKind::custom)
            return {c.predicate(frame.image(r)), 1};
        if (c.kind == ConditionKind::image) {
            if (!i.matcher)
                i.matcher = ar::vision::create_matcher("ncc-cpu");
            if (!i.matcher)
                throw failure(ErrorCode::dependency_missing, "loop.match", "ncc-cpu unavailable");
            auto found = i.matcher->find_checked(frame.image, asset(c.value), c.threshold, &r);
            if (!found)
                throw found.error();
            visual_known = visual_known || found.value().found;
            return {found.value().found, found.value().score, found.value().box};
        }
        if (!i.ocr)
            i.ocr = ar::vision::create_ocr(i.ocr_backend, *i.ocr_params);
        if (!i.ocr)
            throw failure(ErrorCode::model_invalid, "loop.ocr", "OCR initialization failed");
        if (i.ocr_frame != frame.id) {
            i.ocr_cache.clear();
            i.ocr_frame = frame.id;
        }
        auto key = std::to_string(r.x) + ":" + std::to_string(r.y) + ":" + std::to_string(r.width) + ":" +
                   std::to_string(r.height) + ":" + std::to_string(static_cast<int>(c.kind));
        auto found = i.ocr_cache.find(key);
        if (found == i.ocr_cache.end()) {
            auto recognized = i.ocr->recognize_crop_checked(frame.image, &r, c.kind == ConditionKind::number);
            if (!recognized)
                throw recognized.error();
            found = i.ocr_cache.emplace(key, std::move(recognized.value())).first;
        }
        for (auto &item : found->second)
            if (item.score >= c.threshold &&
                (c.kind == ConditionKind::number
                     ? (!number(item.text).empty() && number(item.text) == number(c.value))
                     : item.text == c.value)) {
                visual_known = true;
                return {true, item.score, item.box, item.text, found->second};
            }
        Observation unmatched;
        unmatched.raw_ocr = found->second;
        for (auto &item : found->second)
            if (item.score > unmatched.score) {
                unmatched.score = item.score;
                unmatched.value = item.text;
                unmatched.box = item.box;
            }
        return unmatched;
    }
    void regions(const Condition &c, std::vector<cv::Rect> &r) {
        if (c.kind == ConditionKind::completed)
            return;
        if (!c.children.empty()) {
            for (auto &child : c.children)
                regions(child, r);
        } else
            r.push_back(rect(c.region, i.server, ctx.session.config().reference_size));
    }
    void event(std::string type, std::string phase, Json data, uint64_t frame = 0) {
        auto &a = data.GetAllocator();
        data.AddMember("schema_version", 1, a);
        put(data, "event", type, a);
        ar::runtime::Event e;
        e.run_id = i.run_id;
        e.device_id = ctx.session.config().device_id;
        e.resource = "ar.loop";
        e.version = "1";
        e.node = phase;
        e.type = type;
        e.frame_id = frame;
        e.time_ms = ctx.session.clock().now_ms();
        e.details = json(data);
        if (i.trace)
            i.trace->emit(e);
        if (i.callback) {
            Json outer;
            outer.SetObject();
            auto &al = outer.GetAllocator();
            outer.AddMember("schema_version", 1, al);
            put(outer, "type", type, al);
            put(outer, "run_id", i.run_id, al);
            put(outer, "phase", phase, al);
            outer.AddMember("frame_id", frame, al);
            outer.AddMember("time_ms", e.time_ms, al);
            Value value;
            value.CopyFrom(data, al);
            outer.AddMember("data", value, al);
            try {
                i.callback(json(outer));
            } catch (const std::exception &ex) {
                i.event_error = failure(ErrorCode::io_failed, "loop.events", ex.what());
            }
        }
    }
    void lifecycle(std::string type, std::string phase, std::string status = "") {
        Json d;
        d.SetObject();
        auto &a = d.GetAllocator();
        put(d, "status", status, a);
        if (type == "loop.phase.end" && status != "success" && status != "not_done" && i.last && i.trace) {
            auto saved = i.trace->record_frame(*i.last, true, true);
            if (saved)
                put(d, "failure_image", saved.value(), a);
            else
                i.event_error = saved.error();
        }
        event(std::move(type), std::move(phase), std::move(d));
    }
    void frame_event(const Frame &frame, const std::string &phase, const std::string &kind,
                     const std::string &selected, const Observation &goal, Value rules, double capture_ms,
                     int64_t stable_ms, double rec_ms, double action_ms, const std::string &pending = "",
                     bool capped = false, const std::string &action_result = "") {
        Json d;
        d.SetObject();
        auto &a = d.GetAllocator();
        put(d, "kind", kind, a);
        put(d, "selected", selected, a);
        put(d, "pending", pending, a);
        put(d, "action_result", action_result, a);
        d.AddMember("decision_ms", decision_ms, a);
        d.AddMember("loop_revision", 2, a);
        d.AddMember("phase_deadline_ms", deadline, a);
        d.AddMember("busy", active_state && active_state->busy, a);
        d.AddMember("settle_until", active_state ? active_state->settle_until : int64_t(-1), a);
        put(d, "settle_reason", active_state ? active_state->settle_reason : "", a);
        put(d, "settle_rule", active_state && active_state->settling ? active_state->settling->name : "", a);
        d.AddMember("unknown_ms", active_state ? active_state->unknown_ms : int64_t(0), a);
        d.AddMember("unknown_limit_ms", active_state ? active_state->unknown_limit_ms : int64_t(0), a);
        Value attempts(rapidjson::kObjectType);
        uint32_t retry = 0;
        if (active_state) {
            for (auto &[name, count] : active_state->attempts)
                put_count(attempts, name, count, a);
            auto it = active_state->attempts.find(active_state->last_rule);
            if (it != active_state->attempts.end() && it->second)
                retry = it->second - 1;
        }
        d.AddMember("rule_attempts", attempts, a);
        Value shared(rapidjson::kObjectType);
        for (auto &[key, count] : i.task_attempts)
            put_count(shared, key, count, a);
        d.AddMember("task_attempts", shared, a);
        d.AddMember("retry_count", retry, a);
        d.AddMember("action_started_ms", action_started_ms, a);
        if (action_rule) {
            Value action(rapidjson::kObjectType);
            put(action, "type",
                action_rule->action.input.type == ar::runtime::ActionType::click ? "tap" : "key", a);
            action.AddMember("side_effect", action_rule->action.side_effect, a);
            action.AddMember("keycode", action_rule->action.input.keycode, a);
            if (action_target) {
                Value target(rapidjson::kArrayType);
                target.PushBack(action_target->x, a);
                target.PushBack(action_target->y, a);
                action.AddMember("target", target, a);
            }
            if (action_error)
                put(action, "error",
                    std::string(ar::core::error_name(action_error->code)) + ": " + action_error->message, a);
            d.AddMember("action", action, a);
        }
        d.AddMember("utc_unix_ms", utc(), a);
        d.AddMember("generation", frame.generation, a);
        d.AddMember("stable_capped", capped, a);
        d.AddMember("goal", observation(goal, a), a);
        d.AddMember("capture_clock_ms", frame.captured_ms, a);
        d.AddMember("goal_evaluated", kind != "stabilizing", a);
        Value copied;
        copied.CopyFrom(rules, a);
        d.AddMember("rules", copied, a);
        Value timing(rapidjson::kObjectType);
        timing.AddMember("capture_ms", capture_ms, a);
        timing.AddMember("stable_wait_ms", stable_ms, a);
        timing.AddMember("recognition_ms", rec_ms, a);
        timing.AddMember("action_ms", action_ms, a);
        d.AddMember("timing", timing, a);
        if (i.trace) {
            auto started = wall();
            auto saved = i.trace->record_frame(frame, i.options.full_frames);
            if (!saved)
                throw saved.error();
            put(d, "image", saved.value(), a);
            d.AddMember("replayable", i.options.full_frames, a);
            d["timing"].AddMember("trace_image_ms", wall() - started, a);
        }
        event("loop.frame", phase, std::move(d), frame.id);
    }
    Frame stable(const Phase &p, uint32_t &count, int64_t end, double &capture_ms, int64_t &wait_ms,
                 bool &capped) {
        std::vector<cv::Rect> r;
        regions(*p.goal_, r);
        for (auto &rule : p.rules_)
            regions(rule.condition, r);
        for (auto &rule : i.globals)
            regions(rule.condition, r);
        for (auto &g : i.guards)
            regions(g.second, r);
        cv::Mat previous;
        const auto began = ctx.session.clock().now_ms();
        for (;;) {
            check();
            if (count >= p.max_frames_)
                throw failure(ErrorCode::budget_exceeded, "loop.frames", "Phase frame limit");
            if (ctx.session.clock().now_ms() >= end)
                throw failure(ErrorCode::timeout, "loop.phase", "Phase timeout");
            auto started = wall();
            auto captured = ctx.session.capture(stop, budget());
            capture_ms = wall() - started;
            if (!captured)
                throw captured.error();
            Frame f = std::move(captured.value());
            i.last = f;
            ++count;
            ++i.report.frames;
            auto small = sample(f.image, r);
            wait_ms = ctx.session.clock().now_ms() - began;
            capped = wait_ms >= i.options.stable_max.count();
            if (active_state)
                active_state->busy = is_busy(f);
            bool ready = (active_state && active_state->busy) || capped ||
                         (!previous.empty() && difference(previous, small) <= i.options.stable_difference);
            if (ready)
                return f;
            decision_ms = action_started_ms = -1;
            action_rule = nullptr;
            action_target.reset();
            action_error.reset();
            frame_event(f, p.name_, "stabilizing", "$stabilizing", {}, Value(rapidjson::kArrayType),
                        capture_ms, wait_ms, 0, 0);
            previous = std::move(small);
            auto slept = ctx.session.clock().sleep_ms(
                std::min<int64_t>(
                    i.options.poll.count(),
                    std::max<int64_t>(0, std::min(end, deadline) - ctx.session.clock().now_ms())),
                stop);
            if (!slept)
                throw slept.error();
        }
    }
    void rule_observation(Value &rows, const Rule &r, const std::string &scope,
                          const std::optional<Observation> &o, Alloc &a) {
        Value v(rapidjson::kObjectType);
        put(v, "name", scope + "/" + r.name, a);
        v.AddMember("evaluated", o.has_value(), a);
        if (o)
            v.AddMember("result", observation(*o, a), a);
        rows.PushBack(v, a);
    }
    struct Selection {
        std::string name;
        const Rule *rule = nullptr;
        Observation match;
        Observation goal;
    };
    bool is_busy(const Frame &f, bool replay = false) {
        for (auto &c : i.busy)
            if (eval(c, f, !replay).matched)
                return true;
        return false;
    }
    Selection select(const Phase &p, const Frame &f, PhaseState &state, Value &rows, Alloc &a,
                     bool replay = false, int64_t replay_time = 0) {
        Selection s;
        visual_known = false;
        state.settle_reason.clear();
        s.goal = eval(*p.goal_, f, !replay);
        state.busy = is_busy(f, replay);
        decision_ms = replay ? replay_time : ctx.session.clock().now_ms();
        if (state.was_unknown && !state.busy && state.last_time >= 0)
            state.unknown_ms += std::max<int64_t>(0, decision_ms - state.last_time);
        state.last_time = decision_ms;
        state.was_unknown = false;
        auto finish = [&]() {
            if (!state.busy && visual_known)
                state.unknown_ms = 0;
            state.was_unknown = s.name == "$wait" && !visual_known;
            return s;
        };
        for (auto &g : i.guards)
            if (eval(g.second, f, !replay).matched) {
                s.name = "$blocked/" + g.first;
                return finish();
            }
        if (state.effect) {
            auto confirmation = eval(*state.effect->action.confirmation, f, !replay);
            rule_observation(rows, *state.effect, "confirmation", confirmation, a);
            if (confirmation.matched) {
                i.effects[p.name_ + "/" + state.effect->name] = "confirmed";
                state.effect = nullptr;
                s.name = "$confirm.ok";
            } else if (decision_ms >= state.effect_end) {
                i.effects[p.name_ + "/" + state.effect->name] = "unknown";
                state.effect = nullptr;
                s.name = "$confirm.unknown";
                return finish();
            }
        }
        if (!state.effect && s.goal.matched) {
            if (state.settling)
                state.settle_reason = "goal";
            state.settling = nullptr;
            s.name = "$goal";
            return finish();
        }
        if (state.settling) {
            if (state.settling->action.confirmation &&
                eval(*state.settling->action.confirmation, f, !replay).matched)
                state.settle_reason = "expect";
            else if (decision_ms >= state.settle_until)
                state.settle_reason = "timeout";
            // An overlay can occlude the trigger without completing navigation.
            // Busy frames are not evidence that the old button is truly gone.
            else if (!state.settling->action.strict_navigation_confirmation && !state.busy &&
                     !eval(state.settling->condition, f, !replay).matched)
                state.settle_reason = "gone";
            if (state.settle_reason == "timeout" && state.settling->action.strict_navigation_confirmation) {
                s.name = "$nav.unconfirmed/" + state.settling->name;
                state.settling = nullptr;
                return finish();
            }
            if (!state.settle_reason.empty())
                state.settling = nullptr;
        }
        // Busy precedes globals and phase actions, even after a goal in the
        // preceding phase. It pauses unknown time, never the phase deadline.
        if (state.busy) {
            s.name = "$busy";
            return finish();
        }
        bool transient_wait = false;
        auto scan = [&](const std::vector<Rule> &rules, const std::string &scope, bool fallback) {
            for (auto &r : rules) {
                if (s.rule || (r.action.unknown_delay.count() > 0) != fallback) {
                    if (!fallback)
                        rule_observation(rows, r, scope, {}, a);
                    continue;
                }
                if (fallback && (visual_known || state.unknown_ms < r.action.unknown_delay.count()))
                    continue;
                auto o = eval(r.condition, f, !replay);
                if (r.action.global_retry_interval.count() > 0 && o.matched) {
                    auto found = i.global_retry_after.find(r.name);
                    if (found != i.global_retry_after.end() && decision_ms < found->second) {
                        transient_wait = true;
                        rule_observation(rows, r, scope, o, a);
                        continue;
                    }
                }
                rule_observation(rows, r, scope, o, a);
                if (o.matched) {
                    s.name = scope + "/" + r.name;
                    s.rule = &r;
                    s.match = o;
                }
            }
        };
        scan(i.globals, "global", false);
        if (!state.effect && !state.settling && !s.rule && !transient_wait) {
            scan(p.rules_, p.name_, false);
            if (!s.rule)
                scan(p.rules_, p.name_, true);
        }
        if (s.rule) {
            const auto &r = *s.rule;
            auto key = p.name_ + "/" + r.name;
            const auto limit = r.action.attempt_limit ? r.action.attempt_limit : p.stuck_after_;
            if (r.action.side_effect && i.effects.contains(key)) {
                s.rule = nullptr;
                s.name = "$effect.already_attempted";
            } else if (r.action.task_attempt_limit &&
                       i.task_attempts[r.action.budget_key] >= r.action.task_attempt_limit) {
                s.rule = nullptr;
                s.name = "$budget.exhausted/" + r.action.budget_key;
            } else if (!r.action.side_effect && r.action.global_retry_interval.count() == 0 &&
                       state.attempts[s.name] >= limit) {
                s.rule = nullptr;
                s.name = "$stuck/" + s.name;
            }
        } else if (transient_wait)
            s.name = "$transient.wait";
        else if (state.effect)
            s.name = "$confirm.wait";
        else if (state.settling)
            s.name = "$settle";
        else if (s.name != "$confirm.ok")
            s.name = "$wait";
        return finish();
    }
    void commit_action(const Phase &p, PhaseState &state, const Selection &s, int64_t now, int64_t end) {
        const auto &r = *s.rule;
        if (r.action.task_attempt_limit)
            ++i.task_attempts[r.action.budget_key];
        if (r.action.global_retry_interval.count() > 0)
            i.global_retry_after[r.name] = now + r.action.global_retry_interval.count();
        ++state.attempts[s.name];
        state.last_rule = s.name;
        state.unknown_ms = 0;
        state.was_unknown = false;
        if (r.action.side_effect) {
            i.effects[p.name_ + "/" + r.name] = "attempted";
            state.effect = &r;
            state.effect_end = std::min(end, now + r.action.confirmation_timeout.count());
        } else if (!s.name.starts_with("global/")) {
            state.settling = &r;
            state.settle_until = std::min(end, now + r.action.settle_timeout.count());
            state.settle_reason.clear();
        }
    }
    Status phase(const Phase &p, bool single_check = false) {
        struct Limit {
            int64_t &value;
            int64_t old;
            ~Limit() { value = old; }
        } limit{deadline, deadline};
        deadline = std::min(deadline, ctx.session.clock().now_ms() + p.timeout_.count());
        lifecycle("loop.phase.start", p.name_);
        const auto began = ctx.session.clock().now_ms();
        const auto end = std::min(deadline, began + p.timeout_.count());
        PhaseState state;
        state.unknown_limit_ms = p.unknown_after_.count();
        active_state = &state;
        struct Reset {
            PhaseState *&p;
            ~Reset() { p = nullptr; }
        } reset{active_state};
        const Rule *&pending = state.effect;
        uint32_t frames = 0;
        try {
            do {
                double cap = 0;
                int64_t stable_ms = 0;
                bool capped = false;
                Frame f = stable(p, frames, end, cap, stable_ms, capped);
                check();
                action_started_ms = -1;
                action_rule = nullptr;
                action_target.reset();
                action_error.reset();
                Json scratch;
                scratch.SetObject();
                auto &a = scratch.GetAllocator();
                Value rows(rapidjson::kArrayType);
                auto started = wall();
                auto s = select(p, f, state, rows, a);
                auto rec = wall() - started;
                double action_ms = 0;
                std::string action_result;
                Status terminal = Status::success;
                bool failed = false;
                if (s.name == "$confirm.unknown" || s.name == "$effect.already_attempted") {
                    i.report.reason = "Effect outcome unknown; action will not be repeated";
                    terminal = Status::outcome_unknown;
                    failed = true;
                } else if (s.name.starts_with("$blocked/") || s.name.starts_with("$budget.exhausted/") ||
                           s.name.starts_with("$nav.unconfirmed/")) {
                    i.report.reason = s.name;
                    terminal = Status::blocked;
                    failed = true;
                } else if (s.name.starts_with("$stuck/")) {
                    i.report.reason = "Navigation rule reached its phase attempt limit: " + s.name;
                    terminal = Status::failed;
                    failed = true;
                } else if (s.rule) {
                    const auto &r = *s.rule;
                    if (r.action.input.type == ar::runtime::ActionType::click && !r.action.input.target &&
                        s.match.box.empty())
                        throw failure(ErrorCode::invalid_argument, "loop.tap", "Tap needs a positive match");
                    check();
                    action_rule = &r;
                    if (r.action.input.target)
                        action_target = r.action.input.target;
                    else if (r.action.input.type == ar::runtime::ActionType::click)
                        action_target = cv::Point(s.match.box.x + s.match.box.width / 2,
                                                  s.match.box.y + s.match.box.height / 2);
                    action_started_ms = ctx.session.clock().now_ms();
                    commit_action(p, state, s, action_started_ms, end);
                    started = wall();
                    auto result = ctx.session.perform(r.action.input, &f, action_target, stop, budget());
                    action_ms = wall() - started;
                    ++i.report.actions;
                    action_result = result ? "accepted" : "unknown";
                    if (!result)
                        action_error = result.error();
                }
                frame_event(f, p.name_, single_check ? "task_check" : "decision", s.name, s.goal,
                            std::move(rows), cap, stable_ms, rec, action_ms, pending ? pending->name : "",
                            capped, action_result);
                if (action_error && action_error->code == ErrorCode::cancelled)
                    throw *action_error;
                check();
                if (failed) {
                    lifecycle("loop.phase.end", p.name_, status_name(terminal));
                    return terminal;
                }
                if (!pending && s.goal.matched) {
                    i.milestones.insert(p.name_);
                    lifecycle("loop.phase.end", p.name_, "success");
                    return Status::success;
                }
                if (single_check) {
                    lifecycle("loop.phase.end", p.name_, "not_done");
                    return Status::already_done;
                }
                if (state.unknown_ms >= p.unknown_after_.count()) {
                    i.report.reason = "No recognized rule within unknown-time budget";
                    lifecycle("loop.phase.end", p.name_, "unrecognized");
                    return Status::failed;
                }
                auto slept = ctx.session.clock().sleep_ms(
                    std::min<int64_t>(i.options.poll.count(),
                                      std::max<int64_t>(0, end - ctx.session.clock().now_ms())),
                    stop);
                if (!slept)
                    throw slept.error();
            } while (true);
        } catch (const ar::core::Error &e) {
            if (pending)
                i.effects[p.name_ + "/" + pending->name] = "unknown";
            i.report.reason = e.message;
            lifecycle("loop.error", p.name_, ar::core::error_name(e.code));
            auto status = e.code == ErrorCode::cancelled ? Status::cancelled
                          : pending                      ? Status::outcome_unknown
                                                         : Status::failed;
            lifecycle("loop.phase.end", p.name_, status_name(status));
            return status;
        } catch (const std::exception &e) {
            if (pending)
                i.effects[p.name_ + "/" + pending->name] = "unknown";
            i.report.reason = e.what();
            lifecycle("loop.error", p.name_, e.what());
            auto status = pending ? Status::outcome_unknown : Status::failed;
            lifecycle("loop.phase.end", p.name_, status_name(status));
            return status;
        } catch (...) {
            if (pending)
                i.effects[p.name_ + "/" + pending->name] = "unknown";
            i.report.reason = "Unknown recognizer/host exception";
            auto status = pending ? Status::outcome_unknown : Status::failed;
            lifecycle("loop.phase.end", p.name_, status_name(status));
            return status;
        }
    }
    void start(std::string name) {
        static std::atomic<uint64_t> seq{0};
        i.run_id = std::to_string(utc()) + "-" + std::to_string(++seq);
        i.report = {};
        i.report.run_id = i.run_id;
        i.report.trace_directory = i.options.trace_root / ar::core::utf8_to_wide(i.run_id);
        if (std::filesystem::exists(i.report.trace_directory))
            throw std::invalid_argument("Trace already exists");
        i.trace = std::make_unique<ar::runtime::TraceWriter>(i.report.trace_directory);
        i.effects.clear();
        i.task_attempts.clear();
        i.global_retry_after.clear();
        i.milestones.clear();
        i.last.reset();
        i.event_error.reset();
        check();
        lifecycle("loop.task.start", name);
    }
    Report finish(Status s, std::string name) {
        i.report.status = s;
        if (s == Status::success || s == Status::already_done)
            i.report.reason.clear();
        lifecycle("loop.task.end", name, status_name(s));
        if (i.trace && i.trace->error()) {
            i.report.status = Status::failed;
            i.report.reason = i.trace->error()->message;
        }
        if (i.event_error) {
            i.report.status = Status::failed;
            i.report.reason = i.event_error->message;
        }
        return i.report;
    }
};
Report Phase::run(Context &ctx, std::stop_token stop) const {
    try {
        if (timeout_.count() < 1 || timeout_.count() > 3600000)
            throw std::invalid_argument("Invalid phase timeout");
        Engine e(ctx, stop, ctx.session.clock().now_ms() + timeout_.count());
        e.validate(*this);
        for (auto &r : ctx.impl_->globals)
            e.validate(r, true);
        for (auto &g : ctx.impl_->guards)
            e.validate(g.second);
        for (auto &b : ctx.impl_->busy)
            e.validate(b);
        auto lock = ctx.session.acquire_run();
        if (!lock)
            return {Status::failed, lock.error().message};
        e.start(name_);
        return e.finish(e.phase(*this), name_);
    } catch (const std::exception &e) {
        return {Status::failed, e.what()};
    } catch (const ar::core::Error &e) {
        return {e.code == ErrorCode::cancelled ? Status::cancelled : Status::failed, e.message};
    }
}
Report Task::run(Context &ctx, std::stop_token stop) const {
    try {
        if (!done_ || phases_.empty() || timeout_.count() < 1 || timeout_.count() > 3600000 || retries_ > 100)
            throw std::invalid_argument("Task needs completion check, phases and finite limits");
        Engine e(ctx, stop, ctx.session.clock().now_ms() + timeout_.count());
        e.validate(*done_);
        std::set<std::string> names;
        for (auto &p : phases_) {
            e.validate(p);
            if (!names.insert(p.name_).second)
                throw std::invalid_argument("Duplicate phase name");
        }
        for (auto &r : ctx.impl_->globals)
            e.validate(r, true);
        for (auto &g : ctx.impl_->guards)
            e.validate(g.second);
        for (auto &b : ctx.impl_->busy)
            e.validate(b);
        if (ctx.impl_->recovery)
            e.validate(*ctx.impl_->recovery);
        auto lock = ctx.session.acquire_run();
        if (!lock)
            return {Status::failed, lock.error().message};
        e.start(name_);
        for (uint32_t attempt = 0; attempt <= retries_; ++attempt) {
            Phase check("@task.done");
            check.goal(*done_);
            auto done = e.phase(check, true);
            if (done == Status::success)
                return e.finish(Status::already_done, name_);
            if (done != Status::already_done)
                return e.finish(done, name_);
            Status status = Status::success;
            for (auto &p : phases_) {
                if (p.checkpoint_ && ctx.impl_->milestones.contains(p.name_)) {
                    e.lifecycle("loop.phase.skip", p.name_, "completed_checkpoint");
                    continue;
                }
                status = e.phase(p);
                if (status != Status::success)
                    break;
            }
            if (status == Status::success)
                return e.finish(status, name_);
            if (status == Status::cancelled || status == Status::blocked || attempt == retries_ ||
                !ctx.impl_->recovery)
                return e.finish(status, name_);
            ++ctx.impl_->report.recoveries;
            e.lifecycle("loop.recovery", ctx.impl_->recovery->name_);
            auto recovered = e.phase(*ctx.impl_->recovery);
            if (recovered != Status::success)
                return e.finish(recovered, name_);
        }
        return e.finish(Status::failed, name_);
    } catch (const std::exception &e) {
        return {Status::failed, e.what()};
    } catch (const ar::core::Error &e) {
        return {e.code == ErrorCode::cancelled ? Status::cancelled : Status::failed, e.message};
    }
}
Result<uint32_t> Task::replay(Context &ctx, const std::filesystem::path &directory,
                              std::function<void(const ReplayDecision &)> observer) const {
    try {
        if (!done_)
            throw std::invalid_argument("Replay task needs done condition");
        Engine e(ctx, {}, INT64_MAX / 2);
        auto &i = *ctx.impl_;
        i.milestones.clear();
        i.effects.clear();
        i.task_attempts.clear();
        i.global_retry_after.clear();
        i.ocr_cache.clear();
        i.ocr_frame = 0;
        std::map<std::string, const Phase *> phases;
        for (auto &p : phases_) {
            e.validate(p);
            phases[p.name_] = &p;
        }
        if (i.recovery) {
            e.validate(*i.recovery);
            phases[i.recovery->name_] = &*i.recovery;
        }
        Phase done("@task.done");
        done.goal(*done_);
        phases[done.name_] = &done;
        e.validate(*done_);
        for (auto &r : i.globals)
            e.validate(r, true);
        for (auto &g : i.guards)
            e.validate(g.second);
        for (auto &b : i.busy)
            e.validate(b);
        std::ifstream file(directory / L"events.jsonl");
        if (!file)
            throw std::invalid_argument("Missing trace events");
        std::string line;
        uint32_t count = 0, phase_frames = 0;
        Engine::PhaseState state;
        e.active_state = &state;
        cv::Mat stable_previous;
        while (std::getline(file, line)) {
            Json outer;
            outer.Parse(line.c_str());
            if (outer.HasParseError() || !outer.IsObject())
                throw std::invalid_argument("Invalid trace JSON");
            for (auto key : {"type", "node", "details"})
                if (!outer.HasMember(key) || !outer[key].IsString())
                    throw std::invalid_argument("Missing trace field");
            Json data;
            data.Parse(outer["details"].GetString());
            if (data.HasParseError() || !data.IsObject())
                throw std::invalid_argument("Invalid frame details");
            const std::string type = outer["type"].GetString(), phase = outer["node"].GetString();
            if (type == "loop.phase.start") {
                state = {};
                stable_previous.release();
                phase_frames = 0;
            }
            if (type == "loop.phase.end" && data.HasMember("status") && data["status"].IsString() &&
                std::string(data["status"].GetString()) == "success")
                i.milestones.insert(phase);
            if (type != "loop.frame")
                continue;
            if (!data.HasMember("loop_revision") || !data["loop_revision"].IsInt() ||
                data["loop_revision"].GetInt() != 2)
                return failure(ErrorCode::unsupported, "loop.replay",
                               "B2 legacy trace: use archived B2 replay or counterfactual project(), not "
                               "exact B2b replay");
            if (!data.HasMember("replayable") || !data["replayable"].IsBool() ||
                !data["replayable"].GetBool())
                return failure(ErrorCode::unsupported, "loop.replay",
                               "Trace contains thumbnails; record full_frames for exact replay");
            if (!outer.HasMember("frame_id") || !outer["frame_id"].IsUint64() || !data.HasMember("busy") ||
                !data["busy"].IsBool())
                throw std::invalid_argument("Invalid frame id/busy type");
            for (auto key : {"image", "kind", "selected", "settle_reason", "settle_rule"})
                if (!data.HasMember(key) || !data[key].IsString())
                    throw std::invalid_argument("Invalid frame string field");
            for (auto key : {"decision_ms", "action_started_ms", "phase_deadline_ms", "settle_until",
                             "unknown_ms", "unknown_limit_ms"})
                if (!data.HasMember(key) || !data[key].IsInt64())
                    throw std::invalid_argument("Invalid frame timing type");
            if (!data.HasMember("retry_count") || !data["retry_count"].IsUint() ||
                !data.HasMember("rule_attempts") || !data["rule_attempts"].IsObject() ||
                !data.HasMember("timing") || !data["timing"].IsObject() ||
                !data["timing"].HasMember("stable_wait_ms") || !data["timing"]["stable_wait_ms"].IsInt64())
                throw std::invalid_argument("Invalid replay counters/stability timing");
            auto relative = std::filesystem::path(ar::core::utf8_to_wide(data["image"].GetString()));
            auto path = std::filesystem::weakly_canonical(directory / relative);
            if (relative.is_absolute() ||
                !path.generic_wstring().starts_with(
                    std::filesystem::weakly_canonical(directory).generic_wstring() + L"/"))
                throw std::invalid_argument("Trace image escapes root");
            auto pixels = ar::core::read_image(path);
            if (!pixels)
                throw std::invalid_argument(pixels.error().message);
            Frame f;
            f.id = outer["frame_id"].GetUint64();
            f.image = pixels.value();
            if (f.image.size() != ctx.session.config().reference_size)
                throw std::invalid_argument("Replay image not full canonical size");
            auto it = phases.find(phase);
            if (it == phases.end())
                throw std::invalid_argument("Unknown replay phase");
            const auto &p = *it->second;
            state.unknown_limit_ms = p.unknown_after_.count();
            ++count;
            ++phase_frames;
            if (phase_frames > p.max_frames_)
                throw std::invalid_argument("Replay phase frame budget changed");
            std::vector<cv::Rect> areas;
            e.regions(*p.goal_, areas);
            for (auto &r : p.rules_)
                e.regions(r.condition, areas);
            for (auto &r : i.globals)
                e.regions(r.condition, areas);
            for (auto &g : i.guards)
                e.regions(g.second, areas);
            auto small = sample(f.image, areas);
            state.busy = e.is_busy(f, true);
            bool ready = state.busy ||
                         data["timing"]["stable_wait_ms"].GetInt64() >= i.options.stable_max.count() ||
                         (!stable_previous.empty() &&
                          difference(stable_previous, small) <= i.options.stable_difference);
            const bool stabilizing = std::string(data["kind"].GetString()) == "stabilizing";
            if (ready == stabilizing)
                return failure(ErrorCode::resource_invalid, "loop.replay",
                               "Stability changed at frame " + std::to_string(f.id));
            std::string actual;
            std::optional<cv::Point> target;
            const Rule *planned = nullptr;
            if (stabilizing) {
                actual = "$stabilizing";
                stable_previous = std::move(small);
            } else {
                stable_previous.release();
                Json scratch;
                scratch.SetObject();
                Value rows(rapidjson::kArrayType);
                auto s =
                    e.select(p, f, state, rows, scratch.GetAllocator(), true, data["decision_ms"].GetInt64());
                actual = s.name;
                if (s.rule) {
                    planned = s.rule;
                    const auto &r = *s.rule;
                    if (r.action.input.target)
                        target = r.action.input.target;
                    else if (r.action.input.type == ar::runtime::ActionType::click)
                        target = cv::Point(s.match.box.x + s.match.box.width / 2,
                                           s.match.box.y + s.match.box.height / 2);
                    e.commit_action(p, state, s, data["action_started_ms"].GetInt64(),
                                    data["phase_deadline_ms"].GetInt64());
                }
            }
            Json shared;
            shared.SetObject();
            for (auto &[key, n] : i.task_attempts)
                put_count(shared, key, n, shared.GetAllocator());
            if (data.HasMember("task_attempts")) {
                if (!data["task_attempts"].IsObject() || json(data["task_attempts"]) != json(shared))
                    throw std::invalid_argument("Task budget state changed in replay");
            } else if (!i.task_attempts.empty())
                throw std::invalid_argument("Missing task budget state: use archived policy for this trace");
            const std::string expected = data["selected"].GetString();
            if (observer)
                observer({f.id, phase, expected, actual, actual == expected});
            if (actual != expected)
                return failure(ErrorCode::resource_invalid, "loop.replay",
                               "Decision mismatch at frame " + std::to_string(f.id) + ": " + expected +
                                   " vs " + actual);
            if (data.HasMember("action") != (planned != nullptr))
                throw std::invalid_argument("Replay input presence changed");
            if (planned) {
                const auto &v = data["action"];
                if (!v.IsObject() || !v.HasMember("type") || !v["type"].IsString() ||
                    !v.HasMember("side_effect") || !v["side_effect"].IsBool() || !v.HasMember("keycode") ||
                    !v["keycode"].IsInt() ||
                    std::string(v["type"].GetString()) !=
                        (planned->action.input.type == ar::runtime::ActionType::click ? "tap" : "key") ||
                    v["side_effect"].GetBool() != planned->action.side_effect ||
                    v["keycode"].GetInt() != planned->action.input.keycode)
                    throw std::invalid_argument("Replay input type/effect/key changed");
                if (target && (!v.HasMember("target") || !v["target"].IsArray() || v["target"].Size() != 2 ||
                               !v["target"][0].IsInt() || !v["target"][1].IsInt()))
                    throw std::invalid_argument("Invalid replay input target");
            }
            for (auto key : {"busy", "settle_until", "settle_reason", "settle_rule", "rule_attempts",
                             "retry_count", "unknown_ms", "unknown_limit_ms"})
                if (!data.HasMember(key))
                    throw std::invalid_argument("Missing B2b decision state");
            Json attempts;
            attempts.SetObject();
            for (auto &[name, n] : state.attempts)
                put_count(attempts, name, n, attempts.GetAllocator());
            auto hit = state.attempts.find(state.last_rule);
            const uint32_t retry = hit != state.attempts.end() && hit->second ? hit->second - 1 : 0;
            if (data["busy"].GetBool() != state.busy ||
                data["settle_until"].GetInt64() != state.settle_until ||
                data["settle_reason"].GetString() != state.settle_reason ||
                std::string(data["settle_rule"].GetString()) !=
                    (state.settling ? state.settling->name : "") ||
                json(data["rule_attempts"]) != json(attempts) || data["retry_count"].GetUint() != retry ||
                data["unknown_ms"].GetInt64() != state.unknown_ms ||
                data["unknown_limit_ms"].GetInt64() != state.unknown_limit_ms)
                return failure(ErrorCode::resource_invalid, "loop.replay",
                               "B2b settle/busy/retry/unknown fields changed at frame " +
                                   std::to_string(f.id));
            if (target && (!data.HasMember("action") || !data["action"].HasMember("target") ||
                           (*target).x != data["action"]["target"][0].GetInt() ||
                           (*target).y != data["action"]["target"][1].GetInt()))
                return failure(ErrorCode::resource_invalid, "loop.replay",
                               "Action target changed at frame " + std::to_string(f.id));
        }
        if (!count)
            throw std::invalid_argument("Trace has no frames");
        return count;
    } catch (const ar::core::Error &e) {
        return e;
    } catch (const std::exception &e) {
        return failure(ErrorCode::resource_invalid, "loop.replay", e.what());
    }
}

Result<ProjectionReport> Task::project(Context &ctx, const std::filesystem::path &directory,
                                       std::function<void(const std::string &)> observer) const {
    try {
        Engine e(ctx, {}, INT64_MAX / 2);
        auto &i = *ctx.impl_;
        i.effects.clear();
        i.task_attempts.clear();
        i.global_retry_after.clear();
        i.milestones.clear();
        i.ocr_cache.clear();
        i.ocr_frame = 0;
        for (auto &p : phases_)
            e.validate(p);
        for (auto &r : i.globals)
            e.validate(r, true);
        for (auto &g : i.guards)
            e.validate(g.second);
        for (auto &b : i.busy)
            e.validate(b);
        std::ifstream file(directory / L"events.jsonl");
        if (!file)
            throw std::invalid_argument("Missing projection trace");
        std::string line;
        ProjectionReport report;
        size_t index = 0;
        Phase done("@task.done");
        if (!done_)
            throw std::invalid_argument("Projection needs task completion condition");
        done.goal(*done_);
        e.validate(done);
        bool checking_done = true;
        uint32_t phase_frames = 0;
        Engine::PhaseState state;
        e.active_state = &state;
        int64_t phase_start = -1, task_start = -1, stable_start = -1;
        cv::Mat previous;
        while (std::getline(file, line)) {
            Json outer;
            outer.Parse(line.c_str());
            if (outer.HasParseError() || !outer.IsObject())
                throw std::invalid_argument("Invalid projection JSON");
            if (!outer.HasMember("type") || !outer["type"].IsString())
                throw std::invalid_argument("Missing projection event type");
            if (std::string(outer["type"].GetString()) != "loop.frame")
                continue;
            if (!outer.HasMember("details") || !outer["details"].IsString() || !outer.HasMember("frame_id") ||
                !outer["frame_id"].IsUint64())
                throw std::invalid_argument("Invalid projection event fields");
            Json data;
            data.Parse(outer["details"].GetString());
            if (data.HasParseError() || !data.IsObject() || !data.HasMember("image") ||
                !data["image"].IsString() || !data.HasMember("capture_clock_ms") ||
                !data["capture_clock_ms"].IsInt64())
                throw std::invalid_argument("Missing projection image/time");
            auto rel = std::filesystem::path(ar::core::utf8_to_wide(data["image"].GetString()));
            auto full = std::filesystem::weakly_canonical(directory / rel);
            if (rel.is_absolute() ||
                !full.generic_wstring().starts_with(
                    std::filesystem::weakly_canonical(directory).generic_wstring() + L"/"))
                throw std::invalid_argument("Projection image escapes root");
            auto pixels = ar::core::read_image(full);
            if (!pixels)
                throw std::invalid_argument(pixels.error().message);
            Frame f;
            f.id = outer["frame_id"].GetUint64();
            f.image = pixels.value();
            if (f.image.size() != ctx.session.config().reference_size)
                throw std::invalid_argument("Projection needs full canonical frames");
            const auto now = data["capture_clock_ms"].GetInt64();
            if (task_start < 0)
                task_start = now;
            if (phase_start < 0)
                phase_start = now;
            ++report.frames;
            Json row;
            row.SetObject();
            auto &a = row.GetAllocator();
            row.AddMember("frame_id", f.id, a);
            row.AddMember("source_clock_ms", now, a);
            std::string selected = "$finished", phase = "@finished";
            std::optional<cv::Point> target;
            int keycode = 0;
            bool acted = false;
            if (!report.complete && report.reason.empty() && (checking_done || index < phases_.size())) {
                const auto &p = checking_done ? done : phases_[index];
                phase = p.name_;
                state.unknown_limit_ms = p.unknown_after_.count();
                ++phase_frames;
                const auto end = std::min(task_start + timeout_.count(), phase_start + p.timeout_.count());
                if (now >= end || phase_frames > p.max_frames_) {
                    report.reason = "Projection phase/task budget: " + phase;
                    selected = "$failed/budget";
                } else {
                    std::vector<cv::Rect> areas;
                    e.regions(*p.goal_, areas);
                    for (auto &r : p.rules_)
                        e.regions(r.condition, areas);
                    for (auto &r : i.globals)
                        e.regions(r.condition, areas);
                    for (auto &g : i.guards)
                        e.regions(g.second, areas);
                    auto small = sample(f.image, areas);
                    if (stable_start < 0)
                        stable_start = now;
                    state.busy = e.is_busy(f, true);
                    const bool ready =
                        state.busy || now - stable_start >= i.options.stable_max.count() ||
                        (!previous.empty() && difference(previous, small) <= i.options.stable_difference);
                    if (!ready) {
                        selected = "$stabilizing";
                        previous = std::move(small);
                    } else {
                        previous.release();
                        stable_start = -1;
                        Json scratch;
                        scratch.SetObject();
                        Value rows(rapidjson::kArrayType);
                        auto s = e.select(p, f, state, rows, scratch.GetAllocator(), true, now);
                        selected = s.name;
                        if (s.rule) {
                            auto &act = s.rule->action;
                            if (act.input.target)
                                target = act.input.target;
                            else if (act.input.type == ar::runtime::ActionType::click)
                                target = cv::Point(s.match.box.x + s.match.box.width / 2,
                                                   s.match.box.y + s.match.box.height / 2);
                            keycode = act.input.keycode;
                            acted = true;
                            ++report.actions;
                            e.commit_action(p, state, s, now, end);
                        }
                        if (selected == "$goal") {
                            i.milestones.insert(phase);
                            if (checking_done) {
                                checking_done = false;
                                index = phases_.size();
                            } else
                                ++index;
                            phase_start = -1;
                        } else if (selected.starts_with("$stuck/") || selected.starts_with("$blocked/") ||
                                   selected.starts_with("$budget.exhausted/") ||
                                   selected.starts_with("$nav.unconfirmed/") ||
                                   selected == "$confirm.unknown" || selected == "$effect.already_attempted")
                            report.reason = "Projection stopped: " + selected;
                        else if (checking_done) {
                            checking_done = false;
                            phase_start = -1;
                        } else if (state.unknown_ms >= p.unknown_after_.count())
                            report.reason = "Projection unknown-time budget: " + phase;
                    }
                }
            } else if (!report.reason.empty()) {
                selected = "$failed";
                phase = index < phases_.size() ? phases_[index].name_ : "@failed";
            }
            put(row, "phase", phase, a);
            put(row, "selected", selected, a);
            row.AddMember("busy", state.busy, a);
            row.AddMember("settle_until", state.settle_until, a);
            put(row, "settle_reason", state.settle_reason, a);
            row.AddMember("unknown_ms", state.unknown_ms, a);
            Value attempts(rapidjson::kObjectType);
            for (auto &[n, c] : state.attempts)
                put_count(attempts, n, c, a);
            row.AddMember("rule_attempts", attempts, a);
            Value shared(rapidjson::kObjectType);
            for (auto &[key, n] : i.task_attempts)
                put_count(shared, key, n, a);
            row.AddMember("task_attempts", shared, a);
            row.AddMember("action", acted, a);
            row.AddMember("keycode", keycode, a);
            if (target) {
                Value v(rapidjson::kArrayType);
                v.PushBack(target->x, a);
                v.PushBack(target->y, a);
                row.AddMember("target", v, a);
            }
            if (observer)
                observer(json(row));
            if (phase_start < 0) {
                state = {};
                previous.release();
                stable_start = -1;
                phase_frames = 0;
            }
            if (index == phases_.size() && !checking_done)
                report.complete = true;
        }
        if (!report.complete && report.reason.empty())
            report.reason = "Recorded images ended before projected task completed";
        return report;
    } catch (const ar::core::Error &e) {
        return e;
    } catch (const std::exception &e) {
        return failure(ErrorCode::resource_invalid, "loop.project", e.what());
    }
}

} // namespace ar::loop
