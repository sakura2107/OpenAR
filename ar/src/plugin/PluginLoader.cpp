#include "plugin/PluginLoader.h"
#include "PluginAdapters.hpp"
#include <mutex>
namespace ar::plugin {
namespace {
template <class T> T exported(HMODULE library, const char *symbol) {
    return reinterpret_cast<T>(GetProcAddress(library, symbol));
}
template <class Interface, class Adapter, class... Args>
std::unique_ptr<Interface> create_adapter(const std::shared_ptr<detail::Module> &module,
                                          ar::log::ILogger *logger, Args &&...args) {
    try {
        return std::make_unique<Adapter>(module, std::forward<Args>(args)...);
    } catch (const std::exception &e) {
        AR_LOG(logger, ar::log::Level::error,
               std::format("Backend '{}' initialization failed: {}", module->name, e.what()));
        return nullptr;
    }
}
bool required(ar::log::ILogger *logger, const std::string &name,
              std::initializer_list<std::pair<bool, const char *>> exports) {
    std::string missing;
    for (const auto &[present, symbol] : exports)
        if (!present) {
            if (!missing.empty())
                missing += ", ";
            missing += symbol;
        }
    if (missing.empty())
        return true;
    AR_LOG(logger, ar::log::Level::warning,
           std::format("Plugin '{}' rejected: missing exports {}", name, missing));
    return false;
}
} // namespace
std::vector<std::string> load_plugins(const std::filesystem::path &directory, ar::log::ILogger *logger) {
    static std::mutex load_mutex;
    std::lock_guard lock(load_mutex);
    std::vector<std::string> loaded;
    std::error_code error;
    const auto dir = std::filesystem::absolute(directory, error);
    if (error || !std::filesystem::is_directory(dir, error))
        return loaded;
    std::vector<std::filesystem::path> paths;
    for (std::filesystem::directory_iterator it(dir, error), end; !error && it != end; it.increment(error))
        if (it->is_regular_file(error) && it->path().extension() == L".dll")
            paths.push_back(it->path());
    std::sort(paths.begin(), paths.end());
    for (const auto &path : paths) {
        auto module = std::make_shared<detail::Module>();
        module->directory = AddDllDirectory(dir.parent_path().c_str());
        module->library = LoadLibraryExW(path.c_str(), nullptr,
                                         LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module->library) {
            AR_LOG(logger, ar::log::Level::warning,
                   std::format("Cannot load plugin {} (Win32 {})", ar::core::wide_to_utf8(path.wstring()),
                               GetLastError()));
            continue;
        }
        auto abi = exported<int32_t (*)()>(module->library, "arfw_plugin_abi_version");
        auto name = exported<const char *(*)()>(module->library, "arfw_plugin_backend_name");
        if (!abi || !name || abi() != 1) {
            AR_LOG(logger, ar::log::Level::warning, "Plugin has missing or incompatible v1 handshake");
            continue;
        }
        const char *text = name();
        if (!text || !*text || strnlen_s(text, 129) > 128) {
            AR_LOG(logger, ar::log::Level::warning, "Plugin backend name is invalid");
            continue;
        }
        module->name = text;
        auto descriptor =
            exported<const ArfwPluginDescriptorV2 *(*)()>(module->library, "arfw_plugin_descriptor_v2");
        if (descriptor) {
            const auto *value = descriptor();
            if (!value || value->struct_size < sizeof(*value) || value->abi_version != 2 ||
                !value->backend_name || module->name != value->backend_name || !value->families ||
                (value->families & ~15u) || (value->capabilities & ~7u)) {
                AR_LOG(logger, ar::log::Level::warning, "Plugin v2 descriptor is malformed or incompatible");
                continue;
            }
            module->families = value->families;
            module->capabilities = value->capabilities;
        }
#define AR_LOAD(field) module->field = exported<decltype(module->field)>(module->library, "arfw_" #field)
        AR_LOAD(matcher_create);
        AR_LOAD(matcher_destroy);
        AR_LOAD(matcher_prepare);
        AR_LOAD(matcher_find);
        AR_LOAD(controller_create);
        AR_LOAD(controller_destroy);
        AR_LOAD(controller_connect);
        AR_LOAD(controller_disconnect);
        AR_LOAD(controller_click);
        AR_LOAD(controller_long_press);
        AR_LOAD(controller_swipe);
        AR_LOAD(controller_press_key);
        AR_LOAD(controller_screenshot);
        AR_LOAD(controller_free_image);
        AR_LOAD(device_create);
        AR_LOAD(device_destroy);
        AR_LOAD(device_check_state);
        AR_LOAD(device_launch);
        AR_LOAD(device_shutdown);
        AR_LOAD(device_restart);
        AR_LOAD(device_show_window);
        AR_LOAD(device_hide_window);
        AR_LOAD(device_launch_app);
        AR_LOAD(device_close_app);
        AR_LOAD(device_app_state);
        AR_LOAD(ocr_create);
        AR_LOAD(ocr_destroy);
        AR_LOAD(ocr_recognize);
        if (descriptor) {
            AR_LOAD(matcher_create_v2);
            AR_LOAD(matcher_find_v2);
            AR_LOAD(controller_create_v2);
            AR_LOAD(controller_set_context_v2);
            AR_LOAD(controller_last_error_v2);
            AR_LOAD(controller_error_v2);
            AR_LOAD(controller_start_app_v2);
            AR_LOAD(controller_stop_app_v2);
            AR_LOAD(controller_screenshot_v2);
            AR_LOAD(ocr_create_v2);
            AR_LOAD(ocr_create_profile_v3);
            AR_LOAD(ocr_run_mode_v3);
            AR_LOAD(ocr_run_v2);
            AR_LOAD(ocr_result_count_v2);
            AR_LOAD(ocr_result_item_v2);
            AR_LOAD(ocr_result_destroy_v2);
        }
#undef AR_LOAD
#define AR_PRESENT(field) {module->field != nullptr, "arfw_" #field}
        bool valid = true;
        if (module->matcher_create || (module->families & ARFW_FAMILY_MATCHER))
            valid = required(logger, module->name,
                             {AR_PRESENT(matcher_create), AR_PRESENT(matcher_destroy),
                              AR_PRESENT(matcher_find)}) &&
                    valid;
        if (module->controller_create || (module->families & ARFW_FAMILY_CONTROLLER))
            valid = required(logger, module->name,
                             {AR_PRESENT(controller_create), AR_PRESENT(controller_destroy),
                              AR_PRESENT(controller_connect), AR_PRESENT(controller_disconnect),
                              AR_PRESENT(controller_click), AR_PRESENT(controller_long_press),
                              AR_PRESENT(controller_swipe), AR_PRESENT(controller_press_key),
                              AR_PRESENT(controller_screenshot), AR_PRESENT(controller_free_image)}) &&
                    valid;
        if (module->device_create || (module->families & ARFW_FAMILY_DEVICE))
            valid = required(logger, module->name,
                             {AR_PRESENT(device_create), AR_PRESENT(device_destroy),
                              AR_PRESENT(device_check_state), AR_PRESENT(device_launch),
                              AR_PRESENT(device_shutdown), AR_PRESENT(device_restart),
                              AR_PRESENT(device_show_window), AR_PRESENT(device_hide_window),
                              AR_PRESENT(device_launch_app), AR_PRESENT(device_close_app),
                              AR_PRESENT(device_app_state)}) &&
                    valid;
        if (module->ocr_create || (module->families & ARFW_FAMILY_OCR))
            valid = required(logger, module->name,
                             {AR_PRESENT(ocr_create), AR_PRESENT(ocr_destroy), AR_PRESENT(ocr_recognize)}) &&
                    valid;
        if (descriptor) {
            const uint32_t exported_families =
                (module->matcher_create ? 1u : 0u) | (module->controller_create ? 2u : 0u) |
                (module->device_create ? 4u : 0u) | (module->ocr_create ? 8u : 0u);
            if (exported_families != module->families) {
                AR_LOG(logger, ar::log::Level::warning, "Plugin families disagree with descriptor");
                valid = false;
            }
            if (module->families & ARFW_FAMILY_MATCHER)
                valid = required(logger, module->name, {AR_PRESENT(matcher_find_v2)}) && valid;
            if ((module->families & ARFW_FAMILY_MATCHER) && (module->capabilities & ARFW_CAP_CONFIG))
                valid = required(logger, module->name, {AR_PRESENT(matcher_create_v2)}) && valid;
            if (module->families & ARFW_FAMILY_CONTROLLER)
                valid = required(logger, module->name,
                                 {AR_PRESENT(controller_create_v2), AR_PRESENT(controller_last_error_v2),
                                  AR_PRESENT(controller_error_v2), AR_PRESENT(controller_screenshot_v2),
                                  AR_PRESENT(controller_start_app_v2), AR_PRESENT(controller_stop_app_v2)}) &&
                        valid;
            if ((module->families & ARFW_FAMILY_CONTROLLER) && (module->capabilities & ARFW_CAP_CONTEXT))
                valid = required(logger, module->name, {AR_PRESENT(controller_set_context_v2)}) && valid;
            if (module->families & ARFW_FAMILY_OCR)
                valid = required(logger, module->name,
                                 {AR_PRESENT(ocr_create_v2), AR_PRESENT(ocr_run_v2),
                                  AR_PRESENT(ocr_result_count_v2), AR_PRESENT(ocr_result_item_v2),
                                  AR_PRESENT(ocr_result_destroy_v2)}) &&
                        valid;
        }
#undef AR_PRESENT
        if (!valid)
            continue;
        int count = 0;
        if (module->matcher_create &&
            ar::vision::MatcherRegistry::instance().add(
                module->name, [module](const auto &config) -> std::unique_ptr<ar::vision::IImageMatcher> {
                    return create_adapter<ar::vision::IImageMatcher, detail::Matcher>(module, config.logger,
                                                                                      config);
                })) {
            ++count;
        }
        if (module->controller_create &&
            ar::control::ControllerRegistry::instance().add(
                module->name, [module](const auto &config) -> std::unique_ptr<ar::control::IController> {
                    return create_adapter<ar::control::IController, detail::Controller>(module, config.logger,
                                                                                        config);
                })) {
            ++count;
        }
        if (module->device_create &&
            ar::device::DeviceControllerRegistry::instance().add(
                module->name,
                [module](const auto &path, int32_t index,
                         auto *logger) -> std::unique_ptr<ar::device::IDeviceController> {
                    return create_adapter<ar::device::IDeviceController, detail::Device>(module, logger, path,
                                                                                         index, logger);
                })) {
            ++count;
        }
        if (module->ocr_create &&
            ar::vision::OcrRegistry::instance().add(
                module->name,
                [module](const auto &params, auto *logger) -> std::unique_ptr<ar::vision::IOcrEngine> {
                    return create_adapter<ar::vision::IOcrEngine, detail::Ocr>(module, logger, params,
                                                                               logger);
                })) {
            ++count;
        }
        if (count) {
            loaded.push_back(module->name);
            AR_LOG(logger, ar::log::Level::info,
                   std::format("Loaded backend '{}' ({} ABI)", module->name, descriptor ? "v2" : "v1"));
        } else
            AR_LOG(logger, ar::log::Level::warning,
                   std::format("Backend '{}' is duplicate, frozen or has no usable family", module->name));
    }
    return loaded;
}
void freeze_registries() {
    ar::vision::MatcherRegistry::instance().freeze();
    ar::vision::OcrRegistry::instance().freeze();
    ar::control::ControllerRegistry::instance().freeze();
    ar::device::DeviceControllerRegistry::instance().freeze();
}
} // namespace ar::plugin
