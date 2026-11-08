#include "MuMuController.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <system_error>

#include "core/Str.hpp"
#include "core/Path.hpp"

namespace ar::control {

namespace {
constexpr wchar_t kIpcDllName[] = L"external_renderer_ipc.dll";

template <typename T> T load_function(HINSTANCE dll, const char *name) {
    return reinterpret_cast<T>(GetProcAddress(dll, name));
}
} // namespace

MuMuController::MuMuController(const ControllerConfig &config)
    : adb_(config.adb_path, config.logger),
      serial_(config.serial.empty() ? std::format("127.0.0.1:{}", config.adb_port) : config.serial),
      index_(config.mumu_index), logger_(config.logger) {
    if (config.mumu_path.empty() || !std::filesystem::is_directory(config.mumu_path) || index_ < 0)
        throw std::invalid_argument("MuMu needs a valid installation directory and instance index");
    // nemu_connect wants the emulator's install root; MuMu >= 13 dereferences
    // it and crashes on null (older builds accepted null and resolved the
    // install themselves). The dll itself is usually NOT in that root (MuMu 12
    // keeps it in shell\, newer builds in nx_main\sdk\), so loading falls back
    // to a bare LoadLibrary, which finds the copy next to the exe.
    mumu_path_ = std::filesystem::absolute(config.mumu_path).wstring();
    set_context({}, config.command_timeout_ms);
}

MuMuController::~MuMuController() {
    disconnect();
}

bool MuMuController::load_library() {
    if (loaded_)
        return true;

    std::string output;
    if (!adb_.connect(serial_, output))
        return false;
    if (output.find("(10016)") != std::string::npos || output.find("error") != std::string::npos) {
        AR_LOG(logger_, ar::log::Level::error,
               std::format("Cannot connect {}. Please check this host !", serial_));
        return fail_sdk("adb.connect", "MuMu ADB transport unavailable", 0, ar::core::ErrorCode::offline);
    }

    // Load the emulator's OWN dll first: its IPC protocol must match the
    // running emulator version (a MuMu 12 dll cannot capture from a newer
    // emulator - size queries still answer, buffer capture fails silently).
    // Layouts: MuMu 12 -> <root>\shell\, MuMu >= 13 -> <root>\nx_main\sdk\.
    // The framework's copy next to the exe is the last-resort fallback.
    std::vector<std::wstring> candidates;
    if (!mumu_path_.empty()) {
        candidates.push_back(mumu_path_ + L"\\shell\\" + kIpcDllName);
        candidates.push_back(mumu_path_ + L"\\nx_main\\sdk\\" + kIpcDllName);
        candidates.push_back(mumu_path_ + L"\\" + kIpcDllName);
    }
    candidates.push_back((ar::core::exe_dir() / kIpcDllName).wstring());
    for (const auto &candidate : candidates) {
        dll_ = LoadLibraryExW(candidate.c_str(), nullptr,
                              LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (dll_)
            break;
    }
    if (!dll_) {
        AR_LOG(logger_, ar::log::Level::error,
               std::format("Cannot find {} (searched install dir and exe dir)",
                           ar::core::wide_to_utf8(kIpcDllName)));
        return fail_sdk("load_library", "MuMu SDK DLL not found", GetLastError(),
                        ar::core::ErrorCode::dependency_missing);
    }

    nemu_connect_ = load_function<NemuConnectFunc>(dll_, "nemu_connect");
    nemu_capture_display_ = load_function<NemuCaptureFunc>(dll_, "nemu_capture_display");
    nemu_touch_down_ = load_function<NemuTouchDownFunc>(dll_, "nemu_input_event_touch_down");
    nemu_touch_up_ = load_function<NemuTouchUpFunc>(dll_, "nemu_input_event_touch_up");
    nemu_disconnect_ = load_function<NemuDisconnectFunc>(dll_, "nemu_disconnect");
    if (!nemu_connect_ || !nemu_capture_display_ || !nemu_touch_down_ || !nemu_touch_up_ ||
        !nemu_disconnect_) {
        AR_LOG(logger_, ar::log::Level::error, "MuMu controller DLL is missing a required function");
        FreeLibrary(dll_);
        dll_ = nullptr;
        return fail_sdk("load_library", "MuMu SDK is missing a required export", 0,
                        ar::core::ErrorCode::unsupported);
    }
    loaded_ = true;
    return true;
}

bool MuMuController::connect() {
    sdk_error_.reset();
    if (connected_)
        return true;
    if (handle_)
        disconnect();
    if (!load_library())
        return false;

    handle_ = nemu_connect_(mumu_path_.empty() ? nullptr : mumu_path_.c_str(), index_);
    if (!handle_) {
        AR_LOG(logger_, ar::log::Level::error, std::format("MuMu {} connect fail !", index_));
        return fail_sdk("connect", "MuMu SDK connection failed");
    }

    int width = 0;
    int height = 0;
    if (nemu_capture_display_(handle_, 0, 0, &width, &height, nullptr) != 0 || width <= 0 || height <= 0 ||
        uint64_t(width) * height * 4 > 256ull * 1024 * 1024) {
        AR_LOG(logger_, ar::log::Level::error,
               "Get MuMu screen size failed! Please check the MuMu emulator is running !");
        disconnect();
        return fail_sdk("connect", "MuMu screen size is unavailable or exceeds byte budget");
    }
    frame_buffer_.resize(static_cast<size_t>(width) * height * 4);
    connected_ = true;
    return true;
}

bool MuMuController::disconnect() {
    if (handle_ && nemu_disconnect_)
        nemu_disconnect_(handle_);
    handle_ = 0;
    connected_ = false;
    if (dll_)
        FreeLibrary(dll_);
    dll_ = nullptr;
    loaded_ = false;
    nemu_connect_ = nullptr;
    nemu_capture_display_ = nullptr;
    nemu_touch_down_ = nullptr;
    nemu_touch_up_ = nullptr;
    nemu_disconnect_ = nullptr;
    return true;
}

bool MuMuController::ensure_connected() {
    if (connected_)
        return true;
    if (handle_)
        disconnect();
    return connect();
}

bool MuMuController::click(cv::Point point) {
    sdk_error_.reset();
    // Back-to-back down/up can be acknowledged without reaching Android UI.
    // Reuse the cancellable press path; this is gesture duration, not a blind
    // post-action sleep. A fresh frame must still confirm the action outcome.
    constexpr uint32_t tap_ms = 80;
    if (cancelled_ && cancelled_())
        return fail_sdk("click", "Click cancelled before touch", 0, ar::core::ErrorCode::cancelled);
    if (timeout_ms_ < tap_ms)
        return fail_sdk("click", "Insufficient touch duration budget", 0, ar::core::ErrorCode::timeout);
    const bool result = long_press(point, tap_ms);
    if (!result && sdk_error_) sdk_error_->operation = "click";
    return result;
}

bool MuMuController::long_press(cv::Point point, int32_t ms) {
    sdk_error_.reset();
    if (!ensure_connected())
        return false;
    if (nemu_touch_down_(handle_, 0, point.x, point.y)) {
        nemu_touch_up_(handle_, 0);
        return fail_sdk("long_press", "MuMu touch down failed");
    }
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(std::min<uint32_t>(std::max(ms, 0), timeout_ms_));
    bool cancelled = false;
    while (std::chrono::steady_clock::now() < deadline) {
        if (cancelled_ && cancelled_()) {
            cancelled = true;
            break;
        }
        Sleep(5);
    }
    const bool released = nemu_touch_up_(handle_, 0) == 0;
    if (cancelled)
        return fail_sdk("long_press", "Long press cancelled", 0, ar::core::ErrorCode::cancelled);
    if (uint32_t(std::max(ms, 0)) > timeout_ms_)
        return fail_sdk("long_press", "Long press exceeded budget", 0, ar::core::ErrorCode::timeout);
    return released || fail_sdk("long_press", "MuMu touch up failed");
}

bool MuMuController::press_key(int32_t keycode) {
    sdk_error_.reset();
    if (!ensure_connected())
        return false;
    // Public keys are Android KEYCODE values. The vendor key API has different
    // semantics on tested MuMu versions; keep this contract consistent via ADB.
    std::string output;
    return adb_.run(std::format("-s {} shell input keyevent {}", ar::core::quote_arg(serial_), keycode),
                    output);
}

cv::Mat MuMuController::screenshot() {
    sdk_error_.reset();
    if (!ensure_connected())
        return {};

    int width = 0;
    int height = 0;
    if (nemu_capture_display_(handle_, 0, 0, &width, &height, nullptr) != 0 || width <= 0 || height <= 0) {
        AR_LOG(logger_, ar::log::Level::error, "Get MuMu screen size failed during capture");
        fail_sdk("screenshot", "MuMu screen size query failed");
        return {};
    }

    for (int attempt = 0; attempt < 2; ++attempt) {
        const size_t required_bytes = static_cast<size_t>(width) * height * 4;
        if (required_bytes > 256 * 1024 * 1024) {
            fail_sdk("screenshot", "MuMu frame exceeds byte budget");
            return {};
        }
        if (frame_buffer_.size() < required_bytes)
            frame_buffer_.resize(required_bytes);

        int captured_width = width;
        int captured_height = height;
        if (nemu_capture_display_(handle_, 0, static_cast<int>(required_bytes), &captured_width,
                                  &captured_height, frame_buffer_.data()) != 0 ||
            captured_width <= 0 || captured_height <= 0) {
            fail_sdk("screenshot", "MuMu capture failed");
            return {};
        }

        if (captured_width != width || captured_height != height) {
            // The window resized between size query and capture; retry once
            // with the new dimensions before giving up.
            width = captured_width;
            height = captured_height;
            if (attempt == 0)
                continue;
            fail_sdk("screenshot", "MuMu display resized repeatedly during capture");
            return {};
        }

        const cv::Mat rgba(captured_height, captured_width, CV_8UC4, frame_buffer_.data());
        cv::Mat image;
        cv::cvtColor(rgba, image, cv::COLOR_RGBA2BGR);
        cv::flip(image, image, 0);
        return image;
    }
    return {};
}

bool MuMuController::swipe(cv::Point from, cv::Point to, int32_t ms) {
    sdk_error_.reset();
    if (!ensure_connected())
        return false;
    std::string output;
    return adb_.run(std::format("-s {} shell input swipe {} {} {} {} {}", ar::core::quote_arg(serial_),
                                from.x, from.y, to.x, to.y, ms),
                    output) &&
           output.find("error") == std::string::npos;
}

} // namespace ar::control
