#include "TextRecognizer.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>

namespace ar::paddle {
TextRecognizer::TextRecognizer(const ar::vision::OcrParams &params, OrtDevice device,
                               ar::log::ILogger *logger, RecConfig config)
    : params_(params), logger_(logger), config_(std::move(config)) {
    dictionary_ = std::make_unique<Dictionary>(logger_);
    if (!dictionary_->load(params_.dictionary)) throw std::runtime_error("OCR dictionary cannot be loaded");
    session_ = std::make_unique<OrtSession>(device, params_.rec_model, params_.device_id, logger_, config_.threads, config_.optimize_all);
    auto &ort = session_->session();
    if (ort.GetInputCount() != 1 || ort.GetOutputCount() != 1)
        throw std::invalid_argument("Recognizer requires one NCHW input and one NTC output");
    auto input_type = ort.GetInputTypeInfo(0), output_type = ort.GetOutputTypeInfo(0);
    const auto input = input_type.GetTensorTypeAndShapeInfo(), output = output_type.GetTensorTypeAndShapeInfo();
    model_input_shape_ = input.GetShape();
    const auto out_shape = output.GetShape();
    if (input.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
        output.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || model_input_shape_.size() != 4 ||
        model_input_shape_[1] != 3 || out_shape.size() != 3)
        throw std::invalid_argument("Recognizer model must use float32 NCHW(3 channels) -> NTC");
    const auto height = model_input_shape_[2];
    if (height > 0) {
        if (config_.height && config_.height != height) throw std::invalid_argument("Configured recognition height conflicts with ONNX");
        input_height_ = static_cast<int>(height);
    } else {
        input_height_ = config_.height ? config_.height : config_.dynamic_height;
        if (!input_height_) throw std::invalid_argument("Dynamic ONNX height requires an explicit profile height");
    }
    if (input_height_ < 8 || input_height_ > 256 || model_input_shape_[0] > 32 || model_input_shape_[3] > config_.max_width)
        throw std::invalid_argument("Recognizer model exceeds configured input budget");
    expected_classes_ = static_cast<int64_t>(dictionary_->size()) + 1 + (config_.use_space ? 1 : 0);
    if (out_shape[2] > 0 && out_shape[2] != expected_classes_)
        throw std::invalid_argument(std::format("OCR dictionary/model mismatch: output={} expected={} (rows={} blank=1 space={})",
            out_shape[2], expected_classes_, dictionary_->size(), config_.use_space));
    number_classes_.push_back(0);
    katakana_classes_.push_back(0);
    text_classes_.push_back(0);
    std::string found_digits;
    for (int64_t index = 1; index < expected_classes_; ++index) {
        const auto token = index <= static_cast<int64_t>(dictionary_->size()) ? dictionary_->get_char(index - 1) : " ";
        const bool allowed = !config_.blocked_characters.contains(token);
        if (allowed) text_classes_.push_back(static_cast<int>(index));
        if (allowed && config_.katakana_only && katakana_token(token)) katakana_classes_.push_back(static_cast<int>(index));
        const auto first = static_cast<unsigned char>(token[0]);
        const size_t length = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
        if (token.size() == length && config_.number_characters.find(token) != std::string::npos) {
            number_classes_.push_back(static_cast<int>(index));
            if (token.size() == 1 && token[0] >= '0' && token[0] <= '9') found_digits += token;
        }
    }
    for (char digit = '0'; digit <= '9'; ++digit)
        if (found_digits.find(digit) == std::string::npos) throw std::invalid_argument("Numeric route requires all ten digit classes");
    if (config_.katakana_only && katakana_classes_.size() <= 1) throw std::invalid_argument("Requested katakana charset has no model classes");
}

void TextRecognizer::preprocess(const std::vector<cv::Mat> &crops) {
    input_tensor_.clear(); output_tensor_.clear(); blob_.release();
    input_width_ = numbers_ ? config_.number_min_width : config_.min_width;
    for (const auto &crop : crops) {
        if (crop.empty() || crop.type() != CV_8UC3 || crop.dims != 2)
            throw std::invalid_argument("OCR crop must be a nonempty uint8 BGR image");
        const double width = std::ceil(static_cast<double>(input_height_) * crop.cols / crop.rows);
        if (width > config_.max_width) throw std::invalid_argument("OCR crop exceeds recognition width budget");
        input_width_ = std::max(input_width_, static_cast<int>(width));
    }
    input_width_ = ((input_width_ + config_.width_multiple - 1) / config_.width_multiple) * config_.width_multiple;
    if (model_input_shape_[3] > 0) input_width_ = static_cast<int>(model_input_shape_[3]);
    if (input_width_ > config_.max_width) throw std::invalid_argument("OCR input exceeds width budget");
    if (uint64_t(crops.size()) * 3 * input_height_ * input_width_ * sizeof(float) > 256ull * 1024 * 1024)
        throw std::invalid_argument("OCR input tensor exceeds memory budget");
    std::vector<cv::Mat> normalized;
    normalized.reserve(crops.size());
    for (const auto &crop : crops) {
        int width, height;
        if (config_.left_align) {
            height = input_height_;
            width = std::min(input_width_, static_cast<int>(std::ceil(static_cast<double>(height) * crop.cols / crop.rows)));
        } else {
            const double ratio = std::min(static_cast<double>(input_width_) / crop.cols, static_cast<double>(input_height_) / crop.rows);
            width = std::max(1, static_cast<int>(crop.cols * ratio));
            height = std::max(1, static_cast<int>(crop.rows * ratio));
        }
        const int top = config_.left_align ? 0 : (input_height_ - height) / 2;
        const int left = config_.left_align ? 0 : (input_width_ - width) / 2;
        cv::Mat resized, padded;
        cv::resize(crop, resized, {width, height});
        if (config_.normalized_padding) resized = normalize_ocr(resized, config_.norm);
        cv::copyMakeBorder(resized, padded, top, input_height_ - height - top, left, input_width_ - width - left,
                           cv::BORDER_CONSTANT, cv::Scalar::all(config_.padding));
        if (!config_.normalized_padding) padded = normalize_ocr(padded, config_.norm);
        normalized.push_back(std::move(padded));
    }
    blob_ = cv::dnn::blobFromImages(normalized, 1, normalized.front().size(), cv::Scalar(), false, false);
    input_shape_ = {static_cast<int64_t>(crops.size()), 3, input_height_, input_width_};
    input_tensor_.emplace_back(Ort::Value::CreateTensor<float>(
        Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU), blob_.ptr<float>(), blob_.total(), input_shape_.data(), input_shape_.size()));
}

bool TextRecognizer::infer() {
    output_tensor_ = session_->session().Run(Ort::RunOptions{nullptr}, session_->input_names().data(), input_tensor_.data(),
        input_tensor_.size(), session_->output_names().data(), session_->output_names().size());
    return !output_tensor_.empty();
}

bool TextRecognizer::postprocess(std::vector<std::string> &texts, std::vector<cv::Rect> &boxes, std::vector<float> &scores) {
    texts.clear(); scores.clear();
    if (output_tensor_.empty()) return false;
    const auto info = output_tensor_.front().GetTensorTypeAndShapeInfo();
    const auto shape = info.GetShape();
    if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || shape.size() != 3 ||
        shape[0] != static_cast<int64_t>(boxes.size()) || shape[1] <= 0 || shape[2] != expected_classes_)
        throw std::runtime_error("OCR output shape/classes do not match dictionary and crop batch");
    const auto *output = output_tensor_.front().GetTensorData<float>();
    std::vector<cv::Rect> filtered;
    for (int64_t batch = 0; batch < shape[0]; ++batch) {
        std::string text;
        float sum = 0;
        int count = 0, previous = -1;
        for (int64_t step = 0; step < shape[1]; ++step) {
            const auto *row = output + (batch * shape[1] + step) * shape[2];
            int best = 0;
            if (numbers_) {
                for (int cls : number_classes_) if (row[cls] > row[best]) best = cls;
            } else if (config_.katakana_only) {
                for (int cls : katakana_classes_) if (row[cls] > row[best]) best = cls;
            } else if (!config_.blocked_characters.empty()) {
                for (int cls : text_classes_) if (row[cls] > row[best]) best = cls;
            } else best = static_cast<int>(std::max_element(row, row + shape[2]) - row);
            if (!std::isfinite(row[best])) throw std::runtime_error("OCR output contains nonfinite confidence");
            if (best != previous && best != 0 && (config_.sequence_mean || row[best] >= params_.text_threshold)) {
                text += best <= static_cast<int64_t>(dictionary_->size()) ? dictionary_->get_char(best - 1) : " ";
                sum += row[best]; ++count;
            }
            previous = best;
        }
        if (!count || sum / count < params_.text_threshold) continue;
        texts.push_back(std::move(text)); scores.push_back(sum / count); filtered.push_back(boxes[batch]);
    }
    boxes = std::move(filtered);
    return true;
}

bool TextRecognizer::recognize(std::vector<std::string> &texts, std::vector<cv::Rect> &boxes,
                               std::vector<float> &scores, const cv::Mat &image, const std::vector<cv::Mat> &crops, bool numbers) {
    texts.clear(); scores.clear();
    if (image.empty() || crops.empty() || boxes.size() != crops.size()) return false;
    for (const auto &crop : crops) if (crop.empty() || crop.type() != CV_8UC3) throw std::invalid_argument("Invalid OCR crop");
    numbers_ = numbers;
    const size_t batch_size = model_input_shape_[0] > 0 ? static_cast<size_t>(model_input_shape_[0]) : static_cast<size_t>(config_.max_batch);
    if (model_input_shape_[0] > 0 && crops.size() % batch_size) throw std::invalid_argument("Fixed ONNX batch does not match crop count");
    std::vector<cv::Rect> all_boxes;
    for (size_t start = 0; start < crops.size(); start += batch_size) {
        const size_t end = std::min(crops.size(), start + batch_size);
        std::vector<cv::Mat> chunk(crops.begin() + start, crops.begin() + end);
        if (!numbers_) for (auto &crop : chunk) crop = recognition_context(crop, config_);
        std::vector<cv::Rect> chunk_boxes(boxes.begin() + start, boxes.begin() + end);
        std::vector<std::string> chunk_texts;
        std::vector<float> chunk_scores;
        preprocess(chunk);
        if (!infer() || !postprocess(chunk_texts, chunk_boxes, chunk_scores)) return false;
        texts.insert(texts.end(), chunk_texts.begin(), chunk_texts.end());
        scores.insert(scores.end(), chunk_scores.begin(), chunk_scores.end());
        all_boxes.insert(all_boxes.end(), chunk_boxes.begin(), chunk_boxes.end());
    }
    boxes = std::move(all_boxes);
    return true;
}
} // namespace ar::paddle
