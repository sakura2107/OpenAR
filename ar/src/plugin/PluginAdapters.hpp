#pragma once
// Internal implementation shared only with PluginLoader.cpp.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <memory>
#include <string>
#include <Windows.h>
#include "control/Controller.h"
#include "core/Str.hpp"
#include "device/DeviceController.h"
#include "plugin/PluginApiV2.h"
#include "vision/Matcher.h"
#include "vision/Ocr.h"
namespace ar::plugin::detail {
struct Module {
    HMODULE library = nullptr;
    DLL_DIRECTORY_COOKIE directory = nullptr;
    std::string name;
    uint32_t families = 0, capabilities = 0;
    void *(*matcher_create)(arfw_log_fn, void *) = nullptr;
    void *(*matcher_create_v2)(const ArfwMatcherConfigV2 *, arfw_log_fn, void *, char *, int32_t) = nullptr;
    int32_t (*matcher_find_v2)(void *, const ArfwImageViewV2 *, const ArfwImageViewV2 *, float,
                               const ArfwRectV2 *, float *, int32_t *, int32_t *, char *, int32_t) = nullptr;
    void (*matcher_destroy)(void *) = nullptr;
    int32_t (*matcher_prepare)(void *, const uint8_t *, int64_t, int32_t, int32_t, int32_t) = nullptr;
    int32_t (*matcher_find)(void *, const uint8_t *, int64_t, int32_t, int32_t, int32_t, const uint8_t *,
                            int64_t, int32_t, int32_t, float, int32_t, int32_t, int32_t, int32_t, float *,
                            int32_t *, int32_t *, char *, int32_t) = nullptr;
    void *(*controller_create)(const ArfwControllerConfig *, arfw_log_fn, void *) = nullptr;
    void *(*controller_create_v2)(const ArfwControllerConfigV2 *, arfw_log_fn, void *, char *,
                                  int32_t) = nullptr;
    void (*controller_destroy)(void *) = nullptr;
    int32_t (*controller_connect)(void *) = nullptr;
    int32_t (*controller_disconnect)(void *) = nullptr;
    int32_t (*controller_click)(void *, int32_t, int32_t) = nullptr;
    int32_t (*controller_long_press)(void *, int32_t, int32_t, int32_t) = nullptr;
    int32_t (*controller_swipe)(void *, int32_t, int32_t, int32_t, int32_t, int32_t) = nullptr;
    int32_t (*controller_press_key)(void *, int32_t) = nullptr;
    int32_t (*controller_screenshot)(void *, int32_t *, int32_t *, int32_t *, uint8_t **) = nullptr;
    int32_t (*controller_screenshot_v2)(void *, ArfwImageViewV2 *) = nullptr;
    void (*controller_free_image)(void *, uint8_t *) = nullptr;
    int32_t (*controller_set_context_v2)(void *, arfw_cancel_fn, void *, uint32_t) = nullptr;
    int32_t (*controller_last_error_v2)(void *, char *, int32_t) = nullptr;
    int32_t (*controller_error_v2)(void *, ArfwErrorV2 *) = nullptr;
    int32_t (*controller_start_app_v2)(void *, const char *) = nullptr;
    int32_t (*controller_stop_app_v2)(void *, const char *) = nullptr;
    void *(*device_create)(const char *, int32_t, arfw_log_fn, void *) = nullptr;
    void (*device_destroy)(void *) = nullptr;
    int32_t (*device_check_state)(void *) = nullptr;
    int32_t (*device_launch)(void *) = nullptr;
    int32_t (*device_shutdown)(void *) = nullptr;
    int32_t (*device_restart)(void *) = nullptr;
    int32_t (*device_show_window)(void *) = nullptr;
    int32_t (*device_hide_window)(void *) = nullptr;
    int32_t (*device_launch_app)(void *, const char *) = nullptr;
    int32_t (*device_close_app)(void *, const char *) = nullptr;
    int32_t (*device_app_state)(void *, const char *, ArfwAppState *) = nullptr;
    void *(*ocr_create)(const ArfwOcrParams *, arfw_log_fn, void *) = nullptr;
    void *(*ocr_create_v2)(const ArfwOcrParamsV2 *, arfw_log_fn, void *, char *, int32_t) = nullptr;
    void *(*ocr_create_profile_v3)(const char *, int32_t, arfw_log_fn, void *, char *, int32_t) = nullptr;
    int32_t (*ocr_run_mode_v3)(void *, const ArfwImageViewV2 *, const ArfwRectV2 *, int32_t, void **,
                              double *, double *, char *, int32_t) = nullptr;
    void (*ocr_destroy)(void *) = nullptr;
    int32_t (*ocr_recognize)(void *, const uint8_t *, int64_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                             int32_t, int32_t, ArfwOcrItem *, int32_t, int32_t *, char *, int32_t) = nullptr;
    int32_t (*ocr_run_v2)(void *, const ArfwImageViewV2 *, const ArfwRectV2 *, void **, char *,
                          int32_t) = nullptr;
    int32_t (*ocr_result_count_v2)(void *) = nullptr;
    int32_t (*ocr_result_item_v2)(void *, int32_t, ArfwOcrItemV2 *) = nullptr;
    void (*ocr_result_destroy_v2)(void *) = nullptr;
    ~Module() {
        if (library)
            FreeLibrary(library);
        if (directory)
            RemoveDllDirectory(directory);
    }
};
inline void log_bridge(void *user, int32_t level, const char *text) noexcept {
    if (user && text && level >= 0 && level <= 4)
        static_cast<ar::log::ILogger *>(user)->log(static_cast<ar::log::Level>(level), text);
}
inline ar::core::Error plugin_error(const Module &module, std::string operation, const char *text,
                                    int32_t status = ARFW_PLUGIN_ERR_FAILED) {
    return ar::core::failure(status == ARFW_PLUGIN_ERR_ARGS ? ar::core::ErrorCode::invalid_argument
                                                            : ar::core::ErrorCode::backend_failed,
                             std::move(operation), text && *text ? text : "Plugin operation failed",
                             module.name, status);
}
inline ArfwImageViewV2 view(const cv::Mat &image) {
    return {sizeof(ArfwImageViewV2),
            image.data,
            uint64_t(image.rows - 1) * image.step + uint64_t(image.cols) * image.elemSize(),
            int64_t(image.step),
            image.cols,
            image.rows,
            image.channels()};
}
class Matcher final : public ar::vision::IImageMatcher {
  public:
    Matcher(std::shared_ptr<Module> module, const ar::vision::MatcherConfig &config)
        : module_(std::move(module)), logger_(config.logger), limit_(config.max_input_bytes) {
        if (limit_ <= 0 || limit_ > 256 * 1024 * 1024)
            throw std::invalid_argument("Invalid matcher byte limit");
        char error[512]{};
        if (module_->matcher_create_v2) {
            ArfwMatcherConfigV2 c{sizeof(c), limit_};
            handle_ = module_->matcher_create_v2(&c, log_bridge, logger_, error, sizeof(error));
        } else {
            if (limit_ > 4096 * 2160 * 3)
                throw std::runtime_error("v1 matcher cannot increase its input byte limit");
            handle_ = module_->matcher_create(log_bridge, logger_);
        }
        if (!handle_)
            throw std::runtime_error(*error ? error : "Matcher initialization failed");
    }
    ~Matcher() override {
        if (handle_)
            module_->matcher_destroy(handle_);
    }
    std::string_view name() const override { return module_->name; }
    bool prepare(const cv::Mat &templ) override {
        if (!ar::core::validate_image(templ, limit_))
            return false;
        return !module_->matcher_prepare ||
               module_->matcher_prepare(handle_, templ.data, templ.step, templ.cols, templ.rows,
                                        templ.channels()) == 0;
    }
    ar::core::Result<ar::vision::MatchResult> find_checked(const cv::Mat &image, const cv::Mat &templ,
                                                           float threshold, const cv::Rect *roi) override {
        if (!ar::core::validate_image(image, limit_) || !ar::core::validate_image(templ, limit_) ||
            image.type() != templ.type() || (image.channels() != 1 && image.channels() != 3) ||
            !std::isfinite(threshold) || threshold < -1 || threshold > 1 ||
            (roi && !ar::core::valid_roi(*roi, image.size())))
            return plugin_error(*module_, "match", "Invalid matcher input", 1);
        ar::vision::MatchResult result;
        if (templ.cols > (roi ? roi->width : image.cols) || templ.rows > (roi ? roi->height : image.rows))
            return result;
        float score = -1;
        int32_t x = -1, y = -1;
        char error[512]{};
        int32_t status;
        if (module_->matcher_find_v2) {
            const auto iv = view(image), tv = view(templ);
            ArfwRectV2 range{};
            if (roi)
                range = {roi->x, roi->y, roi->width, roi->height};
            status = module_->matcher_find_v2(handle_, &iv, &tv, threshold, roi ? &range : nullptr, &score,
                                              &x, &y, error, sizeof(error));
        } else
            status = module_->matcher_find(
                handle_, image.data, image.step, image.cols, image.rows, image.channels(), templ.data,
                templ.step, templ.cols, templ.rows, threshold, roi ? roi->x : 0, roi ? roi->y : 0,
                roi ? roi->width : 0, roi ? roi->height : 0, &score, &x, &y, error, sizeof(error));
        if (status != 0)
            return plugin_error(*module_, "match", error, status);
        if (!std::isfinite(score) || ((x < 0) != (y < 0)))
            return plugin_error(*module_, "match", "Malformed matcher result");
        result.score = score;
        if (x >= 0 && y >= 0) {
            const cv::Rect bounds = roi ? *roi : cv::Rect(0, 0, image.cols, image.rows);
            if (x < bounds.x || y < bounds.y ||
                int64_t(x) + templ.cols > int64_t(bounds.x) + bounds.width + 1 ||
                int64_t(y) + templ.rows > int64_t(bounds.y) + bounds.height + 1)
                return plugin_error(*module_, "match", "Matcher result outside ROI");
            x = std::min(x, bounds.x + bounds.width - templ.cols);
            y = std::min(y, bounds.y + bounds.height - templ.rows);
            result.found = score >= threshold;
            result.box = {x, y, templ.cols, templ.rows};
            result.center = {x + templ.cols / 2, y + templ.rows / 2};
        }
        return result;
    }
    ar::vision::MatchResult find(const cv::Mat &image, const cv::Mat &templ, float threshold,
                                 const cv::Rect *roi) override {
        auto result = find_checked(image, templ, threshold, roi);
        if (result)
            return result.value();
        AR_LOG(logger_, ar::log::Level::error, result.error().message);
        return {};
    }

  private:
    std::shared_ptr<Module> module_;
    ar::log::ILogger *logger_;
    int32_t limit_;
    void *handle_ = nullptr;
};
class Controller final : public ar::control::IController {
  public:
    Controller(std::shared_ptr<Module> module, const ar::control::ControllerConfig &config)
        : module_(std::move(module)) {
        const auto adb = ar::core::wide_to_utf8(config.adb_path.wstring()),
                   mumu = ar::core::wide_to_utf8(config.mumu_path.wstring());
        ArfwControllerConfig c{adb.empty() ? nullptr : adb.c_str(), config.adb_port,
                               mumu.empty() ? nullptr : mumu.c_str(), config.mumu_index};
        char error[512]{};
        if (module_->controller_create_v2) {
            ArfwControllerConfigV2 v{sizeof(v), c, config.serial.c_str(), config.command_timeout_ms};
            handle_ = module_->controller_create_v2(&v, log_bridge, config.logger, error, sizeof(error));
        } else {
            if (!config.serial.empty())
                throw std::runtime_error("v1 controller does not support explicit serial");
            handle_ = module_->controller_create(&c, log_bridge, config.logger);
        }
        if (!handle_)
            throw std::runtime_error(*error ? error : "Controller initialization failed");
    }
    ~Controller() override {
        if (handle_)
            module_->controller_destroy(handle_);
    }
    std::string_view name() const override { return module_->name; }
    bool connect() override { return checked(module_->controller_connect(handle_), "connect"); }
    bool disconnect() override { return checked(module_->controller_disconnect(handle_), "disconnect"); }
    bool click(cv::Point p) override {
        return checked(module_->controller_click(handle_, p.x, p.y), "click");
    }
    bool long_press(cv::Point p, int32_t ms) override {
        return checked(module_->controller_long_press(handle_, p.x, p.y, ms), "long_press");
    }
    bool swipe(cv::Point a, cv::Point b, int32_t ms) override {
        return checked(module_->controller_swipe(handle_, a.x, a.y, b.x, b.y, ms), "swipe");
    }
    bool press_key(int32_t key) override {
        return checked(module_->controller_press_key(handle_, key), "key");
    }
    void set_context(std::function<bool()> cancel, uint32_t timeout) override {
        cancelled_ = std::move(cancel);
        if (module_->controller_set_context_v2)
            checked(module_->controller_set_context_v2(
                        handle_,
                        [](void *self) -> int32_t {
                            auto *c = static_cast<Controller *>(self);
                            return c->cancelled_ && c->cancelled_();
                        },
                        this, timeout),
                    "context");
    }
    ar::core::Error last_error() const override { return error_; }
    bool supports_context() const override { return (module_->capabilities & ARFW_CAP_CONTEXT) != 0; }
    bool start_app(std::string_view package) override {
        const std::string p(package);
        if (!module_->controller_start_app_v2) {
            error_ = ar::core::failure(ar::core::ErrorCode::unsupported, "start_app",
                                       "Controller has no app lifecycle capability", module_->name);
            return false;
        }
        return checked(module_->controller_start_app_v2(handle_, p.c_str()), "start_app");
    }
    bool stop_app(std::string_view package) override {
        const std::string p(package);
        if (!module_->controller_stop_app_v2) {
            error_ = ar::core::failure(ar::core::ErrorCode::unsupported, "stop_app",
                                       "Controller has no app lifecycle capability", module_->name);
            return false;
        }
        return checked(module_->controller_stop_app_v2(handle_, p.c_str()), "stop_app");
    }
    cv::Mat screenshot() override {
        auto result = capture_checked();
        return result ? result.value() : cv::Mat{};
    }
    ar::core::Result<cv::Mat> capture_checked() override {
        int32_t w = 0, h = 0, channels = 0;
        uint8_t *data = nullptr;
        uint64_t length = 0;
        int32_t status;
        if (module_->controller_screenshot_v2) {
            ArfwImageViewV2 v{};
            v.struct_size = sizeof(v);
            status = module_->controller_screenshot_v2(handle_, &v);
            data = const_cast<uint8_t *>(v.data);
            w = v.width;
            h = v.height;
            channels = v.channels;
            length = v.byte_length;
            if (v.stride != int64_t(w) * channels)
                status = ARFW_PLUGIN_ERR_FAILED;
        } else
            status = module_->controller_screenshot(handle_, &w, &h, &channels, &data);
        struct Release {
            Module *module;
            void *handle;
            uint8_t *data;
            ~Release() {
                if (data)
                    module->controller_free_image(handle, data);
            }
        } release{module_.get(), handle_, data};
        if (!checked(status, "screenshot"))
            return error_;
        if (!data || w <= 0 || h <= 0 || (channels != 1 && channels != 3 && channels != 4) ||
            uint64_t(w) * h * channels > 256ull * 1024 * 1024 ||
            (module_->controller_screenshot_v2 && length != uint64_t(w) * h * channels))
            return plugin_error(*module_, "screenshot", "Invalid screenshot dimensions or byte length");
        try {
            return cv::Mat(h, w, CV_MAKETYPE(CV_8U, channels), data).clone();
        } catch (const std::exception &e) {
            return plugin_error(*module_, "screenshot", e.what());
        }
    }

  private:
    bool checked(int32_t status, const char *operation) {
        if (status == 0)
            return true;
        char text[512]{};
        int32_t code = static_cast<int32_t>(ar::core::ErrorCode::backend_failed);
        if (module_->controller_last_error_v2)
            code = module_->controller_last_error_v2(handle_, text, sizeof(text));
        error_ = plugin_error(*module_, operation, text, status);
        if (code >= 0 && code <= static_cast<int32_t>(ar::core::ErrorCode::budget_exceeded))
            error_.code = static_cast<ar::core::ErrorCode>(code);
        if (module_->controller_error_v2) {
            ArfwErrorV2 native{};
            native.struct_size = sizeof(native);
            if (module_->controller_error_v2(handle_, &native) == 0 && native.code >= 0 &&
                native.code <= 12) {
                error_.code = static_cast<ar::core::ErrorCode>(native.code);
                error_.native_code = native.native_code;
                if (native.message)
                    error_.message = native.message;
                if (native.operation && *native.operation)
                    error_.operation = native.operation;
            }
        }
        if (cancelled_ && cancelled_())
            error_.code = ar::core::ErrorCode::cancelled;
        return false;
    }
    std::shared_ptr<Module> module_;
    void *handle_ = nullptr;
    std::function<bool()> cancelled_;
    ar::core::Error error_;
};
class Device final : public ar::device::IDeviceController {
  public:
    Device(std::shared_ptr<Module> module, const std::filesystem::path &path, int32_t index,
           ar::log::ILogger *logger)
        : module_(std::move(module)) {
        const auto text = ar::core::wide_to_utf8(path.wstring());
        handle_ = module_->device_create(text.c_str(), index, log_bridge, logger);
        if (!handle_)
            throw std::runtime_error("Device initialization failed");
    }
    ~Device() override {
        if (handle_)
            module_->device_destroy(handle_);
    }
    std::string_view name() const override { return module_->name; }
    ar::device::Status check_device_state() override { return call(module_->device_check_state); }
    ar::device::Status launch() override { return call(module_->device_launch); }
    ar::device::Status shutdown() override { return call(module_->device_shutdown); }
    ar::device::Status restart() override { return call(module_->device_restart); }
    ar::device::Status show_window() override { return call(module_->device_show_window); }
    ar::device::Status hide_window() override { return call(module_->device_hide_window); }
    ar::device::Status launch_app(std::string_view package) override {
        return str_call(module_->device_launch_app, package);
    }
    ar::device::Status close_app(std::string_view package) override {
        return str_call(module_->device_close_app, package);
    }
    ar::device::AppState app_state(std::string_view package) override {
        ArfwAppState state{};
        const std::string text(package);
        if (module_->device_app_state(handle_, text.c_str(), &state) != 0)
            return {};
        return {state.installed != 0, state.running != 0};
    }

  private:
    ar::device::Status call(int32_t (*fn)(void *)) { return static_cast<ar::device::Status>(fn(handle_)); }
    ar::device::Status str_call(int32_t (*fn)(void *, const char *), std::string_view text) {
        const std::string copy(text);
        return static_cast<ar::device::Status>(fn(handle_, copy.c_str()));
    }
    std::shared_ptr<Module> module_;
    void *handle_ = nullptr;
};
class Ocr final : public ar::vision::IOcrEngine {
  public:
    Ocr(std::shared_ptr<Module> module, const ar::vision::OcrParams &params, ar::log::ILogger *logger)
        : module_(std::move(module)), logger_(logger) {
        ArfwOcrParams c{params.det_model.c_str(),  params.rec_model.c_str(), params.dictionary.c_str(),
                        params.text_threshold,     params.box_threshold,     params.unclip_ratio,
                        params.score_mode ? 1 : 0, params.device_id};
        char error[512]{};
        if (!params.profile.empty()) {
            if (!module_->ocr_create_profile_v3) throw std::runtime_error("OCR plugin does not support language profiles");
            handle_ = module_->ocr_create_profile_v3(params.profile.c_str(), params.device_id, log_bridge, logger, error, sizeof(error));
        } else if (module_->ocr_create_v2) {
            ArfwOcrParamsV2 v{sizeof(v), c, params.det_line_merge_ratio};
            handle_ = module_->ocr_create_v2(&v, log_bridge, logger, error, sizeof(error));
        } else {
            if (params.det_line_merge_ratio != 0.5f)
                throw std::runtime_error("v1 OCR cannot configure line merge ratio");
            handle_ = module_->ocr_create(&c, log_bridge, logger);
        }
        if (!handle_)
            throw std::runtime_error(*error ? error : "OCR initialization failed");
    }
    ~Ocr() override {
        if (handle_)
            module_->ocr_destroy(handle_);
    }
    std::string_view name() const override { return module_->name; }
    ar::core::Result<std::vector<ar::vision::OcrItem>> recognize_checked(const cv::Mat &image,
                                                                         const cv::Rect *roi) override {
        return run_checked(image, roi, 0);
    }
    ar::core::Result<std::vector<ar::vision::OcrItem>> recognize_crop_checked(const cv::Mat &image,
        const cv::Rect *roi = nullptr, bool numbers = false) override {
        return run_checked(image, roi, numbers ? 2 : 1);
    }
    ar::core::Result<std::vector<ar::vision::OcrItem>> run_checked(const cv::Mat &image, const cv::Rect *roi, int mode) {
        if (mode && !module_->ocr_run_mode_v3)
            return ar::core::failure(ar::core::ErrorCode::unsupported, "ocr.crop", "Plugin has no crop recognition route", std::string(name()));
        if (!ar::core::validate_image(image) || (roi && !ar::core::valid_roi(*roi, image.size())))
            return plugin_error(*module_, "ocr", "Invalid OCR input", 1);
        std::vector<ar::vision::OcrItem> items;
        char error[512]{};
        if (module_->ocr_run_v2) {
            const auto image_view = view(image);
            ArfwRectV2 range{};
            if (roi)
                range = {roi->x, roi->y, roi->width, roi->height};
            void *result = nullptr;
            double detection_ms = 0, recognition_ms = 0;
            const auto status = module_->ocr_run_mode_v3 ?
                module_->ocr_run_mode_v3(handle_, &image_view, roi ? &range : nullptr, mode, &result,
                                         &detection_ms, &recognition_ms, error, sizeof(error)) :
                module_->ocr_run_v2(handle_, &image_view, roi ? &range : nullptr, &result, error, sizeof(error));
            struct Release {
                Module *module;
                void *result;
                ~Release() {
                    if (result)
                        module->ocr_result_destroy_v2(result);
                }
            } release{module_.get(), result};
            if (status != 0 || !result)
                return plugin_error(*module_, "ocr", error, status);
            const auto count = module_->ocr_result_count_v2(result);
            if (count < 0 || count > 10000)
                return plugin_error(*module_, "ocr", "Invalid OCR result count");
            items.reserve(count);
            size_t text_bytes = 0;
            for (int32_t i = 0; i < count; ++i) {
                ArfwOcrItemV2 item{};
                item.struct_size = sizeof(item);
                if (module_->ocr_result_item_v2(result, i, &item) != 0 || !item.text ||
                    item.text_length > 1024 * 1024 || !std::isfinite(item.score))
                    return plugin_error(*module_, "ocr", "Malformed OCR item");
                text_bytes += static_cast<size_t>(item.text_length);
                if (text_bytes > 16 * 1024 * 1024)
                    return plugin_error(*module_, "ocr", "OCR text exceeds byte budget");
                if (item.text_length && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, item.text,
                                                             static_cast<int>(item.text_length), nullptr, 0))
                    return plugin_error(*module_, "ocr", "Invalid UTF-8 OCR text");
                items.push_back({std::string(item.text, size_t(item.text_length)),
                                 item.score,
                                 {item.box.x, item.box.y, item.box.width, item.box.height}});
            }
        } else {
            int32_t count = 0;
            auto call = [&](ArfwOcrItem *output, int32_t capacity) {
                return module_->ocr_recognize(handle_, image.data, image.step, image.cols, image.rows,
                                              image.channels(), roi ? roi->x : 0, roi ? roi->y : 0,
                                              roi ? roi->width : 0, roi ? roi->height : 0, output, capacity,
                                              &count, error, sizeof(error));
            };
            auto status = call(nullptr, 0);
            if (status != 0)
                return plugin_error(*module_, "ocr", error, status);
            if (count < 0 || count > 10000)
                return plugin_error(*module_, "ocr", "Invalid OCR result count");
            if (!count)
                return items;
            const auto capacity = count;
            std::vector<ArfwOcrItem> output(capacity);
            status = call(output.data(), capacity);
            if (status != 0)
                return plugin_error(*module_, "ocr", error, status);
            if (count < 0 || count > capacity)
                return plugin_error(*module_, "ocr", "v1 OCR result grew beyond capacity");
            for (int32_t i = 0; i < count; ++i) {
                const auto &item = output[i];
                const char *end = static_cast<const char *>(std::memchr(item.text, 0, sizeof(item.text)));
                if (!end || !std::isfinite(item.score))
                    return plugin_error(*module_, "ocr", "Malformed v1 OCR item");
                items.push_back({std::string(item.text, end),
                                 item.score,
                                 {item.box_x, item.box_y, item.box_w, item.box_h}});
            }
        }
        for (const auto &item : items)
            if (!ar::core::valid_roi(item.box, image.size()))
                return plugin_error(*module_, "ocr", "OCR box outside source image");
        return items;
    }
    std::vector<ar::vision::OcrItem> recognize(const cv::Mat &image, const cv::Rect *roi) override {
        auto result = recognize_checked(image, roi);
        if (result)
            return result.value();
        AR_LOG(logger_, ar::log::Level::error, result.error().message);
        return {};
    }

  private:
    std::shared_ptr<Module> module_;
    void *handle_ = nullptr;
    ar::log::ILogger *logger_;
};
} // namespace ar::plugin::detail
