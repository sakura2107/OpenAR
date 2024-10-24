#pragma once

namespace ar{
	enum struct textRecognitionType : int {
		TESSERACT_OCR,
	};

	enum struct OCRLanguage : int {
		SIMPLE_CHINESE,
		ENGLISH,
	};
	struct textPoint {
		int x = -1;
		int y = -1;
		bool is_empty = true;
	};
}
