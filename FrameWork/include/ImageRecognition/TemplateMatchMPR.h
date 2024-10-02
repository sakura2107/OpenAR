#pragma once
#include <vector>
#include <opencv2/opencv.hpp>
#include <ImageRecognition/ImageParams.hpp>

namespace ar {

	bool multiPointsRecognition(int* res, 
		std::string& res_msg,
		unsigned char* image,
		unsigned char* temp,
		const int& image_width,
		const int& image_height,
		const int& temp_width,
		const int& temp_height,
		const float& threshold = 0.95f,
		const int& num_points = 36);

	namespace detail {

		void makePointsRandom(int* points, unsigned char* temp, const int& temp_width, const int& temp_height, const int& num_points);

		void findBestDivision(int* res, const int& num);

	}

}