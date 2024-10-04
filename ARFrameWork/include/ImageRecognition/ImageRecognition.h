#pragma once
#include <memory>
#include <ImageRecognition/ImageParams.hpp>
#include <opencv2/opencv.hpp>

namespace ar{

class ImageRecognition{
public:
    virtual ~ImageRecognition() {}
    //return a centre point
    virtual point compareImageReturnCentrePoint(cv::Mat& image, cv::Mat& temp, const float& threshold) = 0;

    virtual point compareImageReturnCentrePoint(const std::string& image_path, const std::string& temp_path, const float& threshold) = 0;
};

class ImageRecognitionFactory{
public:
    std::unique_ptr<ImageRecognition> createIamgeRecognition(ar::imageRecognitionType type);
};

}