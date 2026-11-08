#pragma once
#include <memory>
#include <string>
#include <vector>

#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

#include "Dictionary.h"
#include "log/Logger.h"
#include "vision/Ocr.h"
#include "OrtSession.hpp"
#include "OcrConfig.h"

namespace ar::paddle {

// CRNN text recognition over the crops produced by TextDetector. Recognizes
// all crops in one batch; texts/scores/boxes come out index-aligned (boxes
// that fail the score threshold are dropped).
// Shared unchanged by the paddle-ocr-cpu / paddle-ocr-gpu plugins.
class TextRecognizer {
  public:
    TextRecognizer() = delete;
    TextRecognizer(const ar::vision::OcrParams &params, OrtDevice device, ar::log::ILogger *logger = nullptr,
                   RecConfig config = {});
    ~TextRecognizer() = default;

    [[nodiscard]] bool recognize(std::vector<std::string> &texts, std::vector<cv::Rect> &boxes,
                                 std::vector<float> &scores, const cv::Mat &image,
                                 const std::vector<cv::Mat> &crops, bool numbers = false);

  private:
    void preprocess(const std::vector<cv::Mat> &crops);
    bool infer();
    bool postprocess(std::vector<std::string> &texts, std::vector<cv::Rect> &boxes,
                     std::vector<float> &scores);

    ar::vision::OcrParams params_;
    ar::log::ILogger *logger_ = nullptr;
    std::unique_ptr<OrtSession> session_;
    std::unique_ptr<Dictionary> dictionary_;
    RecConfig config_;
    std::vector<int64_t> model_input_shape_;
    std::vector<int> number_classes_;
    std::vector<int> katakana_classes_;
    std::vector<int> text_classes_;
    bool numbers_ = false;
    int64_t expected_classes_ = 0;

    int32_t input_width_ = 320;
    int32_t input_height_ = 48;
    cv::Mat blob_;
    std::vector<int64_t> input_shape_;
    std::vector<Ort::Value> input_tensor_;
    std::vector<Ort::Value> output_tensor_;
};

} // namespace ar::paddle
