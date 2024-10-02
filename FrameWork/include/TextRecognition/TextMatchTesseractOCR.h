#pragma once
#include <string>

namespace ar {
	bool wordMatchTesseractOCR(int* res,
		std::string& msg_res,
		const unsigned char* image,
		const std::string& text,
		const int& image_width,
		const int& image_height,
		const std::string& language);

	bool checkForMatch(const std::string& target, const std::string& foundText);
}