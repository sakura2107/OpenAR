#pragma once
#include <TextRecognition/TextRecognition.h>
#include <opencv2/opencv.hpp>
#include <string>

namespace ar {
	class tesseractOCR : public textRecognition{
	public:
		textPoint compareWordReturnCentrePoints(cv::Mat& image, const std::string& str, OCRLanguage language) override;
	};
}