#pragma once
#include <string>
#include <memory>
#include <DeviceController/DeviceParams.hpp>
#include <DeviceController/DeviceController.h>

namespace ar{

class MuMuDeviceController : public DeviceController{
private:
    std::string mumu_path;

    constexpr std::string getMuMuManagerPath(std::string path);

    ARDeviceError checkIsCommandValid(std::string cmd);
public:
    MuMuDeviceController(const std::string mumu_path);

    ARDeviceError getDeviceInfo(const int& index, deviceInfo& info) override;

    ARDeviceError checkDeviceState(const int& index) override;

    ARDeviceError checkIsDeviceIndexValid(const int& index) override;

    ARDeviceError lauchDevice(const int& index) override;

    ARDeviceError shutdownDevice(const int& index) override;

    ARDeviceError restartDevice(const int& index) override;

    ARDeviceError showWindow(const int& index) override;

    ARDeviceError hideWindow(const int& index) override;

    ARDeviceError lauchApp(const int& index, const std::string& app_name) override;

    ARDeviceError closeApp(const int& index, const std::string& app_name) override;

    ARDeviceError getAppState(const int& index, const std::string& app_name, appInfo&) override;
};

}