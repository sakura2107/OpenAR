#include <DeviceController/DeviceController.h>
#include <DeviceController/MuMuDeviceController.h>

std::unique_ptr<ar::DeviceController> ar::DeviceControllerFactory::createDeviceController
(ar::deviceControllerType type, std::string device_path)
{
    switch(type){
        case(ar::deviceControllerType::MUMU) :
            return std::make_unique<ar::MuMuDeviceController>(device_path);
    }
    //add other device controller maybe ...
    return nullptr;
}