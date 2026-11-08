#include "plugin/PluginApi.h"
ARFW_PLUGIN_EXPORT int32_t arfw_plugin_abi_version() {
    return 1;
}
ARFW_PLUGIN_EXPORT const char *arfw_plugin_backend_name() {
    return "fixture-missing";
}
ARFW_PLUGIN_EXPORT void *arfw_controller_create(const ArfwControllerConfig *, arfw_log_fn, void *) {
    return reinterpret_cast<void *>(1);
}
