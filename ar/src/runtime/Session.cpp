#include "runtime/Session.h"
#include <cmath>
#include <stdexcept>
#include <map>
namespace ar::runtime {
using ar::core::ErrorCode;
using ar::core::failure;
namespace {
std::atomic<uint64_t> next_session{1};
std::shared_ptr<std::mutex> device_mutex(const std::string &id) {
    static std::mutex lock;
    static std::map<std::string, std::weak_ptr<std::mutex>> devices;
    std::lock_guard guard(lock);
    for (auto it = devices.begin(); it != devices.end();)
        if (it->second.expired())
            it = devices.erase(it);
        else
            ++it;
    auto &entry = devices[id];
    auto mutex = entry.lock();
    if (!mutex) {
        mutex = std::make_shared<std::mutex>();
        entry = mutex;
    }
    return mutex;
}
} // namespace
ar::core::Result<CoordinateMap> CoordinateMap::create(cv::Size original, const SessionConfig &config) {
    if (original.width <= 0 || original.height <= 0 || config.reference_size.width <= 0 ||
        config.reference_size.height <= 0 ||
        (config.rotation != 0 && config.rotation != 90 && config.rotation != 180 && config.rotation != 270))
        return failure(ErrorCode::invalid_argument, "coordinates", "Invalid dimensions or rotation");
    CoordinateMap map;
    map.original_ = original;
    map.reference_ = config.reference_size;
    map.rotation_ = config.rotation;
    const cv::Size rotated = (config.rotation == 90 || config.rotation == 270)
                                 ? cv::Size(original.height, original.width)
                                 : original;
    map.viewport_ = config.viewport.empty() ? cv::Rect(0, 0, rotated.width, rotated.height) : config.viewport;
    if (!ar::core::valid_roi(map.viewport_, rotated))
        return failure(ErrorCode::invalid_argument, "coordinates", "Viewport is outside rotated screenshot");
    const double aspect = double(map.viewport_.width) / map.viewport_.height,
                 reference = double(map.reference_.width) / map.reference_.height;
    if (std::abs(aspect / reference - 1) > 0.01)
        return failure(ErrorCode::unsupported, "coordinates",
                       "Screenshot " + std::to_string(original.width) + "x" +
                           std::to_string(original.height) + " cannot map to " +
                           std::to_string(config.reference_size.width) + "x" +
                           std::to_string(config.reference_size.height) +
                           "; configure rotation/viewport/resource profile");
    return map;
}
ar::core::Result<cv::Point> CoordinateMap::to_device(cv::Point p) const {
    if (p.x < 0 || p.y < 0 || p.x >= reference_.width || p.y >= reference_.height)
        return failure(ErrorCode::invalid_argument, "coordinates", "Target outside reference screen");
    cv::Point q(
        static_cast<int>(std::lround(viewport_.x + (p.x + 0.5) * viewport_.width / reference_.width - 0.5)),
        static_cast<int>(
            std::lround(viewport_.y + (p.y + 0.5) * viewport_.height / reference_.height - 0.5)));
    if (rotation_ == 90)
        q = {q.y, original_.height - 1 - q.x};
    else if (rotation_ == 180)
        q = {original_.width - 1 - q.x, original_.height - 1 - q.y};
    else if (rotation_ == 270)
        q = {original_.width - 1 - q.y, q.x};
    return q;
}
cv::Mat CoordinateMap::normalize(const cv::Mat &original) const {
    cv::Mat rotated = original;
    if (rotation_)
        cv::rotate(original, rotated,
                   rotation_ == 90    ? cv::ROTATE_90_CLOCKWISE
                   : rotation_ == 180 ? cv::ROTATE_180
                                      : cv::ROTATE_90_COUNTERCLOCKWISE);
    cv::Mat result;
    cv::resize(rotated(viewport_), result, reference_, 0, 0, cv::INTER_LINEAR);
    return result;
}
DeviceSession::DeviceSession(std::shared_ptr<ar::control::IController> controller, SessionConfig config,
                             std::shared_ptr<IClock> clock,
                             std::unique_ptr<ar::device::IDeviceController> lifecycle)
    : controller_(std::move(controller)), lifecycle_(std::move(lifecycle)), config_(std::move(config)),
      clock_(std::move(clock)), instance_id_(next_session.fetch_add(1)) {
    if (!controller_ || !clock_ || config_.device_id.empty())
        throw std::invalid_argument("Session needs a controller, clock and unique device ID");
    if (!config_.allow_legacy_controller && !controller_->supports_context())
        throw std::invalid_argument("Controller has no operation context; upgrade its plugin or explicitly "
                                    "set allow_legacy_controller");
    if (config_.reference_size.width <= 0 || config_.reference_size.height <= 0 ||
        uint64_t(config_.reference_size.width) * config_.reference_size.height > 16 * 1024 * 1024)
        throw std::invalid_argument("Invalid session reference size");
    run_mutex_ = device_mutex(config_.device_id);
}
DeviceSession::~DeviceSession() {
    try {
        controller_->set_context({}, 1000);
        controller_->disconnect();
    } catch (...) {
    }
}
ar::core::Result<std::unique_lock<std::mutex>> DeviceSession::acquire_run() {
    std::unique_lock lock(*run_mutex_, std::try_to_lock);
    if (!lock.owns_lock())
        return failure(ErrorCode::conflict, "task", "A task is already running on this device");
    return std::move(lock);
}
ar::core::Result<void> DeviceSession::check(std::stop_token stop, uint32_t timeout) const {
    if (stop.stop_requested())
        return failure(ErrorCode::cancelled, "session", "Task cancelled");
    if (!timeout)
        return failure(ErrorCode::timeout, "session", "Operation budget exhausted");
    return {};
}
ar::core::Result<void> DeviceSession::connect(std::stop_token stop, uint32_t timeout) {
    auto valid = check(stop, timeout);
    if (!valid)
        return valid;
    controller_->set_context([stop] { return stop.stop_requested(); }, timeout);
    if (connected_)
        return {};
    if (!controller_->connect())
        return controller_->last_error();
    connected_ = true;
    return {};
}
ar::core::Result<Frame> DeviceSession::capture(std::stop_token stop, uint32_t timeout) {
    std::lock_guard lock(operation_mutex_);
    try {
        const auto begin = clock_->now_ms();
        auto connection = connect(stop, timeout);
        if (!connection)
            return connection.error();
        const auto remaining = int64_t(timeout) - (clock_->now_ms() - begin);
        if (remaining <= 0)
            return failure(ErrorCode::timeout, "capture", "Connection exhausted capture budget");
        auto checked = check(stop, static_cast<uint32_t>(remaining));
        if (!checked)
            return checked.error();
        controller_->set_context([stop] { return stop.stop_requested(); }, static_cast<uint32_t>(remaining));
        auto capture = controller_->capture_checked();
        if (!capture) {
            connected_ = false;
            return capture.error();
        }
        if (stop.stop_requested())
            return failure(ErrorCode::cancelled, "capture", "Task cancelled");
        if (clock_->now_ms() - begin >= timeout)
            return failure(ErrorCode::timeout, "capture", "Capture exceeded its budget");
        auto mapping = CoordinateMap::create(capture.value().size(), config_);
        if (!mapping)
            return mapping.error();
        map_ = mapping.value();
        Frame frame;
        frame.id = ++frame_id_;
        frame.generation = generation_;
        frame.session_id = instance_id_;
        frame.device_id = config_.device_id;
        frame.captured_ms = clock_->now_ms();
        frame.original = capture.value().clone();
        frame.image = map_->normalize(frame.original);
        return frame;
    } catch (const std::exception &e) {
        return failure(ErrorCode::backend_failed, "capture", e.what());
    }
}
ar::core::Result<void> DeviceSession::validate(const Frame &frame) const {
    std::lock_guard lock(operation_mutex_);
    if (frame.session_id != instance_id_ || frame.device_id != config_.device_id ||
        frame.generation != generation_ || frame.id != frame_id_)
        return failure(ErrorCode::stale_frame, "frame",
                       "Recognition frame is no longer current for this device");
    return {};
}
ar::core::Result<void> DeviceSession::perform(const Action &action, const Frame *frame,
                                              std::optional<cv::Point> matched, std::stop_token stop,
                                              uint32_t timeout) {
    std::lock_guard lock(operation_mutex_);
    try {
        const auto begin = clock_->now_ms();
        auto budget = check(stop, timeout);
        if (!budget)
            return budget;
        if (action.type == ActionType::wait) {
            if (action.duration_ms < 0 || uint32_t(action.duration_ms) > timeout)
                return failure(ErrorCode::timeout, "wait", "Wait exceeds operation budget");
            if (action.duration_ms)
                ++generation_;
            return clock_->sleep_ms(action.duration_ms, stop);
        }
        if (action.type == ActionType::none)
            return {};
        auto connection = connect(stop, timeout);
        if (!connection)
            return connection;
        const auto remaining = int64_t(timeout) - (clock_->now_ms() - begin);
        if (remaining <= 0)
            return failure(ErrorCode::timeout, "action", "Connection exhausted action budget");
        auto checked = check(stop, static_cast<uint32_t>(remaining));
        if (!checked)
            return checked;
        controller_->set_context([stop] { return stop.stop_requested(); }, static_cast<uint32_t>(remaining));
        if (frame && (frame->session_id != instance_id_ || frame->device_id != config_.device_id ||
                      frame->generation != generation_ || frame->id != frame_id_))
            return failure(ErrorCode::stale_frame, "action", "Action references a stale frame");
        std::optional<cv::Point> p = action.target ? action.target : matched;
        cv::Point target, destination;
        if (action.type == ActionType::click || action.type == ActionType::long_press ||
            action.type == ActionType::swipe) {
            if (!p || !frame || !map_)
                return failure(ErrorCode::invalid_argument, "action",
                               "Input needs a current frame and target");
            auto mapped = map_->to_device(*p);
            if (!mapped)
                return mapped.error();
            target = mapped.value();
            if (action.type == ActionType::swipe) {
                auto to = map_->to_device(action.to);
                if (!to)
                    return to.error();
                destination = to.value();
            }
            if (action.duration_ms < 0 || uint32_t(action.duration_ms) > timeout)
                return failure(ErrorCode::invalid_argument, "action", "Input duration exceeds budget");
        }
        ++generation_; // Invalidate on attempted input, even when its outcome is uncertain.
        bool success = false;
        switch (action.type) {
        case ActionType::click:
            success = controller_->click(target);
            break;
        case ActionType::long_press:
            success = controller_->long_press(target, action.duration_ms);
            break;
        case ActionType::swipe:
            success = controller_->swipe(target, destination, action.duration_ms);
            break;
        case ActionType::key:
            success = controller_->press_key(action.keycode);
            break;
        case ActionType::start_app:
            success = controller_->start_app(action.package);
            break;
        case ActionType::stop_app:
            success = controller_->stop_app(action.package);
            break;
        case ActionType::reconnect:
            controller_->disconnect();
            connected_ = false;
            success = controller_->connect();
            connected_ = success;
            break;
        default:
            return failure(ErrorCode::unsupported, "action", "Action must be supplied by an extension");
        }
        if (stop.stop_requested())
            return failure(ErrorCode::cancelled, "action", "Task cancelled after action");
        if (clock_->now_ms() - begin >= timeout)
            return failure(ErrorCode::timeout, "action", "Action exceeded its budget");
        if (!success) {
            connected_ = false;
            return controller_->last_error();
        }
        return {};
    } catch (const std::exception &e) {
        connected_ = false;
        return failure(ErrorCode::backend_failed, "action", e.what());
    }
}
} // namespace ar::runtime
