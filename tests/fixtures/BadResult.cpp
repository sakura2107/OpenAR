#include "plugin/PluginApi.h"
#include <limits>
ARFW_PLUGIN_EXPORT int32_t arfw_plugin_abi_version() {
    return 1;
}
ARFW_PLUGIN_EXPORT const char *arfw_plugin_backend_name() {
    return "fixture-bad-result";
}
ARFW_PLUGIN_EXPORT void *arfw_matcher_create(arfw_log_fn, void *) {
    return reinterpret_cast<void *>(1);
}
ARFW_PLUGIN_EXPORT void arfw_matcher_destroy(void *) {}
ARFW_PLUGIN_EXPORT int32_t arfw_matcher_find(void *, const uint8_t *, int64_t, int32_t, int32_t, int32_t,
                                             const uint8_t *, int64_t, int32_t, int32_t, float, int32_t,
                                             int32_t, int32_t, int32_t, float *score, int32_t *x, int32_t *y,
                                             char *, int32_t) {
    *score = std::numeric_limits<float>::quiet_NaN();
    *x = 1;
    *y = 1;
    return 0;
}
