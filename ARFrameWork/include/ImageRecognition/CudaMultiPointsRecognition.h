#pragma once
#include <ImageRecognition/ImageRecognition.h>
#include <ImageRecognition/ImageParams.hpp>

namespace ar {

class CudaMultiPointsRecognition : public ImageRecognition{
public:
	point compareImageReturnCentrePoint(cv::Mat& image, cv::Mat& temp, const float& threshold = 0.95f) override;

	point compareImageReturnCentrePoint(const std::string& image_path, const std::string& temp_path, const float& threshold = 0.95f) override;

	int num_points = 32;
};

}