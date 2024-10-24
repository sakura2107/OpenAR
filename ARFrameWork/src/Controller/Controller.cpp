#include <Controller/Controller.h>
#include <Controller/MuMuController.h>
#include <Controller/AdbController.h>

std::unique_ptr<ar::Controller> ar::ControllerFactory::createController(ar::controllerParams& params, ar::controllerType type){
    switch(type){
        case(ar::controllerType::MUMU) :
            return std::make_unique<ar::MuMuController>(params.adb_path, params.adb_port, params.mumu_path, params.mumu_index);
        case(ar::controllerType::ADB) :
            return std::make_unique<ar::AdbController>(params.adb_path, params.adb_port);
    }
    //add other controller maybe ...
    return nullptr;
}