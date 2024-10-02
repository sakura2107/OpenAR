#pragma once
#include <DeviceController/DeviceParams.hpp>
#include <memory>

namespace ar{

class DeviceController{
public:
    virtual ~DeviceController() {}

    virtual ARDeviceError getDeviceInfo(const int& index, deviceInfo& info) = 0;

    virtual ARDeviceError checkDeviceState(const int& index) = 0;

    virtual ARDeviceError checkIsDeviceIndexValid(const int& index) = 0;

    virtual ARDeviceError lauchDevice(const int& index) = 0;

    virtual ARDeviceError shutdownDevice(const int& index) = 0;

    virtual ARDeviceError restartDevice(const int& index) = 0;

    virtual ARDeviceError showWindow(const int& index) = 0;

    virtual ARDeviceError hideWindow(const int& index) = 0;

    virtual ARDeviceError lauchApp(const int& index, const std::string& app_name) = 0;

    virtual ARDeviceError closeApp(const int& index, const std::string& app_name) = 0;

    virtual ARDeviceError getAppState(const int& index, const std::string& app_name, appInfo& info) = 0;
};

class DeviceControllerFactory{
public:
    std::unique_ptr<DeviceController> createDeviceController(deviceControllerType type, std::string device_path);
};

}
