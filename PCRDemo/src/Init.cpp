#include <Init.h>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <Log/MiniLog.h>
#include <stdlib.h>

pcr::global_params::global_params(const std::string device_path,
                                  const int index,
                                  std::string adb_path,
                                  int adb_port,
                                  ar::controllerType controller_type,
                                  ar::imageRecognitionType image_recognition_type,
                                  ar::deviceControllerType m_deviceControllerType):
                                  device_path(device_path),
                                  index(index),
                                  adb_path(adb_path),
                                  adb_port(adb_port),
                                  controller_type(controller_type),
                                  image_recognition_type(image_recognition_type),
                                  device_controller_type(m_deviceControllerType)
{
    ar::controllerParams params;
    //mumu
    
    if (controller_type == ar::controllerType::MUMU) {
        params.adb_path = adb_path;
        params.adb_port = adb_port;
        size_t wchar_size = strlen(device_path.c_str()) + 1;
        wchar_t* w_device_path = new wchar_t[wchar_size];
        size_t convertedChars = 0;
        mbstowcs_s(&convertedChars, w_device_path, wchar_size, device_path.c_str(), _TRUNCATE);
        params.mumu_path = w_device_path;
        params.mumu_index = index;
    }
    
    if (controller_type == ar::controllerType::ADB) {
        params.adb_path = adb_path;
        params.adb_port = adb_port;
    }

    ar::ControllerFactory controller_factory;
    controller = controller_factory.createController(params, controller_type);

    ar::DeviceControllerFactory device_controller_factory;
    device_controller = device_controller_factory.createDeviceController(m_deviceControllerType, device_path);

    ar::ImageRecognitionFactory image_recognition_factory;
    image_recognition = image_recognition_factory.createIamgeRecognition(image_recognition_type);
}

void pcr::checkImageCompareRate(ar::imageRecognitionType image_recognition_type) {
    cv::Mat image = cv::imread("./res/test/1.png");
    cv::Mat temp = cv::imread("./res/test/2.png");
    ar::point p;
    ar::ImageRecognitionFactory image_recognition_factory;
    auto image_recognition = image_recognition_factory.createIamgeRecognition(image_recognition_type);
    auto start = std::chrono::high_resolution_clock::now();
    p = image_recognition->compareImageReturnCentrePoint(image, temp, 0.95f);
    auto end = std::chrono::high_resolution_clock::now();
    if(!p.is_empty) ar::info("x: {} y: {}", p.x, p.y);
    std::chrono::duration<double, std::milli> duration = end - start;
    switch (image_recognition_type) {
    case(ar::imageRecognitionType::MPR):
        ar::info("MPR Match two image use {} ms !", (int)duration.count());
        break;
#if defined(ENABLE_CUDA)
    case(ar::imageRecognitionType::MPR_CUDA):
        ar::info("MPR_CUDA Match two image use {} ms !", (int)duration.count());
        break;
#endif
    case(ar::imageRecognitionType::PSR):
        ar::info("PSR Match two image use {} ms !", (int)duration.count());
        break;
    //add other compare image method maybe ...
    }
}