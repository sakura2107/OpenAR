#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/Registry.h"
#include "log/Logger.h"

namespace ar::device {

// Status values carry the same meaning as the old int32 return codes
// (documented in the original IDeviceController): ok=1, offline=2,
// unknown=0, invalid_command=-1, invalid_index=-2.
enum class Status : int32_t {
    ok = 1,
    offline = 2,
    unknown = 0,
    invalid_command = -1,
    invalid_index = -2,
};

struct AppState {
    bool installed = false;
    bool running = false;
};

// Device lifecycle interface (emulator instances). Backends ("mumu", ...)
// register themselves and are created by name.
class IDeviceController {
  public:
    virtual ~IDeviceController() = default;

    virtual std::string_view name() const = 0;

    virtual Status check_device_state() = 0;
    virtual Status launch() = 0;
    virtual Status shutdown() = 0;
    virtual Status restart() = 0;
    virtual Status show_window() = 0;
    virtual Status hide_window() = 0;
    virtual Status launch_app(std::string_view package) = 0;
    virtual Status close_app(std::string_view package) = 0;
    virtual AppState app_state(std::string_view package) = 0;
};

using DeviceControllerRegistry =
    ar::core::Registry<IDeviceController, const std::filesystem::path &, int32_t, ar::log::ILogger *>;

std::unique_ptr<IDeviceController> create_device_controller(std::string_view backend,
                                                            const std::filesystem::path &device_path,
                                                            int32_t index,
                                                            ar::log::ILogger *logger = nullptr);
std::vector<std::string_view> device_controller_backends();

} // namespace ar::device
