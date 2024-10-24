#pragma once
#include <string>
#include <Controller/Controller.h>
#include <ImageRecognition/ImageRecognition.h>
#include <DeviceController/DeviceController.h>
#include <Data/ARData.h>

class ARLGlobalParams {
public:
    ARLGlobalParams() = delete;
    ARLGlobalParams(ARProject* project_data);
    ~ARLGlobalParams(){}
public:
    std::string device_path = "";
    int index;
    std::string adb_path;
    int adb_port;
    ar::controllerType controller_type;
    ar::imageRecognitionType image_recognition_type;
    ar::deviceControllerType device_controller_type;

    bool start_with_silence = false;
    int duration_time = 200; //ms

    std::unique_ptr<ar::Controller> controller;
    std::unique_ptr<ar::DeviceController> device_controller;
    std::unique_ptr<ar::ImageRecognition> image_recognition;
private:
    ARProject* project_data = nullptr;
    void Init();
};