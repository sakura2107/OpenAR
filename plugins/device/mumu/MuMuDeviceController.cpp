#include "MuMuDeviceController.h"

#include <chrono>
#include <format>

#include <rapidjson/document.h>

#include "core/Cmd.hpp"

namespace ar::device {

namespace {

// The manager prints JSON; anything without a '{' means the command itself
// failed (old CheckIsCommandValid semantics).
bool command_output_is_json(const std::string &output) {
    return output.find("{") != std::string::npos;
}

} // namespace

MuMuDeviceController::MuMuDeviceController(const std::filesystem::path &mumu_path, int32_t index,
                                           ar::log::ILogger *logger)
    : mumu_path_(mumu_path), index_(index), logger_(logger) {}

std::filesystem::path MuMuDeviceController::manager_path() const {
    return mumu_path_ / "nx_main" / "MuMuManager.exe";
}

bool MuMuDeviceController::refresh_info(bool force) {
    const auto now = std::chrono::steady_clock::now();
    if (!force && info_cached_ && now - info_time_ < kInfoTtl)
        return info_valid_;

    info_cached_ = true;
    info_time_ = now;
    info_valid_ = false;
    process_started_ = false;

    const std::string args = std::format("info -v {}", index_);
    std::string output;
    AR_LOG(logger_, ar::log::Level::debug,
           std::format("{} {}", ar::core::wide_to_utf8(manager_path().wstring()), args));
    if (!ar::core::run_process(ar::core::wide_to_utf8(manager_path().wstring()), args, &output))
        return false;
    if (!command_output_is_json(output))
        return false;

    rapidjson::Document doc;
    doc.Parse(output.c_str());
    if (doc.HasParseError() || !doc.IsObject())
        return false;

    // `error_code == 0` (or its absence) marks a valid instance; the manager
    // reports -200 for unknown indexes.
    if (doc.HasMember("error_code")) {
        if (!doc["error_code"].IsInt())
            return false;
        if (doc["error_code"].GetInt() == -200)
            return false;
        if (doc["error_code"].GetInt() != 0)
            return false;
    }
    info_valid_ = true;
    if (doc.HasMember("is_process_started") && doc["is_process_started"].IsBool())
        process_started_ = doc["is_process_started"].GetBool();
    return true;
}

Status MuMuDeviceController::check_device_state() {
    if (!refresh_info(false)) {
        AR_LOG(logger_, ar::log::Level::error, std::format("Invalid MuMu index : {}", index_));
        return Status::invalid_index;
    }
    return process_started_ ? Status::ok : Status::offline;
}

Status MuMuDeviceController::control(std::string_view action, bool check_state) {
    if (!refresh_info(check_state)) {
        AR_LOG(logger_, ar::log::Level::error, std::format("Invalid MuMu index : {}", index_));
        return Status::invalid_index;
    }
    if (check_state && !process_started_) {
        AR_LOG(logger_, ar::log::Level::error, std::format("MuMu index : {} offline !", index_));
        return Status::offline;
    }

    const std::string args = std::format("control -v {} {}", index_, action);
    std::string output;
    if (!ar::core::run_process(ar::core::wide_to_utf8(manager_path().wstring()), args, &output) ||
        !command_output_is_json(output)) {
        AR_LOG(logger_, ar::log::Level::error, "Cmd invalid !");
        return Status::invalid_command;
    }

    rapidjson::Document doc;
    doc.Parse(output.c_str());
    if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("errcode") || !doc["errcode"].IsInt() ||
        doc["errcode"].GetInt() != 0) {
        AR_LOG(logger_, ar::log::Level::error, std::format("Unknown error in MuMu {} !", index_));
        return Status::unknown;
    }

    AR_LOG(logger_, ar::log::Level::info, std::format("MuMu {} action '{}' ok", index_, action));
    return Status::ok;
}

Status MuMuDeviceController::launch() {
    return control("launch", false);
}

Status MuMuDeviceController::shutdown() {
    return control("shutdown", true);
}

Status MuMuDeviceController::restart() {
    return control("restart", false);
}

Status MuMuDeviceController::show_window() {
    return control("showWindow", true);
}

Status MuMuDeviceController::hide_window() {
    return control("hideWindow", true);
}

Status MuMuDeviceController::launch_app(std::string_view package) {
    return control(std::format("app launch -pkg {}", package), true);
}

Status MuMuDeviceController::close_app(std::string_view package) {
    return control(std::format("app close -pkg {}", package), true);
}

AppState MuMuDeviceController::app_state(std::string_view package) {
    AppState state;
    if (!refresh_info(false)) {
        AR_LOG(logger_, ar::log::Level::error, std::format("Invalid MuMu index : {}", index_));
        return state;
    }
    if (!process_started_) {
        AR_LOG(logger_, ar::log::Level::error, std::format("MuMu index : {} offline !", index_));
        return state;
    }

    const std::string args = std::format("control -v {} app info -pkg {}", index_, package);
    std::string output;
    if (!ar::core::run_process(ar::core::wide_to_utf8(manager_path().wstring()), args, &output) ||
        !command_output_is_json(output))
        return state;

    rapidjson::Document doc;
    doc.Parse(output.c_str());
    if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("state") || !doc["state"].IsString())
        return state;

    const std::string state_text = doc["state"].GetString();
    if (state_text == "running") {
        state.installed = true;
        state.running = true;
    } else if (state_text == "stopped") {
        state.installed = true;
    }
    return state;
}

} // namespace ar::device
