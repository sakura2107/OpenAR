#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <Controller/ControllerParams.hpp>
#include <memory>

namespace ar{

class Controller{
public:
    virtual ~Controller(){};
    virtual bool initialize() = 0;
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool click(const int x, const int y) = 0;
    virtual bool longClick(const int x, const int y, const int ms_duration) = 0;
    virtual bool inputKey(const int key_code) = 0;
    virtual bool swipe(const int x_1, const int y_1, const int x_2, const int y_2, const int ms_duration) = 0;
    virtual bool screencap(cv::Mat& image_) = 0;
};

class ControllerFactory{
public:
    std::unique_ptr<Controller> createController(controllerParams& params, controllerType type);
};

}
