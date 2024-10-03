#include <LaunchDevice.h>
#include <windows.h>
#include <Log/MiniLog.h>

void pcr::launchDevice(pcr::global_params& params){
    ar::info("Launch MuMu {}", params.index);
    ar::appInfo info;
    do{
        Sleep(params.operate_duration);
        if(params.device_controller->lauchDevice(params.index)!= ar::ARDeviceError::AR_NO_ERROR)std::exit(0);
    }while(params.device_controller->checkDeviceState(params.index) != ar::ARDeviceError::AR_NO_ERROR);
    //是否静默运行，静默运行会将模拟器隐藏
    if(params.start_with_silence) params.device_controller->hideWindow(params.index);
    
    do{
        Sleep(params.operate_duration);
        if (params.device_controller->lauchApp(params.index, "com.bilibili.priconne") != ar::ARDeviceError::AR_NO_ERROR)std::exit(0);
        params.device_controller->getAppState(params.index, "com.bilibili.priconne", info);
    }while(!info.is_running);
}