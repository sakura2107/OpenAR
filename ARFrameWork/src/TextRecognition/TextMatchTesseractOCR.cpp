#include <TextRecognition/TextMatchTesseractOCR.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

bool ar::wordMatchTesseractOCR(int* res,
	std::string& msg_res,
	const unsigned char* image,
	const std::string& text,
	const int& image_width,
	const int& image_height,
	const std::string& language)
{
	tesseract::TessBaseAPI* ocr = new tesseract::TessBaseAPI();
	if (ocr->Init("./TesseractOCRData", language.c_str()) == -1) {
		msg_res = "Could not initialize tesseract !";
		ocr->End();
		delete ocr;
		return false;
	}

	ocr->SetImage(image, image_width, image_height, 1, image_width);

	ocr->Recognize(0);
	tesseract::ResultIterator* ri = ocr->GetIterator();
	tesseract::PageIteratorLevel level = tesseract::RIL_WORD;

    if (ri != 0) {
		do {
			const char* word = ri->GetUTF8Text(level);
			float conf = ri->Confidence(level);

			if (word != nullptr && checkForMatch(text, std::string(word))) {
				ri->BoundingBox(level, &res[0], &res[1], &res[2], &res[3]);
				delete word;
				return true;
			}
		} while (ri->Next(level));
    }
	return true;
}

bool ar::checkForMatch(const std::string& target, const std::string& foundText) {
    return foundText.find(target) != std::string::npos;
}