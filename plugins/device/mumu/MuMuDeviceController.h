#pragma once
// Internal header: "mumu" backend of IDeviceController, driven through the
// MuMuManager.exe CLI.
#include <chrono>
#include <filesystem>
#include <string>

#include "device/DeviceController.h"
#include "log/Logger.h"

namespace ar::device {

class MuMuDeviceController final : public IDeviceController {
  public:
    MuMuDeviceController() = delete;
    MuMuDeviceController(const std::filesystem::path &mumu_path, int32_t index,
                         ar::log::ILogger *logger = nullptr);
    ~MuMuDeviceController() override = default;

    std::string_view name() const override { return "mumu"; }

    Status check_device_state() override;
    Status launch() override;
    Status shutdown() override;
    Status restart() override;
    Status show_window() override;
    Status hide_window() override;
    Status launch_app(std::string_view package) override;
    Status close_app(std::string_view package) override;
    AppState app_state(std::string_view package) override;

  private:
    // MuMuManager actions that follow the `control -v <index> <action>` shape.
    // `check_state` mirrors the original per-action pre-checks: launch/restart
    // only validated the index, shutdown/show/hide/app ops also required the
    // instance to be online.
    Status control(std::string_view action, bool check_state);

    // Runs `MuMuManager info -v <index>` and caches the parsed result for
    // kInfoTtl (each process spawn costs tens of milliseconds and the old
    // code ran it two to three times per operation).
    bool refresh_info(bool force);
    bool info_valid() const { return info_valid_; }
    bool info_process_started() const { return process_started_; }

    std::filesystem::path manager_path() const;

    std::filesystem::path mumu_path_;
    int32_t index_ = 0;
    ar::log::ILogger *logger_ = nullptr;

    static constexpr std::chrono::seconds kInfoTtl{3};
    std::chrono::steady_clock::time_point info_time_{};
    bool info_cached_ = false;
    bool info_valid_ = false;
    bool process_started_ = false;
};

} // namespace ar::device
