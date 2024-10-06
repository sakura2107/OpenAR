#pragma once
#include <opencv2/opencv.hpp>
#include <ImageRecognition/ImageParams.hpp>
#include <ImageRecognition/ImageRecognition.h>

namespace ar {

    class PrefixSumRecognition : public ImageRecognition {
    public:
        //return a centre point
        point compareImageReturnCentrePoint(cv::Mat& image, cv::Mat& temp, const float& threshold = 0.95f) override;

        point compareImageReturnCentrePoint(const std::string& image_path, const std::string& temp_path, const float& threshold = 0.95f) override;

    };

}