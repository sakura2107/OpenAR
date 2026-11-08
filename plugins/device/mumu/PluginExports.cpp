#include "MuMuDeviceController.h"
#include "plugins/common/Exports.h"
namespace {
struct Instance {
    ar::plugin_support::LogBridge bridge;
    std::unique_ptr<ar::device::IDeviceController> controller;
};
template <class Fn> int32_t call(void *device, Fn &&fn) noexcept {
    try {
        auto *instance = static_cast<Instance *>(device);
        if (!instance)
            return static_cast<int32_t>(ar::device::Status::invalid_command);
        return static_cast<int32_t>(fn(*instance->controller));
    } catch (...) {
        return static_cast<int32_t>(ar::device::Status::unknown);
    }
}
bool valid_package(const char *package) {
    return package && *package &&
           std::string_view(package).find_first_not_of(
               "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._") == std::string_view::npos;
}
} // namespace
ARFW_PLUGIN_EXPORT int32_t arfw_plugin_abi_version() {
    return 1;
}
ARFW_PLUGIN_EXPORT const char *arfw_plugin_backend_name() {
    return "mumu";
}
ARFW_PLUGIN_EXPORT const ArfwPluginDescriptorV2 *arfw_plugin_descriptor_v2() {
    static const ArfwPluginDescriptorV2 d{sizeof(d), 2, "mumu", ARFW_FAMILY_DEVICE, 0};
    return &d;
}
ARFW_PLUGIN_EXPORT void *arfw_device_create(const char *path, int32_t index, arfw_log_fn log, void *user) {
    try {
        if (index < 0)
            return nullptr;
        auto instance = std::make_unique<Instance>();
        instance->bridge.fn = log;
        instance->bridge.user = user;
        instance->controller = std::make_unique<ar::device::MuMuDeviceController>(
            ar::plugin_support::path(path), index, &instance->bridge);
        return instance.release();
    } catch (...) {
        return nullptr;
    }
}
ARFW_PLUGIN_EXPORT void arfw_device_destroy(void *device) {
    delete static_cast<Instance *>(device);
}
ARFW_PLUGIN_EXPORT int32_t arfw_device_check_state(void *d) {
    return call(d, [](auto &c) { return c.check_device_state(); });
}
ARFW_PLUGIN_EXPORT int32_t arfw_device_launch(void *d) {
    return call(d, [](auto &c) { return c.launch(); });
}
ARFW_PLUGIN_EXPORT int32_t arfw_device_shutdown(void *d) {
    return call(d, [](auto &c) { return c.shutdown(); });
}
ARFW_PLUGIN_EXPORT int32_t arfw_device_restart(void *d) {
    return call(d, [](auto &c) { return c.restart(); });
}
ARFW_PLUGIN_EXPORT int32_t arfw_device_show_window(void *d) {
    return call(d, [](auto &c) { return c.show_window(); });
}
ARFW_PLUGIN_EXPORT int32_t arfw_device_hide_window(void *d) {
    return call(d, [](auto &c) { return c.hide_window(); });
}
ARFW_PLUGIN_EXPORT int32_t arfw_device_launch_app(void *d, const char *p) {
    return call(
        d, [&](auto &c) { return valid_package(p) ? c.launch_app(p) : ar::device::Status::invalid_command; });
}
ARFW_PLUGIN_EXPORT int32_t arfw_device_close_app(void *d, const char *p) {
    return call(
        d, [&](auto &c) { return valid_package(p) ? c.close_app(p) : ar::device::Status::invalid_command; });
}
ARFW_PLUGIN_EXPORT int32_t arfw_device_app_state(void *d, const char *p, ArfwAppState *out) {
    return ar::plugin_support::guarded([&]() -> int32_t {
        if (!d || !valid_package(p) || !out)
            return ARFW_PLUGIN_ERR_ARGS;
        auto state = static_cast<Instance *>(d)->controller->app_state(p);
        *out = {state.installed ? 1 : 0, state.running ? 1 : 0};
        return 0;
    });
}
