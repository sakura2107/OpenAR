#pragma once
#include <future>
#include <thread>
#include <unordered_map>
#include "runtime/Resource.h"
namespace ar::runtime {
struct RecognitionMatch {
    bool found = false;
    float score = 0;
    cv::Rect box;
    std::string text, backend;
};
struct Event {
    std::string run_id, device_id, resource, version, node, type, details;
    uint64_t frame_id = 0;
    int64_t time_ms = 0;
    std::optional<RecognitionMatch> match;
    std::optional<ar::core::Error> error;
};
class IEventSink {
  public:
    virtual ~IEventSink() = default;
    virtual void emit(const Event &event) noexcept = 0;
};
struct ExtensionContext {
    DeviceSession &session;
    const Frame &frame;
    IClock &clock;
    std::stop_token stop;
    int64_t deadline_ms;
    std::string_view params;
    ar::core::Result<void> check() const;
    uint32_t remaining_ms() const;
};
struct Extensions {
    std::unordered_map<std::string, std::function<ar::core::Result<RecognitionMatch>(ExtensionContext &)>>
        recognitions;
    std::unordered_map<std::string, std::function<ar::core::Result<void>(ExtensionContext &)>> actions;
};
enum class RunStatus { completed, failed, cancelled };
struct RunReport {
    RunStatus status = RunStatus::failed;
    std::string run_id, last_node;
    uint32_t transitions = 0, recoveries = 0, frames = 0;
    int64_t elapsed_ms = 0;
    std::optional<ar::core::Error> error, diagnostic_error;
    std::filesystem::path trace_directory;
};
struct TaskOptions {
    std::optional<RunLimits> limits;
    std::filesystem::path trace_directory;
    bool save_failure_snapshot = true;
};
class TaskRunner {
  public:
    static RunReport run(DeviceSession &session, const ResourceBundle &resource, std::string entry,
                         const Extensions &extensions = {}, std::stop_token stop = {},
                         const TaskOptions &options = {}, IEventSink *sink = nullptr);
};
class TaskJob {
  public:
    void cancel() { worker_.request_stop(); }
    RunReport wait() { return result_.get(); }
    bool ready() const { return result_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }

  private:
    friend class Tasker;
    std::shared_future<RunReport> result_;
    std::jthread worker_;
};
class Tasker {
  public:
    Tasker(std::shared_ptr<DeviceSession> session, std::shared_ptr<const ResourceBundle> resource,
           Extensions extensions = {});
    // Snapshots resources and extensions into the job; Tasker may be destroyed while it runs.
    std::shared_ptr<TaskJob> post_task(std::string entry = {}, TaskOptions options = {},
                                       std::shared_ptr<IEventSink> sink = {}) const;

  private:
    std::shared_ptr<DeviceSession> session_;
    std::shared_ptr<const ResourceBundle> resource_;
    Extensions extensions_;
};
} // namespace ar::runtime
