#include <TextRecognition/TesseractOCR.h>
#include <Log/MiniLog.h>
#include <TextRecognition/TextMatchTesseractOCR.h>

ar::textPoint ar::tesseractOCR::compareWordReturnCentrePoints(cv::Mat& image, const std::string& text, ar::OCRLanguage language) {
	ar::textPoint point;
	if (image.empty()) {
		ar::error("Image empty !");
		return point;
	}
	if(image.channels() != 1) cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
	bool err = false;
	std::string language_str = "eng";
	std::string msg_res = "";
	int* res = new int[4];
	res[0] = -1; res[1] = -1; res[2] = -1; res[3] = -1;
	switch (language) {
	case(ar::OCRLanguage::ENGLISH):
		language_str = "eng";
	case(ar::OCRLanguage::SIMPLE_CHINESE):
		language_str = "chi_sim";
	}
		
	if (!ar::wordMatchTesseractOCR(res, msg_res, image.data, text, image.cols, image.rows, language_str)) {
		delete[] res;
		ar::error("{}", msg_res);
		return point;
	}

	if (res[0] != -1 && res[1] != -1 && res[2] != -1 && res[3] != -1)
		point = { (res[0] + res[2]) / 2, (res[1] + res[3]) / 2, false };
	delete[] res;
	return point;
}