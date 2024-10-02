#pragma once
#include <Init.h>
#include <memory>
#include <DeviceController/DeviceController.h>

namespace pcr{

struct close_device_params{
public:
    bool is_close_device = true;

    close_device_params(bool is_close_device);
};

void close_device(global_params& params);

}