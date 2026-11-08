#include "host/JobHost.h"
#include "core/Json.h"
#include "core/IO.h"
#include "core/Str.hpp"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <set>
#include <tuple>
#include <atomic>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace ar::host {
using ar::core::ErrorCode;
using ar::core::failure;
namespace {
using Json = rapidjson::Document;
using Value = rapidjson::Value;
int64_t now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
std::string utf(const std::filesystem::path &p) {
    return ar::core::wide_to_utf8(p.wstring());
}
std::string stringify(const Value &v) {
    rapidjson::StringBuffer b;
    rapidjson::Writer<rapidjson::StringBuffer> w(b);
    v.Accept(w);
    return {b.GetString(), b.GetSize()};
}
bool object_json(const std::string &s, int max_depth = 32) {
    int depth = 0;
    bool in = false, escaped = false;
    for (unsigned char c : s) {
        if (in) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                in = false;
        } else if (c == '"')
            in = true;
        else if (c == '{' || c == '[') {
            if (++depth > max_depth)
                return false;
        } else if (c == '}' || c == ']') {
            if (--depth < 0)
                return false;
        }
    }
    if (in || depth)
        return false;
    Json d;
    d.Parse<rapidjson::kParseValidateEncodingFlag>(s.data(), s.size());
    return !d.HasParseError() && d.IsObject();
}
void put(Value &v, const char *key, const std::string &s, Json::AllocatorType &a) {
    Value text;
    text.SetString(s.data(), static_cast<rapidjson::SizeType>(s.size()), a);
    v.AddMember(Value(key, a), text, a);
}
bool safe_id(std::string_view s) {
    return !s.empty() && s.size() <= 128 &&
           std::all_of(s.begin(), s.end(),
                       [](unsigned char c) {
                           return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                  (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
                       }) &&
           s != "." && s != "..";
}
std::wstring mutex_name(std::string_view key) {
    uint64_t n = 14695981039346656037ull;
    for (unsigned char c : key) {
        n ^= c;
        n *= 1099511628211ull;
    }
    std::wostringstream name;
    name << L"Local\\OpenAR.host.device." << std::hex << n;
    return name.str();
}
struct Lease {
    HANDLE handle = nullptr;
    bool owned = false;
    explicit Lease(const std::string &key) {
        if (key.empty()) {
            owned = true;
            return;
        }
        handle = CreateMutexW(nullptr, FALSE, mutex_name(key).c_str());
        if (handle) {
            auto r = WaitForSingleObject(handle, 0);
            owned = r == WAIT_OBJECT_0 || r == WAIT_ABANDONED;
        }
    }
    ~Lease() {
        if (handle) {
            if (owned)
                ReleaseMutex(handle);
            CloseHandle(handle);
        }
    }
};
JobStatus parse_status(const std::string &s) {
    for (auto v :
         {JobStatus::queued, JobStatus::running, JobStatus::succeeded, JobStatus::failed,
          JobStatus::cancelled, JobStatus::blocked, JobStatus::outcome_unknown, JobStatus::interrupted})
        if (s == status_name(v))
            return v;
    throw std::invalid_argument("Invalid stored job status");
}
const Value &field(const Value &v, const char *k) {
    if (!v.IsObject() || !v.HasMember(k))
        throw std::invalid_argument("Incomplete stored job");
    return v[k];
}
std::string string_field(const Value &v, const char *k) {
    auto &x = field(v, k);
    if (!x.IsString())
        throw std::invalid_argument("Invalid stored job string");
    return {x.GetString(), x.GetStringLength()};
}
int64_t time_field(const Value &v, const char *k) {
    auto &x = field(v, k);
    if (!x.IsInt64())
        throw std::invalid_argument("Invalid stored timestamp");
    return x.GetInt64();
}
JobSnapshot decode(const std::string &text) {
    if (!object_json(text, 64))
        throw std::invalid_argument("Invalid stored job JSON/nesting");
    Json d;
    d.Parse(text.data(), text.size());
    if (d.HasParseError() || !d.IsObject() || !field(d, "schema_version").IsInt() ||
        d["schema_version"].GetInt() != 1)
        throw std::invalid_argument("Invalid stored job schema");
    JobSnapshot j;
    j.id = string_field(d, "id");
    auto &r = field(d, "request");
    j.request = {string_field(r, "device_id"), string_field(r, "task_id"), stringify(field(r, "options")),
                 string_field(r, "request_key")};
    j.status = parse_status(string_field(d, "status"));
    j.created_ms = time_field(d, "created_ms");
    j.started_ms = time_field(d, "started_ms");
    j.finished_ms = time_field(d, "finished_ms");
    auto &c = field(d, "cancellation_requested");
    if (!c.IsBool())
        throw std::invalid_argument("Invalid cancellation state");
    j.cancellation_requested = c.GetBool();
    auto &result = field(d, "result");
    j.result.status = j.status;
    j.result.reason = string_field(result, "reason");
    j.result.detail_json = stringify(field(result, "detail"));
    j.result.trace_directory = ar::core::utf8_to_wide(string_field(result, "trace_directory"));
    if (!safe_id(j.id) || !safe_id(j.request.device_id) || !safe_id(j.request.task_id) ||
        !object_json(j.request.options_json) || !object_json(j.result.detail_json) ||
        j.request.request_key.size() > 128)
        throw std::invalid_argument("Invalid stored job identifiers/options");
    return j;
}
} // namespace
const char *status_name(JobStatus s) {
    switch (s) {
    case JobStatus::queued:
        return "queued";
    case JobStatus::running:
        return "running";
    case JobStatus::succeeded:
        return "succeeded";
    case JobStatus::failed:
        return "failed";
    case JobStatus::cancelled:
        return "cancelled";
    case JobStatus::blocked:
        return "blocked";
    case JobStatus::outcome_unknown:
        return "outcome_unknown";
    case JobStatus::interrupted:
        return "interrupted";
    }
    return "failed";
}
bool terminal(JobStatus s) {
    return s != JobStatus::queued && s != JobStatus::running;
}
std::string snapshot_json(const JobSnapshot &j) {
    Json d;
    d.SetObject();
    auto &a = d.GetAllocator();
    d.AddMember("schema_version", 1, a);
    put(d, "id", j.id, a);
    put(d, "status", status_name(j.status), a);
    d.AddMember("created_ms", j.created_ms, a);
    d.AddMember("started_ms", j.started_ms, a);
    d.AddMember("finished_ms", j.finished_ms, a);
    d.AddMember("cancellation_requested", j.cancellation_requested, a);
    Value req(rapidjson::kObjectType);
    put(req, "device_id", j.request.device_id, a);
    put(req, "task_id", j.request.task_id, a);
    put(req, "request_key", j.request.request_key, a);
    Json options;
    options.Parse(j.request.options_json.c_str());
    req.AddMember("options", Value(options, a), a);
    d.AddMember("request", req, a);
    Value result(rapidjson::kObjectType);
    put(result, "reason", j.result.reason, a);
    put(result, "trace_directory", utf(j.result.trace_directory), a);
    Json detail;
    detail.Parse(j.result.detail_json.c_str());
    result.AddMember("detail", Value(detail, a), a);
    d.AddMember("result", result, a);
    return stringify(d);
}
struct JobHost::Impl {
    struct Job {
        JobSnapshot view;
        std::stop_source stop;
    };
    struct Worker {
        DeviceInfo device;
        std::deque<std::shared_ptr<Job>> queue;
        std::jthread thread;
    };
    HostOptions options;
    Executor executor;
    std::vector<DeviceInfo> devices;
    std::vector<TaskInfo> tasks;
    std::map<std::string, std::unique_ptr<Worker>> workers;
    std::map<std::string, std::shared_ptr<Job>> retained;
    std::deque<HostEvent> ring;
    size_t ring_bytes = 0;
    mutable std::mutex mutex;
    std::condition_variable cv;
    bool stopping = false;
    uint64_t counter = 0, sequence = 0;
    std::string instance;
    HANDLE state_lock = INVALID_HANDLE_VALUE;

    std::filesystem::path metadata(const std::string &id) const {
        return options.state_directory / L"jobs" / (ar::core::utf8_to_wide(id) + L".json");
    }
    void save(const JobSnapshot &j) {
        auto dest = metadata(j.id), temp = dest;
        temp += L".tmp";
        {
            std::ofstream f(temp, std::ios::binary | std::ios::trunc);
            auto s = snapshot_json(j);
            if (!f || !f.write(s.data(), s.size()) || !f.flush())
                throw std::runtime_error("Cannot persist job metadata");
        }
        if (!MoveFileExW(temp.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot commit job metadata");
    }
    bool trace_inside(const std::filesystem::path &p) const {
        if (p.empty())
            return true;
        auto parent = std::filesystem::weakly_canonical(options.trace_root);
        auto target = std::filesystem::weakly_canonical(p);
        auto rel = target.lexically_relative(parent);
        return !rel.empty() && !rel.is_absolute() && *rel.begin() != L".." && rel != L".";
    }
    void event(const std::string &id, std::string type, std::string payload) {
        if (payload.size() > 65536 || !object_json(payload)) {
            type = "host.event_omitted";
            payload = "{\"reason\":\"Event too large or invalid; full runtime trace remains authoritative\"}";
        }
        HostEvent e{++sequence, now(), id, std::move(type), std::move(payload)};
        ring_bytes += e.payload_json.size();
        ring.push_back(std::move(e));
        while (ring.size() > options.max_events || ring_bytes > options.max_event_bytes) {
            ring_bytes -= ring.front().payload_json.size();
            ring.pop_front();
        }
    }
    void state_event(const Job &j) { event(j.view.id, "host.job", snapshot_json(j.view)); }
    Impl(std::vector<DeviceInfo> d, std::vector<TaskInfo> t, Executor run, HostOptions o)
        : options(std::move(o)), executor(std::move(run)), devices(std::move(d)), tasks(std::move(t)) {
        if (!executor || devices.empty() || devices.size() > 32 || tasks.empty() || tasks.size() > 256 ||
            options.state_directory.empty() || options.trace_root.empty() || !options.max_jobs ||
            options.max_jobs > 10000 || !options.max_queued_per_device ||
            options.max_queued_per_device > 1000 || !options.max_events || options.max_events > 100000 ||
            options.max_event_bytes < 65536 || options.max_event_bytes > 256 * 1024 * 1024)
            throw std::invalid_argument("Invalid host configuration/limits");
        static std::atomic<uint64_t> instances{0};
        LARGE_INTEGER ticks;
        QueryPerformanceCounter(&ticks);
        instance = std::to_string(now()) + "-" + std::to_string(GetCurrentProcessId()) + "-" +
                   std::to_string(ticks.QuadPart) + "-" + std::to_string(++instances);
        std::set<std::string> task_ids, device_ids, lock_keys;
        for (auto &task : tasks)
            if (!safe_id(task.id) || !task_ids.insert(task.id).second)
                throw std::invalid_argument("Invalid/duplicate task ID");
        for (auto &device : devices) {
            if (!safe_id(device.id) || !device_ids.insert(device.id).second || device.lock_key.size() > 512 ||
                (!device.lock_key.empty() && !lock_keys.insert(device.lock_key).second))
                throw std::invalid_argument("Duplicate device identity/lease");
            for (auto &id : device.tasks)
                if (!task_ids.contains(id))
                    throw std::invalid_argument("Device references unregistered task");
        }
        options.state_directory = std::filesystem::absolute(options.state_directory);
        options.trace_root = std::filesystem::absolute(options.trace_root);
        std::filesystem::create_directories(options.state_directory / L"jobs");
        std::filesystem::create_directories(options.trace_root);
        state_lock =
            CreateFileW((options.state_directory / L".host.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (state_lock == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Host state directory is already locked/unavailable");
        try {
            for (auto &entry : std::filesystem::directory_iterator(options.state_directory / L"jobs")) {
                if (entry.path().extension() != L".json")
                    continue;
                auto bytes = ar::core::read_bytes(entry.path(), 1024 * 1024);
                if (!bytes)
                    throw std::runtime_error("Cannot read retained job");
                auto j = std::make_shared<Job>();
                j->view = decode(std::string(bytes.value().begin(), bytes.value().end()));
                if (entry.path().stem().wstring() != ar::core::utf8_to_wide(j->view.id) ||
                    !trace_inside(j->view.result.trace_directory) || retained.contains(j->view.id) ||
                    retained.size() >= options.max_jobs)
                    throw std::runtime_error("Invalid or oversized job history");
                if (!terminal(j->view.status)) {
                    j->view.status = j->view.result.status = JobStatus::interrupted;
                    j->view.finished_ms = now();
                    j->view.result.reason =
                        "Previous process ended before terminal state; never automatically resumed";
                    save(j->view);
                }
                retained.emplace(j->view.id, j);
            }
            for (auto &device : devices) {
                auto w = std::make_unique<Worker>();
                w->device = device;
                workers.emplace(device.id, std::move(w));
            }
            for (auto &[id, w] : workers)
                w->thread = std::jthread([this, ptr = w.get()] { work(*ptr); });
        } catch (...) {
            stop();
            CloseHandle(state_lock);
            state_lock = INVALID_HANDLE_VALUE;
            throw;
        }
    }
    ~Impl() {
        stop();
        if (state_lock != INVALID_HANDLE_VALUE)
            CloseHandle(state_lock);
    }
    void stop() {
        {
            std::lock_guard lock(mutex);
            if (!stopping) {
                stopping = true;
                for (auto &[id, j] : retained)
                    if (!terminal(j->view.status))
                        j->stop.request_stop();
            }
        }
        cv.notify_all();
        for (auto &[id, w] : workers)
            if (w->thread.joinable())
                w->thread.join();
    }
    void work(Worker &w) {
        for (;;) {
            std::shared_ptr<Job> job;
            {
                std::unique_lock lock(mutex);
                cv.wait(lock, [&] { return stopping || !w.queue.empty(); });
                if (w.queue.empty())
                    return;
                job = w.queue.front();
                w.queue.pop_front();
                if (job->view.status != JobStatus::queued)
                    continue;
                if (stopping || job->stop.stop_requested()) {
                    job->view.status = job->view.result.status = JobStatus::cancelled;
                    job->view.finished_ms = now();
                    job->view.result.reason = "Cancelled before execution";
                    try {
                        save(job->view);
                    } catch (const std::exception &e) {
                        job->view.result.reason = e.what();
                    }
                    state_event(*job);
                    continue;
                }
                job->view.status = JobStatus::running;
                job->view.started_ms = now();
                try {
                    save(job->view);
                } catch (const std::exception &e) {
                    job->view.status = job->view.result.status = JobStatus::failed;
                    job->view.result.reason = e.what();
                    job->view.finished_ms = now();
                    state_event(*job);
                    continue;
                }
                state_event(*job);
            }
            JobResult result;
            try {
                Lease lease(w.device.lock_key);
                if (!lease.owned) {
                    result.status = JobStatus::blocked;
                    result.reason = "Device lease is held by another backend process";
                } else {
                    RunContext ctx{job->stop.get_token(),
                                   [this, job](std::string json) {
                                       std::lock_guard lock(mutex);
                                       event(job->view.id, "runtime.event", std::move(json));
                                   },
                                   [this, job](std::filesystem::path p) {
                                       std::lock_guard lock(mutex);
                                       if (!trace_inside(p))
                                           throw std::invalid_argument("Trace escapes configured root");
                                       if (job->view.result.trace_directory != p) {
                                           job->view.result.trace_directory = std::move(p);
                                           save(job->view);
                                       }
                                   }};
                    result = executor(job->view.request, ctx);
                    if (!terminal(result.status) || result.status == JobStatus::interrupted ||
                        !object_json(result.detail_json) || result.detail_json.size() > 65536 ||
                        !trace_inside(result.trace_directory))
                        throw std::invalid_argument("Invalid executor result");
                }
            } catch (const std::exception &e) {
                result.status = JobStatus::failed;
                result.reason = e.what();
            } catch (...) {
                result.status = JobStatus::failed;
                result.reason = "Unhandled executor exception";
            }
            {
                std::lock_guard lock(mutex);
                if (result.trace_directory.empty())
                    result.trace_directory = job->view.result.trace_directory;
                job->view.result = std::move(result);
                job->view.status = job->view.result.status;
                job->view.finished_ms = now();
                try {
                    save(job->view);
                } catch (const std::exception &e) {
                    job->view.result.reason += "; terminal metadata persistence failed: ";
                    job->view.result.reason += e.what();
                }
                state_event(*job);
            }
        }
    }
};
JobHost::JobHost(std::vector<DeviceInfo> d, std::vector<TaskInfo> t, Executor e, HostOptions o)
    : impl_(std::make_unique<Impl>(std::move(d), std::move(t), std::move(e), std::move(o))) {}
JobHost::~JobHost() = default;
ar::core::Result<JobSnapshot> JobHost::submit(JobRequest r) {
    auto &i = *impl_;
    std::lock_guard lock(i.mutex);
    if (r.options_json.size() > 16384 || r.request_key.size() > 128 || !object_json(r.options_json))
        return failure(ErrorCode::invalid_argument, "host.submit", "Invalid options or request key");
    if (!r.request_key.empty())
        for (auto &[id, j] : i.retained)
            if (j->view.request.request_key == r.request_key) {
                auto &a = j->view.request;
                if (a.device_id != r.device_id || a.task_id != r.task_id || a.options_json != r.options_json)
                    return failure(ErrorCode::conflict, "host.submit",
                                   "Idempotency key reused with different invocation");
                return j->view;
            }
    if (i.stopping)
        return failure(ErrorCode::cancelled, "host.submit", "Host is stopping");
    auto found = i.workers.find(r.device_id);
    if (found == i.workers.end() || !found->second->device.enabled)
        return failure(ErrorCode::invalid_argument, "host.submit", "Device unavailable or disabled");
    auto &w = *found->second;
    if (std::find(w.device.tasks.begin(), w.device.tasks.end(), r.task_id) == w.device.tasks.end())
        return failure(ErrorCode::invalid_argument, "host.submit", "Task not registered for this device");
    if (w.queue.size() >= i.options.max_queued_per_device || i.retained.size() >= i.options.max_jobs)
        return failure(ErrorCode::budget_exceeded, "host.submit",
                       "Queue/history capacity reached; forget completed metadata explicitly");
    auto j = std::make_shared<Impl::Job>();
    j->view.id = i.instance + "-" + std::to_string(++i.counter);
    j->view.request = std::move(r);
    j->view.created_ms = now();
    try {
        i.save(j->view);
    } catch (const std::exception &e) {
        return failure(ErrorCode::io_failed, "host.submit", e.what());
    }
    i.retained.emplace(j->view.id, j);
    w.queue.push_back(j);
    i.state_event(*j);
    i.cv.notify_all();
    return j->view;
}
ar::core::Result<JobSnapshot> JobHost::get(std::string_view id) const {
    auto &i = *impl_;
    std::lock_guard lock(i.mutex);
    auto j = i.retained.find(std::string(id));
    if (j == i.retained.end())
        return failure(ErrorCode::invalid_argument, "host.get", "Unknown job");
    return j->second->view;
}
ar::core::Result<JobSnapshot> JobHost::cancel(std::string_view id) {
    auto &i = *impl_;
    std::lock_guard lock(i.mutex);
    auto it = i.retained.find(std::string(id));
    if (it == i.retained.end())
        return failure(ErrorCode::invalid_argument, "host.cancel", "Unknown job");
    auto &j = *it->second;
    if (terminal(j.view.status))
        return j.view;
    j.view.cancellation_requested = true;
    j.stop.request_stop();
    if (j.view.status == JobStatus::queued) {
        j.view.status = j.view.result.status = JobStatus::cancelled;
        j.view.finished_ms = now();
        j.view.result.reason = "Cancelled before execution";
        for (auto &[key, w] : i.workers)
            std::erase(w->queue, it->second);
    }
    try {
        i.save(j.view);
    } catch (const std::exception &e) {
        return failure(ErrorCode::io_failed, "host.cancel", e.what());
    }
    i.state_event(j);
    i.cv.notify_all();
    return j.view;
}
ar::core::Result<void> JobHost::forget(std::string_view id) {
    auto &i = *impl_;
    std::lock_guard lock(i.mutex);
    auto it = i.retained.find(std::string(id));
    if (it == i.retained.end())
        return failure(ErrorCode::invalid_argument, "host.forget", "Unknown job");
    if (!terminal(it->second->view.status))
        return failure(ErrorCode::conflict, "host.forget", "Cannot forget active job");
    std::error_code ec;
    std::filesystem::remove(i.metadata(it->first), ec);
    if (ec)
        return failure(ErrorCode::io_failed, "host.forget", "Cannot remove metadata");
    i.event(it->first, "host.forgotten", "{}");
    i.retained.erase(it);
    return {};
}
std::vector<JobSnapshot> JobHost::jobs() const {
    auto &i = *impl_;
    std::lock_guard lock(i.mutex);
    std::vector<JobSnapshot> result;
    for (auto &[id, j] : i.retained)
        result.push_back(j->view);
    std::sort(result.begin(), result.end(),
              [](auto &a, auto &b) { return std::tie(a.created_ms, a.id) < std::tie(b.created_ms, b.id); });
    return result;
}
const std::vector<DeviceInfo> &JobHost::devices() const {
    return impl_->devices;
}
const std::vector<TaskInfo> &JobHost::tasks() const {
    return impl_->tasks;
}
const std::string &JobHost::instance_id() const {
    return impl_->instance;
}
EventPage JobHost::events(uint64_t after, size_t limit) const {
    auto &i = *impl_;
    std::lock_guard lock(i.mutex);
    EventPage p;
    p.instance_id = i.instance;
    p.last_sequence = i.sequence;
    p.first_sequence = i.ring.empty() ? i.sequence + 1 : i.ring.front().sequence;
    p.gap = after > i.sequence || (after < p.first_sequence && p.first_sequence - after > 1);
    const auto cursor = after > i.sequence ? uint64_t(0) : after;
    p.next_sequence = cursor;
    for (auto &e : i.ring)
        if (e.sequence > cursor && p.events.size() < std::clamp<size_t>(limit, 1, 500)) {
            p.events.push_back(e);
            p.next_sequence = e.sequence;
        }
    return p;
}
void JobHost::shutdown() {
    impl_->stop();
}
} // namespace ar::host
