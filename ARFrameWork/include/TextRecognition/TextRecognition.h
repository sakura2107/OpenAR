#pragma once
#include <TextRecognition/TextParams.hpp>
#include <memory>
#include <opencv2/opencv.hpp>

namespace ar {
	class textRecognition {
	public:
		virtual ~textRecognition(){}

		virtual ar::textPoint compareWordReturnCentrePoints(cv::Mat& image, const std::string& str, OCRLanguage language) = 0;
	};

	class textRecognitionFactory {
	public:
		std::unique_ptr<textRecognition> createTextRecognitionFactory(textRecognitionType type);
	};
}
