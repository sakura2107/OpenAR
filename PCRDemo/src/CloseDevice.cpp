#include <CloseDevice.h>

void pcr::close_device(pcr::global_params& params){
    do{
        params.device_controller->shutdownDevice(params.index);
    }while(ar::ARDeviceError::AR_NO_ERROR == params.device_controller->checkDeviceState(params.index));
}