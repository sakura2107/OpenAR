#include "plugin/PluginApiV2.h"
#include <string>
namespace {
int runs = 0, alive = 0;
float merge = 0;
struct Result {
    std::string text;
    int x = 0, y = 0;
};
} // namespace
ARFW_PLUGIN_EXPORT int32_t arfw_plugin_abi_version() {
    return 1;
}
ARFW_PLUGIN_EXPORT const char *arfw_plugin_backend_name() {
    return "fixture-ocr-once";
}
ARFW_PLUGIN_EXPORT const ArfwPluginDescriptorV2 *arfw_plugin_descriptor_v2() {
    static const ArfwPluginDescriptorV2 d{sizeof(d), 2, "fixture-ocr-once", ARFW_FAMILY_OCR,
                                          ARFW_CAP_CONFIG | ARFW_CAP_OCR_RESULT};
    return &d;
}
ARFW_PLUGIN_EXPORT void *arfw_ocr_create(const ArfwOcrParams *, arfw_log_fn, void *) {
    return reinterpret_cast<void *>(1);
}
ARFW_PLUGIN_EXPORT void *arfw_ocr_create_v2(const ArfwOcrParamsV2 *p, arfw_log_fn, void *, char *, int32_t) {
    merge = p->det_line_merge_ratio;
    return reinterpret_cast<void *>(1);
}
ARFW_PLUGIN_EXPORT void arfw_ocr_destroy(void *) {}
ARFW_PLUGIN_EXPORT int32_t arfw_ocr_recognize(void *, const uint8_t *, int64_t, int32_t, int32_t, int32_t,
                                              int32_t, int32_t, int32_t, int32_t, ArfwOcrItem *, int32_t,
                                              int32_t *, char *, int32_t) {
    return 2;
}
ARFW_PLUGIN_EXPORT int32_t arfw_ocr_run_v2(void *, const ArfwImageViewV2 *, const ArfwRectV2 *roi, void **out,
                                           char *, int32_t) {
    auto *r = new Result;
    for (int i = 0; i < 400; ++i)
        r->text += "中文";
    if (roi) {
        r->x = roi->x;
        r->y = roi->y;
    }
    *out = r;
    ++runs;
    ++alive;
    return 0;
}
ARFW_PLUGIN_EXPORT int32_t arfw_ocr_result_count_v2(void *) {
    return 1;
}
ARFW_PLUGIN_EXPORT int32_t arfw_ocr_result_item_v2(void *r, int32_t, ArfwOcrItemV2 *out) {
    auto &value = *static_cast<Result *>(r);
    *out = {sizeof(*out), value.text.c_str(), value.text.size(), 0.99f, {value.x, value.y, 10, 10}};
    return 0;
}
ARFW_PLUGIN_EXPORT void arfw_ocr_result_destroy_v2(void *r) {
    delete static_cast<Result *>(r);
    --alive;
}
ARFW_PLUGIN_EXPORT int32_t fixture_runs() {
    return runs;
}
ARFW_PLUGIN_EXPORT int32_t fixture_alive() {
    return alive;
}
ARFW_PLUGIN_EXPORT float fixture_merge() {
    return merge;
}
