#include "plugin/PluginApi.h"
ARFW_PLUGIN_EXPORT int32_t arfw_plugin_abi_version() {
    return 1;
}
ARFW_PLUGIN_EXPORT const char *arfw_plugin_backend_name() {
    return "fixture-growing-ocr";
}
ARFW_PLUGIN_EXPORT void *arfw_ocr_create(const ArfwOcrParams *, arfw_log_fn, void *) {
    return reinterpret_cast<void *>(1);
}
ARFW_PLUGIN_EXPORT void arfw_ocr_destroy(void *) {}
ARFW_PLUGIN_EXPORT int32_t arfw_ocr_recognize(void *, const uint8_t *, int64_t, int32_t, int32_t, int32_t,
                                              int32_t, int32_t, int32_t, int32_t, ArfwOcrItem *output,
                                              int32_t, int32_t *count, char *, int32_t) {
    *count = output ? 2 : 1;
    return 0;
}
