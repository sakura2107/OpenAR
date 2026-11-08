#pragma once
#include "core/Result.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <stop_token>
#include <vector>

namespace ar::host {
enum class JobStatus { queued, running, succeeded, failed, cancelled, blocked, outcome_unknown, interrupted };
const char *status_name(JobStatus);
bool terminal(JobStatus);
struct DeviceInfo {
    std::string id, label, kind, lock_key;
    bool enabled = true;
    std::vector<std::string> tasks;
};
struct TaskInfo {
    std::string id, label;
};
struct JobRequest {
    std::string device_id, task_id, options_json = "{}", request_key;
};
struct JobResult {
    JobStatus status = JobStatus::failed;
    std::string reason, detail_json = "{}";
    std::filesystem::path trace_directory;
};
struct JobSnapshot {
    std::string id;
    JobRequest request;
    JobStatus status = JobStatus::queued;
    int64_t created_ms = 0, started_ms = 0, finished_ms = 0;
    bool cancellation_requested = false;
    JobResult result;
};
struct HostEvent {
    uint64_t sequence = 0;
    int64_t time_ms = 0;
    std::string job_id, type, payload_json;
};
struct EventPage {
    std::string instance_id;
    uint64_t first_sequence = 0, last_sequence = 0, next_sequence = 0;
    bool gap = false;
    std::vector<HostEvent> events;
};
struct RunContext {
    std::stop_token stop;
    std::function<void(std::string)> publish;
    std::function<void(std::filesystem::path)> trace;
};
using Executor = std::function<JobResult(const JobRequest &, RunContext &)>;
struct HostOptions {
    std::filesystem::path state_directory, trace_root;
    size_t max_jobs = 512, max_queued_per_device = 16, max_events = 2048;
    size_t max_event_bytes = 8 * 1024 * 1024;
};
// Generic job control only. Execution remains in the registered application runner.
// One worker/device; different devices can execute concurrently. No crash resume.
class JobHost {
  public:
    JobHost(std::vector<DeviceInfo>, std::vector<TaskInfo>, Executor, HostOptions);
    ~JobHost();
    JobHost(const JobHost &) = delete;
    JobHost &operator=(const JobHost &) = delete;
    ar::core::Result<JobSnapshot> submit(JobRequest);
    ar::core::Result<JobSnapshot> get(std::string_view id) const;
    ar::core::Result<JobSnapshot> cancel(std::string_view id);
    ar::core::Result<void> forget(std::string_view id);
    std::vector<JobSnapshot> jobs() const;
    const std::vector<DeviceInfo> &devices() const;
    const std::vector<TaskInfo> &tasks() const;
    EventPage events(uint64_t after, size_t limit = 100) const;
    const std::string &instance_id() const;
    void shutdown(); // cancels queued/running work and joins cooperative workers.
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
std::string snapshot_json(const JobSnapshot &);
} // namespace ar::host
