#pragma once
#include <filesystem>
#include <format>
#include <functional>
#include <opencv2/opencv.hpp>
#include "core/Process.h"
#include "core/Result.h"
#include "log/Logger.h"
namespace ar::control {
class AdbClient {
  public:
    AdbClient() = default;
    explicit AdbClient(std::filesystem::path path, ar::log::ILogger *logger = nullptr)
        : adb_path_(path.empty() ? std::filesystem::path(L"adb.exe") : std::move(path)), logger_(logger) {}
    void set_context(std::function<bool()> cancelled, uint32_t timeout_ms) {
        options_.cancelled = std::move(cancelled);
        options_.timeout = std::chrono::milliseconds(timeout_ms);
    }
    ar::core::Error last_error() const { return error_; }
    bool run(const std::string &args, std::string &output) const {
        auto result = invoke(args);
        output = result.stdout_text() + result.stderr_text;
        return result.success();
    }
    bool run_binary(const std::string &args, std::vector<uint8_t> &output) const {
        auto result = invoke(args);
        output = std::move(result.stdout_bytes);
        return result.success();
    }
    bool connect(const std::string &serial, std::string &output) const {
        // USB transports do not require adb connect.
        if (serial.find(':') == std::string::npos) {
            return run(std::format("-s {} get-state", ar::core::quote_arg(serial)), output) &&
                   output.find("device") != std::string::npos;
        }
        if (!run("connect " + ar::core::quote_arg(serial), output))
            return false;
        if (output.find("unable") != std::string::npos || output.find("failed") != std::string::npos ||
            output.find("cannot") != std::string::npos || output.find("error") != std::string::npos) {
            error_ = ar::core::failure(ar::core::ErrorCode::offline, "adb.connect", output, "adb");
            return false;
        }
        return true;
    }
    bool screencap(const std::string &serial, cv::Mat &image) const {
        std::vector<uint8_t> png;
        if (!run_binary(std::format("-s {} exec-out screencap -p", ar::core::quote_arg(serial)), png) ||
            png.empty())
            return false;
        image = cv::imdecode(png, cv::IMREAD_COLOR);
        if (image.empty()) {
            error_ = ar::core::failure(ar::core::ErrorCode::backend_failed, "adb.screencap",
                                       "Invalid PNG screenshot", "adb");
            return false;
        }
        return true;
    }
    bool app(const std::string &serial, std::string_view package, bool start) const {
        if (package.empty() ||
            package.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._/") !=
                std::string_view::npos) {
            error_ = ar::core::failure(ar::core::ErrorCode::invalid_argument, "adb.app",
                                       "Invalid package/activity name", "adb");
            return false;
        }
        std::string output;
        std::string component(package);
        if (start && package.find('/') == std::string_view::npos) {
            if (!run(std::format("-s {} shell cmd package resolve-activity --brief -a "
                                 "android.intent.action.MAIN -c android.intent.category.LAUNCHER {}",
                                 ar::core::quote_arg(serial), package),
                     output))
                return false;
            while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
                output.pop_back();
            const auto line = output.find_last_of("\n\r");
            component = output.substr(line == std::string::npos ? 0 : line + 1);
            if (component.find('/') == std::string::npos ||
                component.find_first_not_of(
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._/$") !=
                    std::string::npos) {
                error_ = ar::core::failure(ar::core::ErrorCode::unsupported, "adb.app",
                                           "Package has no launchable activity", "adb");
                return false;
            }
            component = "'" + component + "'";
        }
        return run(std::format("-s {} shell {} {}", ar::core::quote_arg(serial),
                               start ? "am start -n" : "am force-stop", component),
                   output) &&
               output.find("Error") == std::string::npos && output.find("error:") == std::string::npos;
    }

  private:
    ar::core::ProcessResult invoke(const std::string &args) const {
        auto result = ar::core::run_process_command(adb_path_, args, options_);
        if (!result.success()) {
            auto code = ar::core::ErrorCode::backend_failed;
            if (result.state == ar::core::ProcessState::timed_out)
                code = ar::core::ErrorCode::timeout;
            else if (result.state == ar::core::ProcessState::cancelled)
                code = ar::core::ErrorCode::cancelled;
            else if (result.state == ar::core::ProcessState::start_failed)
                code = ar::core::ErrorCode::dependency_missing;
            error_ = ar::core::failure(code, "adb",
                                       result.stderr_text.empty() ? "ADB command failed" : result.stderr_text,
                                       "adb", result.native_error ? result.native_error : result.exit_code);
            AR_LOG(logger_, ar::log::Level::error, error_.message);
        }
        return result;
    }
    std::filesystem::path adb_path_ = L"adb.exe";
    ar::log::ILogger *logger_ = nullptr;
    ar::core::ProcessOptions options_;
    mutable ar::core::Error error_;
};
} // namespace ar::control
