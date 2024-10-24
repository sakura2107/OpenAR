#pragma once
#include <string>

namespace ar {

	bool templateMatchPSR(int* res, 
		std::string& res_msg,
		unsigned char* image,
		unsigned char* temp,
		const int& image_width,
		const int& image_height,
		const int& temp_width,
		const int& temp_height,
		const float& threshold = 0.95f);

	namespace detail {

		void generateImagePrefixSumArray(int* dst_array, unsigned char* image, const int& image_width, const int& image_height);

	}

}