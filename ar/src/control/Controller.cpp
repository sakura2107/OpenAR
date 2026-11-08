#include "control/Controller.h"

#include <format>
#include <string>
#include <utility>

namespace ar::control {

std::shared_ptr<IController> create_controller(std::string_view backend, const ControllerConfig &config) {
    auto controller = ControllerRegistry::instance().create(backend, config);
    if (!controller && config.logger) {
        std::string available;
        ControllerRegistry::instance().for_each_name([&available](std::string_view name) {
            available += (available.empty() ? "" : ", ");
            available += name;
        });
        if (ControllerRegistry::instance().contains(backend)) {
            config.logger->log(ar::log::Level::error,
                               "Controller initialization failed: " + std::string(backend));
            return nullptr;
        }
        config.logger->log(
            ar::log::Level::error,
            std::format("Unknown controller backend '{}' (available: {})", backend, available));
    }
    return std::shared_ptr<IController>(std::move(controller));
}

std::vector<std::string_view> controller_backends() {
    std::vector<std::string_view> names;
    ControllerRegistry::instance().for_each_name([&names](std::string_view name) { names.push_back(name); });
    return names;
}

} // namespace ar::control
