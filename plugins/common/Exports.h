#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include "core/Image.h"
#include "core/Str.hpp"
#include "log/Logger.h"
#include "plugin/PluginApiV2.h"
namespace ar::plugin_support {
inline void error_text(char *buffer, int32_t size, const char *message) noexcept {
    if (buffer && size > 0)
        std::snprintf(buffer, static_cast<size_t>(size), "%s", message ? message : "Unknown error");
}
template <class Fn> int32_t guarded(Fn &&fn, char *buffer = nullptr, int32_t size = 0) noexcept {
    try {
        return fn();
    } catch (const std::invalid_argument &e) {
        error_text(buffer, size, e.what());
        return ARFW_PLUGIN_ERR_ARGS;
    } catch (const std::exception &e) {
        error_text(buffer, size, e.what());
        return ARFW_PLUGIN_ERR_FAILED;
    } catch (...) {
        error_text(buffer, size, "Unknown plugin exception");
        return ARFW_PLUGIN_ERR_FAILED;
    }
}
struct LogBridge final : ar::log::ILogger {
    arfw_log_fn fn = nullptr;
    void *user = nullptr;
    void log(ar::log::Level level, std::string_view message) noexcept override {
        if (!fn)
            return;
        try {
            const std::string text(message);
            fn(user, static_cast<int32_t>(level), text.c_str());
        } catch (...) {
        }
    }
    bool enabled(ar::log::Level) const noexcept override { return fn != nullptr; }
};
inline cv::Mat image(const uint8_t *data, int64_t stride, int32_t w, int32_t h, int32_t channels) {
    if (!data || w <= 0 || h <= 0 || (channels != 1 && channels != 3 && channels != 4) ||
        stride < int64_t(w) * channels || stride > INT32_MAX ||
        uint64_t(w) * h * channels > 256ull * 1024 * 1024)
        throw std::invalid_argument("Invalid image or row stride");
    return cv::Mat(h, w, CV_MAKETYPE(CV_8U, channels), const_cast<uint8_t *>(data),
                   static_cast<size_t>(stride));
}
inline cv::Mat image(const ArfwImageViewV2 *view) {
    if (!view || view->struct_size < sizeof(*view))
        throw std::invalid_argument("Invalid image descriptor");
    auto result = image(view->data, view->stride, view->width, view->height, view->channels);
    if (uint64_t(view->height - 1) * view->stride + uint64_t(view->width) * view->channels >
        view->byte_length)
        throw std::invalid_argument("Image buffer is shorter than its geometry");
    return result;
}
inline std::filesystem::path path(const char *text) {
    return text ? std::filesystem::path(ar::core::utf8_to_wide(text)) : std::filesystem::path{};
}
} // namespace ar::plugin_support
