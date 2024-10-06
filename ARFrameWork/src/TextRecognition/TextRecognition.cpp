#include <TextRecognition/TextRecognition.h>
#include <TextRecognition/TesseractOCR.h>

std::unique_ptr<ar::textRecognition> ar::textRecognitionFactory::createTextRecognitionFactory(ar::textRecognitionType type) {
	switch (type) {
	case(ar::textRecognitionType::TESSERACT_OCR):
		return std::make_unique<ar::tesseractOCR>();
	}
}