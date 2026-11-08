#pragma once
// Internal to the ocr plugins. The complete PaddleOCR pipeline (det + rec)
// behind IOcrEngine; the cpu/gpu plugins differ only in the OrtDevice they
// pass in and the backend name they register under.
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>

#include <opencv2/opencv.hpp>

#include "TextDetector.h"
#include "TextRecognizer.h"
#include "log/Logger.h"
#include "vision/Ocr.h"

namespace ar::paddle {

class PaddleOcrEngine final : public ar::vision::IOcrEngine {
  public:
    PaddleOcrEngine() = delete;
    PaddleOcrEngine(std::string_view backend_name, const ar::vision::OcrParams &params, OrtDevice device,
                    ar::log::ILogger *logger = nullptr)
        : name_(backend_name), logger_(logger) {
        const auto config = resolve_config(params);
        detector_ = std::make_unique<TextDetector>(config.params, device, logger, config.det);
        recognizer_ = std::make_unique<TextRecognizer>(config.params, device, logger, config.rec);
        if (config.separate_numbers)
            number_recognizer_ = std::make_unique<TextRecognizer>(config.number_params, device, logger, config.number_rec);
        if (config.number_fallback) {
            number_fallback_ = std::make_unique<TextRecognizer>(config.fallback_params, device, logger, config.fallback_rec);
            fallback_below_confidence_ = config.fallback_below_confidence;
            fallback_on_invalid_syntax_ = config.number_fallback_on_invalid_syntax;
        }
        if (config.number_second_fallback)
            number_second_fallback_=std::make_unique<TextRecognizer>(config.second_fallback_params,device,logger,config.second_fallback_rec);
    }

    std::string_view name() const override { return name_; }

    std::vector<ar::vision::OcrItem> recognize(const cv::Mat &image, const cv::Rect *roi) override {
        double detection_ms = 0, recognition_ms = 0;
        return recognize_mode(image, roi, 0, detection_ms, recognition_ms);
    }

    // mode 0: det+rec, 1: already-cropped text, 2: already-cropped numbers, 3: det only.
    std::vector<ar::vision::OcrItem> recognize_mode(const cv::Mat &image, const cv::Rect *roi, int mode,
                                                  double &detection_ms, double &recognition_ms) {
        detection_ms = recognition_ms = 0;
        if (mode < 0 || mode > 3) throw std::invalid_argument("Invalid OCR mode");
        std::vector<ar::vision::OcrItem> items;
        if (image.empty() || !detector_ || !recognizer_)
            return items;

        cv::Mat input = image;
        cv::Rect range;
        if (roi) {
            if (roi->x < 0 || roi->y < 0 || roi->width <= 0 || roi->height <= 0 ||
                roi->x + roi->width > image.cols || roi->y + roi->height > image.rows) {
                AR_LOG(logger_, ar::log::Level::error, "OCR range is outside image bounds");
                throw std::invalid_argument("OCR range is outside image bounds");
            }
            range = *roi;
            input = image(range);
        }
        if (input.channels() == 1) cv::cvtColor(input, input, cv::COLOR_GRAY2BGR);
        else if (input.channels() == 4) cv::cvtColor(input, input, cv::COLOR_BGRA2BGR);
        if (input.type() != CV_8UC3) throw std::invalid_argument("OCR input must be uint8 BGR/gray/BGRA");

        std::vector<cv::Mat> crops;
        std::vector<cv::Rect> boxes;
        auto started = std::chrono::steady_clock::now();
        if (mode == 0 || mode == 3) {
            if (!detector_->detect(crops, boxes, input)) throw std::runtime_error("OCR detection failed");
            detection_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        } else {
            crops.push_back(input);
            boxes.emplace_back(0, 0, input.cols, input.rows);
        }
        if (crops.empty())
            return items;

        std::vector<std::string> texts;
        std::vector<float> scores;
        if (mode == 3) {
            texts.resize(boxes.size()); scores.resize(boxes.size(), 1);
        } else {
            started = std::chrono::steady_clock::now();
            auto *recognizer = mode == 2 && number_recognizer_ ? number_recognizer_.get() : recognizer_.get();
            const auto original_boxes = boxes;
            if (!recognizer->recognize(texts, boxes, scores, input, crops, mode == 2)) throw std::runtime_error("OCR recognition failed");
            if (mode == 2 && number_fallback_ && (scores.empty() || scores.front() < fallback_below_confidence_ ||
                (fallback_on_invalid_syntax_ && (texts.empty() || !numeric_syntax_valid(texts.front()))))) {
                boxes = original_boxes;
                if (!number_fallback_->recognize(texts, boxes, scores, input, crops, true))
                    throw std::runtime_error("Numeric fallback recognition failed");
            }
            if (mode==2 && number_second_fallback_ && (texts.empty() || scores.empty() || !numeric_syntax_valid(texts.front()))) {
                boxes=original_boxes;
                if (!number_second_fallback_->recognize(texts,boxes,scores,input,crops,true))
                    throw std::runtime_error("Second numeric fallback recognition failed");
            }
            recognition_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        }

        if (roi) {
            for (auto &box : boxes) {
                box.x += range.x;
                box.y += range.y;
            }
        }

        items.reserve(texts.size());
        for (size_t i = 0; i < texts.size(); ++i) {
            ar::vision::OcrItem item;
            item.text = texts[i];
            item.score = i < scores.size() ? scores[i] : 0.0f;
            item.box = i < boxes.size() ? boxes[i] : cv::Rect();
            items.push_back(std::move(item));
        }
        return items;
    }

  private:
    std::string name_;
    ar::log::ILogger *logger_ = nullptr;
    std::unique_ptr<TextDetector> detector_;
    std::unique_ptr<TextRecognizer> recognizer_;
    std::unique_ptr<TextRecognizer> number_recognizer_;
    std::unique_ptr<TextRecognizer> number_fallback_;
    std::unique_ptr<TextRecognizer> number_second_fallback_;
    float fallback_below_confidence_ = 0.85f;
    bool fallback_on_invalid_syntax_ = false;
};

} // namespace ar::paddle
