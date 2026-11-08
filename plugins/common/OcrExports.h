#pragma once
#include "Exports.h"
#include "../ocr/paddle_common/PaddleEngine.h"
namespace {
using ar::plugin_support::guarded;
struct OcrInstance {
    ar::plugin_support::LogBridge bridge;
    std::unique_ptr<ar::paddle::PaddleOcrEngine> engine;
};
struct OcrResult {
    std::vector<ar::vision::OcrItem> items;
};
ar::vision::OcrParams ocr_params(const ArfwOcrParams &params, float merge) {
    ar::vision::OcrParams value;
    value.det_model = params.det_model ? params.det_model : "";
    value.rec_model = params.rec_model ? params.rec_model : "";
    value.dictionary = params.dictionary ? params.dictionary : "";
    value.text_threshold = params.text_threshold;
    value.box_threshold = params.box_threshold;
    value.unclip_ratio = params.unclip_ratio;
    value.score_mode = params.score_mode != 0;
    value.device_id = params.device_id;
    value.det_line_merge_ratio = merge;
    if (!std::isfinite(value.text_threshold) || value.text_threshold < 0 || value.text_threshold > 1 ||
        !std::isfinite(value.box_threshold) || value.box_threshold < 0 || value.box_threshold > 1 ||
        !std::isfinite(value.unclip_ratio) || value.unclip_ratio <= 0 || !std::isfinite(merge) || merge < 0)
        throw std::invalid_argument("Invalid OCR parameters");
    return value;
}
void *make_ocr(const ArfwOcrParams *params, float merge, arfw_log_fn log, void *user, char *error,
               int32_t size, const char *profile = nullptr, int device_id = -1) noexcept {
    void *output = nullptr;
    guarded(
        [&]() -> int32_t {
            if (!params)
                throw std::invalid_argument("Missing OCR parameters");
            auto instance = std::make_unique<OcrInstance>();
            instance->bridge.fn = log;
            instance->bridge.user = user;
            auto value = ocr_params(*params, merge);
            if (profile) { value.profile = profile; value.device_id = device_id; }
            instance->engine = std::make_unique<ar::paddle::PaddleOcrEngine>(
                OPENAR_OCR_NAME, value, OPENAR_OCR_DEVICE, &instance->bridge);
            output = instance.release();
            return ARFW_PLUGIN_OK;
        },
        error, size);
    return output;
}
} // namespace
ARFW_PLUGIN_EXPORT int32_t arfw_plugin_abi_version() {
    return 1;
}
ARFW_PLUGIN_EXPORT const char *arfw_plugin_backend_name() {
    return OPENAR_OCR_NAME;
}
ARFW_PLUGIN_EXPORT const ArfwPluginDescriptorV2 *arfw_plugin_descriptor_v2() {
    static const ArfwPluginDescriptorV2 value{sizeof(value), 2, OPENAR_OCR_NAME, ARFW_FAMILY_OCR,
                                              ARFW_CAP_CONFIG | ARFW_CAP_OCR_RESULT};
    return &value;
}
ARFW_PLUGIN_EXPORT void *arfw_ocr_create(const ArfwOcrParams *params, arfw_log_fn log, void *user) {
    char error[512]{};
    auto *output = make_ocr(params, 0.5f, log, user, error, sizeof(error));
    if (!output && log)
        log(user, 3, error);
    return output;
}
ARFW_PLUGIN_EXPORT void *arfw_ocr_create_v2(const ArfwOcrParamsV2 *params, arfw_log_fn log, void *user,
                                            char *error, int32_t size) {
    if (!params || params->struct_size < sizeof(*params)) {
        ar::plugin_support::error_text(error, size, "Invalid OCR config");
        return nullptr;
    }
    return make_ocr(&params->base, params->det_line_merge_ratio, log, user, error, size);
}
ARFW_PLUGIN_EXPORT void arfw_ocr_destroy(void *handle) {
    delete static_cast<OcrInstance *>(handle);
}
// Additive exports: v1/v2 layouts and descriptors stay unchanged.
ARFW_PLUGIN_EXPORT void *arfw_ocr_create_profile_v3(const char *profile, int32_t device_id,
                                                    arfw_log_fn log, void *user, char *error, int32_t size) {
    if (!profile || !*profile) { ar::plugin_support::error_text(error, size, "Missing OCR profile"); return nullptr; }
    ArfwOcrParams empty{};
    empty.text_threshold = empty.box_threshold = 0.5f; empty.unclip_ratio = 1.5f;
    return make_ocr(&empty, 0, log, user, error, size, profile, device_id);
}
ARFW_PLUGIN_EXPORT int32_t arfw_ocr_run_mode_v3(void *handle, const ArfwImageViewV2 *view,
                                               const ArfwRectV2 *range, int32_t mode, void **output,
                                               double *detection_ms, double *recognition_ms, char *error, int32_t size) {
    if (output) *output = nullptr;
    if (detection_ms) *detection_ms = 0;
    if (recognition_ms) *recognition_ms = 0;
    return guarded([&]() -> int32_t {
        auto *instance = static_cast<OcrInstance *>(handle);
        if (!instance || !output || !detection_ms || !recognition_ms) return ARFW_PLUGIN_ERR_ARGS;
        auto image = ar::plugin_support::image(view);
        cv::Rect roi;
        if (range) { roi = {range->x, range->y, range->width, range->height};
            if (!ar::core::valid_roi(roi, image.size())) throw std::invalid_argument("OCR ROI outside image"); }
        auto result = std::make_unique<OcrResult>();
        result->items = instance->engine->recognize_mode(image, range ? &roi : nullptr, mode, *detection_ms, *recognition_ms);
        *output = result.release();
        return ARFW_PLUGIN_OK;
    }, error, size);
}
ARFW_PLUGIN_EXPORT int32_t arfw_ocr_run_v2(void *handle, const ArfwImageViewV2 *view, const ArfwRectV2 *range,
                                           void **output, char *error, int32_t size) {
    if (output)
        *output = nullptr;
    return guarded(
        [&]() -> int32_t {
            auto *instance = static_cast<OcrInstance *>(handle);
            if (!instance || !output)
                return ARFW_PLUGIN_ERR_ARGS;
            auto frame = ar::plugin_support::image(view);
            cv::Rect roi;
            if (range) {
                roi = {range->x, range->y, range->width, range->height};
                if (!ar::core::valid_roi(roi, frame.size()))
                    throw std::invalid_argument("OCR ROI outside image");
            }
            auto result = std::make_unique<OcrResult>();
            result->items = instance->engine->recognize(frame, range ? &roi : nullptr);
            *output = result.release();
            return ARFW_PLUGIN_OK;
        },
        error, size);
}
ARFW_PLUGIN_EXPORT int32_t arfw_ocr_result_count_v2(void *handle) {
    auto *result = static_cast<OcrResult *>(handle);
    return result && result->items.size() <= INT32_MAX ? static_cast<int32_t>(result->items.size()) : -1;
}
ARFW_PLUGIN_EXPORT int32_t arfw_ocr_result_item_v2(void *handle, int32_t index, ArfwOcrItemV2 *out) {
    return guarded([&]() -> int32_t {
        auto *result = static_cast<OcrResult *>(handle);
        if (!result || !out || out->struct_size < sizeof(*out) || index < 0 ||
            size_t(index) >= result->items.size())
            return ARFW_PLUGIN_ERR_ARGS;
        const auto &item = result->items[index];
        out->text = item.text.data();
        out->text_length = item.text.size();
        out->score = item.score;
        out->box = {item.box.x, item.box.y, item.box.width, item.box.height};
        return ARFW_PLUGIN_OK;
    });
}
ARFW_PLUGIN_EXPORT void arfw_ocr_result_destroy_v2(void *handle) {
    delete static_cast<OcrResult *>(handle);
}
ARFW_PLUGIN_EXPORT int32_t arfw_ocr_recognize(void *handle, const uint8_t *data, int64_t stride, int32_t w,
                                              int32_t h, int32_t channels, int32_t x, int32_t y, int32_t rw,
                                              int32_t rh, ArfwOcrItem *items, int32_t capacity,
                                              int32_t *count, char *error, int32_t size) {
    if (count)
        *count = 0;
    return guarded(
        [&]() -> int32_t {
            auto *instance = static_cast<OcrInstance *>(handle);
            if (!instance || !count || capacity < 0 || (capacity > 0 && !items))
                return ARFW_PLUGIN_ERR_ARGS;
            auto frame = ar::plugin_support::image(data, stride, w, h, channels);
            cv::Rect roi(x, y, rw, rh);
            const auto *range = rw == 0 && rh == 0 ? nullptr : &roi;
            if (range && !ar::core::valid_roi(roi, frame.size()))
                throw std::invalid_argument("OCR ROI outside image");
            const auto result = instance->engine->recognize(frame, range);
            *count = static_cast<int32_t>(result.size());
            for (int32_t i = 0; i < std::min(capacity, *count); ++i) {
                const auto &item = result[i];
                std::snprintf(items[i].text, sizeof(items[i].text), "%s", item.text.c_str());
                // Avoid leaving a partial UTF-8 codepoint in legacy fixed text.
                size_t len = std::strlen(items[i].text);
                if (item.text.size() >= sizeof(items[i].text)) {
                    while (len && (static_cast<unsigned char>(items[i].text[len - 1]) & 0xc0) == 0x80)
                        --len;
                    if (len && static_cast<unsigned char>(items[i].text[len - 1]) >= 0xc0)
                        --len;
                    items[i].text[len] = 0;
                }
                items[i].score = item.score;
                items[i].box_x = item.box.x;
                items[i].box_y = item.box.y;
                items[i].box_w = item.box.width;
                items[i].box_h = item.box.height;
            }
            return ARFW_PLUGIN_OK;
        },
        error, size);
}
