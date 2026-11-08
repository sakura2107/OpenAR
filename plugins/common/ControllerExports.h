#pragma once
#include "Exports.h"
#include "control/Controller.h"
namespace {
struct ControllerInstance {
    ar::plugin_support::LogBridge bridge;
    std::unique_ptr<ar::control::IController> engine;
    ar::core::Error error;
};
void *make_controller(const ArfwControllerConfig *base, const char *serial, uint32_t timeout, arfw_log_fn log,
                      void *user, char *error, int32_t size) noexcept {
    void *result = nullptr;
    ar::plugin_support::guarded(
        [&]() -> int32_t {
            if (!base || base->adb_port < 0 || base->adb_port > 65535 || !timeout || timeout > 3600000)
                throw std::invalid_argument("Invalid controller config");
            auto instance = std::make_unique<ControllerInstance>();
            instance->bridge.fn = log;
            instance->bridge.user = user;
            ar::control::ControllerConfig config;
            config.adb_path = ar::plugin_support::path(base->adb_path);
            config.adb_port = static_cast<uint16_t>(base->adb_port);
            config.mumu_path = ar::plugin_support::path(base->mumu_path);
            config.mumu_index = base->mumu_index;
            config.serial = serial ? serial : "";
            config.command_timeout_ms = timeout;
            config.logger = &instance->bridge;
            instance->engine = std::make_unique<OPENAR_CONTROLLER_TYPE>(config);
            result = instance.release();
            return ARFW_PLUGIN_OK;
        },
        error, size);
    return result;
}
template <class Fn> int32_t controller_call(void *handle, Fn &&fn, bool preserve_error = false) noexcept {
    auto *instance = static_cast<ControllerInstance *>(handle);
    if (!instance)
        return ARFW_PLUGIN_ERR_ARGS;
    try {
        if (fn(*instance->engine))
            return ARFW_PLUGIN_OK;
        if (!preserve_error)
            instance->error = instance->engine->last_error();
        return ARFW_PLUGIN_ERR_FAILED;
    } catch (const std::exception &e) {
        instance->error = ar::core::failure(ar::core::ErrorCode::backend_failed, "controller", e.what(),
                                            OPENAR_CONTROLLER_NAME);
    } catch (...) {
        instance->error = ar::core::failure(ar::core::ErrorCode::backend_failed, "controller",
                                            "Unknown plugin exception", OPENAR_CONTROLLER_NAME);
    }
    return ARFW_PLUGIN_ERR_FAILED;
}
} // namespace
ARFW_PLUGIN_EXPORT int32_t arfw_plugin_abi_version() {
    return 1;
}
ARFW_PLUGIN_EXPORT const char *arfw_plugin_backend_name() {
    return OPENAR_CONTROLLER_NAME;
}
ARFW_PLUGIN_EXPORT const ArfwPluginDescriptorV2 *arfw_plugin_descriptor_v2() {
    static const ArfwPluginDescriptorV2 value{sizeof(value), 2, OPENAR_CONTROLLER_NAME,
                                              ARFW_FAMILY_CONTROLLER, ARFW_CAP_CONFIG | ARFW_CAP_CONTEXT};
    return &value;
}
ARFW_PLUGIN_EXPORT void *arfw_controller_create(const ArfwControllerConfig *config, arfw_log_fn log,
                                                void *user) {
    char error[512]{};
    auto *result = make_controller(config, nullptr, 30000, log, user, error, sizeof(error));
    if (!result && log)
        log(user, 3, error);
    return result;
}
ARFW_PLUGIN_EXPORT void *arfw_controller_create_v2(const ArfwControllerConfigV2 *config, arfw_log_fn log,
                                                   void *user, char *error, int32_t size) {
    if (!config || config->struct_size < sizeof(*config)) {
        ar::plugin_support::error_text(error, size, "Invalid controller config size");
        return nullptr;
    }
    return make_controller(&config->base, config->serial, config->command_timeout_ms, log, user, error, size);
}
ARFW_PLUGIN_EXPORT void arfw_controller_destroy(void *handle) {
    delete static_cast<ControllerInstance *>(handle);
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_connect(void *handle) {
    return controller_call(handle, [](auto &c) { return c.connect(); });
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_disconnect(void *handle) {
    return controller_call(handle, [](auto &c) { return c.disconnect(); });
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_click(void *handle, int32_t x, int32_t y) {
    return controller_call(handle, [&](auto &c) { return x >= 0 && y >= 0 && c.click({x, y}); });
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_long_press(void *handle, int32_t x, int32_t y, int32_t ms) {
    return controller_call(handle, [&](auto &c) {
        return x >= 0 && y >= 0 && ms >= 0 && ms <= 3600000 && c.long_press({x, y}, ms);
    });
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_swipe(void *handle, int32_t x, int32_t y, int32_t x2, int32_t y2,
                                                 int32_t ms) {
    return controller_call(handle, [&](auto &c) {
        return x >= 0 && y >= 0 && x2 >= 0 && y2 >= 0 && ms >= 0 && ms <= 3600000 &&
               c.swipe({x, y}, {x2, y2}, ms);
    });
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_press_key(void *handle, int32_t key) {
    return controller_call(handle, [&](auto &c) { return key >= 0 && c.press_key(key); });
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_set_context_v2(void *handle, arfw_cancel_fn cancel, void *user,
                                                          uint32_t timeout) {
    return ar::plugin_support::guarded([&]() -> int32_t {
        auto *instance = static_cast<ControllerInstance *>(handle);
        if (!instance || !timeout)
            return ARFW_PLUGIN_ERR_ARGS;
        instance->engine->set_context([cancel, user] { return cancel && cancel(user) != 0; }, timeout);
        return ARFW_PLUGIN_OK;
    });
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_last_error_v2(void *handle, char *error, int32_t size) {
    auto *instance = static_cast<ControllerInstance *>(handle);
    if (!instance)
        return static_cast<int32_t>(ar::core::ErrorCode::invalid_argument);
    ar::plugin_support::error_text(error, size, instance->error.message.c_str());
    return static_cast<int32_t>(instance->error.code);
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_error_v2(void *handle, ArfwErrorV2 *output) {
    auto *instance = static_cast<ControllerInstance *>(handle);
    if (!instance || !output || output->struct_size < sizeof(*output))
        return ARFW_PLUGIN_ERR_ARGS;
    const auto &error = instance->error;
    *output = {sizeof(*output), static_cast<int32_t>(error.code), error.native_code, error.operation.c_str(),
               error.message.c_str()};
    return ARFW_PLUGIN_OK;
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_start_app_v2(void *handle, const char *package) {
    return controller_call(handle, [&](auto &c) { return package && c.start_app(package); });
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_stop_app_v2(void *handle, const char *package) {
    return controller_call(handle, [&](auto &c) { return package && c.stop_app(package); });
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_screenshot(void *handle, int32_t *w, int32_t *h, int32_t *channels,
                                                      uint8_t **data) {
    if (data)
        *data = nullptr;
    return controller_call(
        handle,
        [&](auto &c) {
            if (!w || !h || !channels || !data)
                throw std::invalid_argument("Missing screenshot output");
            auto capture = c.capture_checked();
            if (!capture) {
                static_cast<ControllerInstance *>(handle)->error = capture.error();
                return false;
            }
            const auto &image = capture.value();
            const auto bytes = image.total() * image.elemSize();
            std::unique_ptr<uint8_t, decltype(&std::free)> buffer(static_cast<uint8_t *>(std::malloc(bytes)),
                                                                  &std::free);
            if (!buffer)
                throw std::bad_alloc();
            const auto row = size_t(image.cols) * image.elemSize();
            for (int y = 0; y < image.rows; ++y)
                std::memcpy(buffer.get() + y * row, image.ptr(y), row);
            *w = image.cols;
            *h = image.rows;
            *channels = image.channels();
            *data = buffer.release();
            return true;
        },
        true);
}
ARFW_PLUGIN_EXPORT void arfw_controller_free_image(void *, uint8_t *data) {
    std::free(data);
}
ARFW_PLUGIN_EXPORT int32_t arfw_controller_screenshot_v2(void *handle, ArfwImageViewV2 *view) {
    if (!view || view->struct_size < sizeof(*view))
        return ARFW_PLUGIN_ERR_ARGS;
    view->data = nullptr;
    int32_t w = 0, h = 0, channels = 0;
    uint8_t *data = nullptr;
    const auto status = arfw_controller_screenshot(handle, &w, &h, &channels, &data);
    view->width = w;
    view->height = h;
    view->channels = channels;
    view->data = data;
    view->stride = int64_t(w) * channels;
    view->byte_length = uint64_t(w) * h * channels;
    return status;
}
