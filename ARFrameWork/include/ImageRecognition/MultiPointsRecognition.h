#pragma once
#include <opencv2/opencv.hpp>
#include <ImageRecognition/ImageParams.hpp>
#include <ImageRecognition/ImageRecognition.h>

namespace ar{

class MultiPointsRecognition : public ImageRecognition{
public:
    //return a centre point
    point compareImageReturnCentrePoint(cv::Mat& large_img, cv::Mat& small_img, const float& threshold = 0.95f) override;

    point compareImageReturnCentrePoint(const std::string& large_img_path, const std::string& small_img_path, const float& threshold = 0.95f) override;

    const unsigned int num_points = 32;
};

}