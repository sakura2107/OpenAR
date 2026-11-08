#include "plugin/PluginApi.h"
ARFW_PLUGIN_EXPORT int32_t arfw_plugin_abi_version() {
    return 99;
}
ARFW_PLUGIN_EXPORT const char *arfw_plugin_backend_name() {
    return "fixture-bad-abi";
}
