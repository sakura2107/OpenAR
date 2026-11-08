#pragma once
#include "Exports.h"
#include "vision/Matcher.h"
namespace {
using ar::plugin_support::guarded;
struct MatcherInstance {
    ar::plugin_support::LogBridge bridge;
    std::unique_ptr<OPENAR_MATCHER_TYPE> engine;
    int32_t limit = 0;
};
void *make_matcher(int32_t max_bytes, arfw_log_fn log, void *user, char *error, int32_t size) noexcept {
    void *result = nullptr;
    guarded(
        [&]() -> int32_t {
            if (max_bytes <= 0 || max_bytes > 256 * 1024 * 1024)
                throw std::invalid_argument("Invalid matcher byte limit");
            auto instance = std::make_unique<MatcherInstance>();
            instance->limit = max_bytes;
            instance->bridge.fn = log;
            instance->bridge.user = user;
            instance->engine = std::make_unique<OPENAR_MATCHER_TYPE>(max_bytes);
#if OPENAR_MATCHER_GPU
            std::string message;
            if (!instance->engine->ok(message))
                throw std::runtime_error(message);
#endif
            result = instance.release();
            return ARFW_PLUGIN_OK;
        },
        error, size);
    return result;
}
} // namespace
ARFW_PLUGIN_EXPORT int32_t arfw_plugin_abi_version() {
    return ARFW_PLUGIN_ABI_VERSION;
}
ARFW_PLUGIN_EXPORT const char *arfw_plugin_backend_name() {
    return OPENAR_MATCHER_NAME;
}
ARFW_PLUGIN_EXPORT const ArfwPluginDescriptorV2 *arfw_plugin_descriptor_v2() {
    static const ArfwPluginDescriptorV2 value{sizeof(value), 2, OPENAR_MATCHER_NAME, ARFW_FAMILY_MATCHER,
                                              ARFW_CAP_CONFIG};
    return &value;
}
ARFW_PLUGIN_EXPORT void *arfw_matcher_create(arfw_log_fn log, void *user) {
    char error[512]{};
    auto *result = make_matcher(4096 * 2160 * 3, log, user, error, sizeof(error));
    if (!result && log)
        log(user, 3, error);
    return result;
}
ARFW_PLUGIN_EXPORT void *arfw_matcher_create_v2(const ArfwMatcherConfigV2 *config, arfw_log_fn log,
                                                void *user, char *error, int32_t size) {
    if (!config || config->struct_size < sizeof(*config)) {
        ar::plugin_support::error_text(error, size, "Invalid matcher config");
        return nullptr;
    }
    return make_matcher(config->max_input_bytes, log, user, error, size);
}
ARFW_PLUGIN_EXPORT void arfw_matcher_destroy(void *handle) {
    delete static_cast<MatcherInstance *>(handle);
}
ARFW_PLUGIN_EXPORT int32_t arfw_matcher_prepare(void *handle, const uint8_t *data, int64_t stride, int32_t w,
                                                int32_t h, int32_t channels) {
    return guarded([&]() -> int32_t {
        if (!handle)
            return ARFW_PLUGIN_ERR_ARGS;
        ar::plugin_support::image(data, stride, w, h, channels);
        return ARFW_PLUGIN_OK;
    });
}
ARFW_PLUGIN_EXPORT int32_t arfw_matcher_find(void *handle, const uint8_t *data, int64_t stride, int32_t w,
                                             int32_t h, int32_t channels, const uint8_t *templ_data,
                                             int64_t templ_stride, int32_t templ_w, int32_t templ_h,
                                             float threshold, int32_t roi_x, int32_t roi_y, int32_t roi_w,
                                             int32_t roi_h, float *score, int32_t *x, int32_t *y, char *error,
                                             int32_t error_size) {
    return guarded(
        [&]() -> int32_t {
            auto *instance = static_cast<MatcherInstance *>(handle);
            if (!instance || !score || !x || !y)
                return ARFW_PLUGIN_ERR_ARGS;
            *score = -1;
            *x = -1;
            *y = -1;
            if ((channels != 1 && channels != 3) || !std::isfinite(threshold) || threshold < -1 ||
                threshold > 1)
                throw std::invalid_argument("Invalid matcher channels or threshold");
            auto frame = ar::plugin_support::image(data, stride, w, h, channels);
            auto templ = ar::plugin_support::image(templ_data, templ_stride, templ_w, templ_h, channels);
            if (!ar::core::validate_image(frame, instance->limit) ||
                !ar::core::validate_image(templ, instance->limit))
                throw std::invalid_argument("Matcher input exceeds configured byte budget");
            cv::Rect roi(roi_x, roi_y, roi_w, roi_h);
            const cv::Rect *range = roi_w == 0 && roi_h == 0 ? nullptr : &roi;
            if (range && !ar::core::valid_roi(roi, frame.size()))
                throw std::invalid_argument("ROI outside image");
            if (templ_w > (range ? roi_w : w) || templ_h > (range ? roi_h : h))
                return ARFW_PLUGIN_OK;
#if OPENAR_MATCHER_GPU
            OPENAR_MATCHER_RESULT result;
            std::string message;
            if (!instance->engine->find(data, stride, w, h, templ_data, templ_stride, templ_w, templ_h,
                                        channels, threshold, roi_x, roi_y, roi_w, roi_h, result, message))
                throw std::runtime_error(message);
            *score = result.score;
            *x = result.x;
            *y = result.y;
#else
            const auto result = instance->engine->find(frame, templ, threshold, range, &instance->bridge);
            *score = result.score;
            if (result.found) {
                *x = result.box.x;
                *y = result.box.y;
            }
#endif
            return ARFW_PLUGIN_OK;
        },
        error, error_size);
}
ARFW_PLUGIN_EXPORT int32_t arfw_matcher_find_v2(void *handle, const ArfwImageViewV2 *frame,
                                                const ArfwImageViewV2 *templ, float threshold,
                                                const ArfwRectV2 *roi, float *score, int32_t *x, int32_t *y,
                                                char *error, int32_t size) {
    return guarded(
        [&]() -> int32_t {
            ar::plugin_support::image(frame);
            ar::plugin_support::image(templ);
            if (frame->channels != templ->channels)
                throw std::invalid_argument("Template channels differ");
            return arfw_matcher_find(handle, frame->data, frame->stride, frame->width, frame->height,
                                     frame->channels, templ->data, templ->stride, templ->width, templ->height,
                                     threshold, roi ? roi->x : 0, roi ? roi->y : 0, roi ? roi->width : 0,
                                     roi ? roi->height : 0, score, x, y, error, size);
        },
        error, size);
}
