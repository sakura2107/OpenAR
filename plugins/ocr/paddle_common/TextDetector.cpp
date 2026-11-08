#include "TextDetector.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <format>

namespace ar::paddle {

namespace {

// Axis-aligned bounding box of a detector quad (ppocr box = 4 points).
cv::Rect quad_bbox(const std::vector<std::vector<int>> &quad) {
    int x_min = INT_MAX;
    int y_min = INT_MAX;
    int x_max = INT_MIN;
    int y_max = INT_MIN;
    for (const auto &point : quad) {
        x_min = std::min(x_min, point[0]);
        y_min = std::min(y_min, point[1]);
        x_max = std::max(x_max, point[0]);
        y_max = std::max(y_max, point[1]);
    }
    return {x_min, y_min, x_max - x_min, y_max - y_min};
}

// Two boxes belong to one visual line when their vertical overlap covers most
// of the smaller box. `gap_ratio` bounds how far apart they may sit
// horizontally: a fraction of the taller box, enough for the probability-map
// dips that split a line, far below the spacing between separate UI elements.
bool on_same_line(const cv::Rect &a, const cv::Rect &b, float gap_ratio) {
    const int overlap_top = std::max(a.y, b.y);
    const int overlap_bottom = std::min(a.y + a.height, b.y + b.height);
    const int overlap = std::max(0, overlap_bottom - overlap_top);
    if (overlap * 2 < std::min(a.height, b.height))
        return false;
    const int gap = std::max(a.x, b.x) - std::min(a.x + a.width, b.x + b.width);
    return gap <= 0 || gap <= gap_ratio * std::max(a.height, b.height);
}

} // namespace

TextDetector::TextDetector(const ar::vision::OcrParams &params, OrtDevice device, ar::log::ILogger *logger, DetConfig config)
    : params_(params), logger_(logger), config_(std::move(config)) {
    session_ = std::make_unique<OrtSession>(device, params_.det_model, params_.device_id, logger_, config_.threads, config_.optimize_all);
    auto &ort = session_->session();
    if (ort.GetInputCount() != 1 || ort.GetOutputCount() != 1) throw std::invalid_argument("Detector requires one input/output");
    auto input_type = ort.GetInputTypeInfo(0);
    const auto info = input_type.GetTensorTypeAndShapeInfo();
    auto output_type = ort.GetOutputTypeInfo(0);
    const auto output = output_type.GetTensorTypeAndShapeInfo();
    model_input_shape_ = info.GetShape();
    if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || model_input_shape_.size() != 4 ||
        model_input_shape_[1] != 3 || model_input_shape_[0] > 1 ||
        output.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || output.GetShape().size() != 4)
        throw std::invalid_argument("Detector must accept float32 NCHW batch 1, channels 3");
}

void TextDetector::preprocess() {
    double ratio = 1;
    if (config_.limit_side > 0) {
        const int side = config_.limit_min ? std::min(image_.cols, image_.rows) : std::max(image_.cols, image_.rows);
        if ((config_.limit_min && side < config_.limit_side) || (!config_.limit_min && side > config_.limit_side))
            ratio = static_cast<double>(config_.limit_side) / side;
    }
    auto dimension = [&](int original, int64_t fixed) {
        if (fixed > 0) return static_cast<int>(fixed);
        const int resized = std::max(1, static_cast<int>(original * ratio));
        return std::max(32, static_cast<int>(config_.ceil_resize ? std::ceil(resized / 32.0) : std::round(resized / 32.0)) * 32);
    };
    const int32_t new_width = dimension(image_.cols, model_input_shape_[3]);
    const int32_t new_height = dimension(image_.rows, model_input_shape_[2]);
    if (new_width > 8192 || new_height > 8192 || int64_t(new_width) * new_height > 16 * 1024 * 1024)
        throw std::invalid_argument("Detection input exceeds image budget");

    cv::Mat resized;
    cv::resize(image_, resized, cv::Size(new_width, new_height));
    resized = normalize_ocr(resized, config_.norm);
    blob_ = cv::dnn::blobFromImage(resized, 1, cv::Size(new_width, new_height), cv::Scalar(), false, false);

    input_shape_ = {1, 3, new_height, new_width};
    input_tensor_.clear();
    input_tensor_.emplace_back(Ort::Value::CreateTensor<float>(
        Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU), blob_.ptr<float>(), blob_.total(),
        input_shape_.data(), input_shape_.size()));
}

bool TextDetector::infer() {
    if (!session_ || !session_->ok() || input_tensor_.size() != session_->input_names().size())
        return false;
    try {
        output_tensor_ = session_->session().Run(
            Ort::RunOptions{nullptr}, session_->input_names().data(), input_tensor_.data(),
            input_tensor_.size(), session_->output_names().data(), session_->output_names().size());
    } catch (const Ort::Exception &e) {
        AR_LOG(logger_, ar::log::Level::error,
               std::format("PaddleOCR detector inference failed: {}", e.what()));
        return false;
    }
    return !output_tensor_.empty();
}

bool TextDetector::postprocess(std::vector<cv::Mat> &crops, std::vector<cv::Rect> &boxes) {
    crops.clear();
    boxes.clear();
    if (output_tensor_.empty())
        return false;

    float *output_data = output_tensor_.front().GetTensorMutableData<float>();
    const auto output_shape = output_tensor_.front().GetTensorTypeAndShapeInfo().GetShape();
    if (output_shape.size() != 4 || output_shape[0] != 1 || output_shape[1] != 1 ||
        output_shape[2] <= 0 || output_shape[3] <= 0 || image_.empty())
        return false;

    const cv::Mat prob(static_cast<int32_t>(output_shape[2]), static_cast<int32_t>(output_shape[3]), CV_32F,
                       output_data);
    cv::Mat bitmap;
    cv::threshold(prob, bitmap, config_.bitmap_threshold, 255, cv::THRESH_BINARY);
    bitmap.convertTo(bitmap, CV_8UC1);

    const float ratio_h = static_cast<float>(prob.rows) / static_cast<float>(image_.rows);
    const float ratio_w = static_cast<float>(prob.cols) / static_cast<float>(image_.cols);
    auto regions = post_processor_.BoxesFromBitmap(prob, bitmap, params_.box_threshold, params_.unclip_ratio,
                                                   params_.score_mode);
    regions = post_processor_.FilterTagDetRes(regions, ratio_h, ratio_w, image_);

    // Re-join lines the detector split into neighbouring components: with a
    // small mid-line probability dip (stylised fonts over translucent or
    // animated backgrounds) DBNet emits e.g. "アップデー" and "ト中です。" as
    // two boxes, so text lookups never see the full string. Runs in image
    // coordinates, before reading-order sorting.
    if (params_.det_line_merge_ratio > 0.0f && regions.size() > 1) {
        bool merged = true;
        while (merged) {
            merged = false;
            for (size_t i = 0; i < regions.size() && !merged; ++i) {
                const cv::Rect box_i = quad_bbox(regions[i]);
                for (size_t j = i + 1; j < regions.size(); ++j) {
                    if (!on_same_line(box_i, quad_bbox(regions[j]), params_.det_line_merge_ratio))
                        continue;
                    const cv::Rect united = box_i | quad_bbox(regions[j]);
                    const int x1 = united.x;
                    const int y1 = united.y;
                    const int x2 = united.x + united.width;
                    const int y2 = united.y + united.height;
                    regions[i] = {{x1, y1}, {x2, y1}, {x2, y2}, {x1, y2}};
                    regions.erase(regions.begin() + static_cast<std::ptrdiff_t>(j));
                    merged = true;
                    break;
                }
            }
        }
    }

    std::vector<PaddleOCR_::OCRPredictResult> sorted_regions;
    sorted_regions.reserve(regions.size());
    for (const auto &region : regions)
        sorted_regions.push_back({region});
    PaddleOCR_::Utility::sorted_boxes(sorted_regions);
    for (const auto &region : sorted_regions) {
        const cv::Rect box = quad_bbox(region.box) & cv::Rect(0, 0, image_.cols, image_.rows);
        auto crop = PaddleOCR_::Utility::GetRotateCropImage(image_, region.box);
        if (box.empty() || crop.empty()) throw std::runtime_error("Detector produced an invalid text crop");
        crops.push_back(std::move(crop));
        boxes.push_back(box);
    }
    return true;
}

bool TextDetector::detect(std::vector<cv::Mat> &crops, std::vector<cv::Rect> &boxes, const cv::Mat &image) {
    if (image.empty() || image.type() != CV_8UC3 || !session_ || !session_->ok())
        return false;
    image_ = image;
    preprocess();
    if (!infer())
        return false;
    return postprocess(crops, boxes);
}

} // namespace ar::paddle
