#include "device/DeviceController.h"

#include <format>
#include <string>
#include <utility>

namespace ar::device {

std::unique_ptr<IDeviceController> create_device_controller(std::string_view backend,
                                                            const std::filesystem::path &device_path,
                                                            int32_t index, ar::log::ILogger *logger) {
    auto controller = DeviceControllerRegistry::instance().create(backend, device_path, index, logger);
    if (!controller && logger) {
        std::string available;
        DeviceControllerRegistry::instance().for_each_name([&available](std::string_view name) {
            available += (available.empty() ? "" : ", ");
            available += name;
        });
        logger->log(
            ar::log::Level::error,
            std::format("Unknown device controller backend '{}' (available: {})", backend, available));
    }
    return controller;
}

std::vector<std::string_view> device_controller_backends() {
    std::vector<std::string_view> names;
    DeviceControllerRegistry::instance().for_each_name(
        [&names](std::string_view name) { names.push_back(name); });
    return names;
}

} // namespace ar::device
