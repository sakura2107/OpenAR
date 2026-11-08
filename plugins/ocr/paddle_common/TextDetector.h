#pragma once
#include <memory>
#include <vector>

#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

#include "log/Logger.h"
#include "vision/Ocr.h"
#include "OrtSession.hpp"
#include "OcrConfig.h"
#include "ppocr/postprocess_op.h"

namespace ar::paddle {

// DBNet text detection. Finds text regions in the image and returns the
// rotated-rect crops together with their axis-aligned bounding boxes.
// Shared unchanged by the paddle-ocr-cpu / paddle-ocr-gpu plugins; the only
// difference between the two is the ONNX Runtime execution provider.
class TextDetector {
  public:
    TextDetector() = delete;
    TextDetector(const ar::vision::OcrParams &params, OrtDevice device, ar::log::ILogger *logger = nullptr,
                 DetConfig config = {});
    ~TextDetector() = default;

    [[nodiscard]] bool detect(std::vector<cv::Mat> &crops, std::vector<cv::Rect> &boxes,
                              const cv::Mat &image);

  private:
    void preprocess();
    bool infer();
    bool postprocess(std::vector<cv::Mat> &crops, std::vector<cv::Rect> &boxes);

    ar::vision::OcrParams params_;
    ar::log::ILogger *logger_ = nullptr;
    std::unique_ptr<OrtSession> session_;
    DetConfig config_;
    std::vector<int64_t> model_input_shape_;

    cv::Mat image_;
    cv::Mat blob_;
    std::vector<int64_t> input_shape_;
    std::vector<Ort::Value> input_tensor_;
    std::vector<Ort::Value> output_tensor_;

    PaddleOCR_::DBPostProcessor post_processor_;
};

} // namespace ar::paddle
