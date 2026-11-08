#pragma once
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <opencv2/opencv.hpp>

#include "core/Registry.h"
#include "log/Logger.h"
#include "core/Image.h"

namespace ar::vision {

struct OcrParams {
    std::string det_model;  // text detection model (onnx)
    std::string rec_model;  // text recognition model (onnx)
    std::string dictionary; // character dictionary file
    float text_threshold = 0.5f;
    float box_threshold = 0.5f;
    float unclip_ratio = 1.5f;
    bool score_mode = false;
    // Detection post-processing: neighbouring boxes on the same visual line
    // (vertical overlap) merge when their horizontal gap is below
    // det_line_merge_ratio * line height. DBNet frequently splits one line
    // into two components on stylised fonts over busy backgrounds, which
    // breaks substring/exact text lookups. 0 disables the merge.
    float det_line_merge_ratio = 0.5f;
    // CUDA device for gpu backends; negative resolves from AR_OCR_CUDA_DEVICE (default 0).
    int device_id = -1;
    // Optional data-driven profile; paths inside it are relative to its file.
    // Requires the plugin's additive profile factory export.
    std::string profile;
};

struct OcrItem {
    std::string text;
    float score = 0.0f;
    cv::Rect box; // text region, original-image coordinates
};

// OCR engine interface. Backends ("paddle-ocr-cpu" / "paddle-ocr-gpu" today)
// share the paddle_common core and register themselves under their name.
class IOcrEngine {
  public:
    virtual ~IOcrEngine() = default;

    virtual std::string_view name() const = 0;

    // Full recognition pass over `image` (optionally limited to `roi`).
    // Returned boxes are in original-image coordinates.
    virtual std::vector<OcrItem> recognize(const cv::Mat &image, const cv::Rect *roi = nullptr) = 0;
    virtual ar::core::Result<std::vector<OcrItem>> recognize_checked(const cv::Mat &image,
                                                                     const cv::Rect *roi = nullptr) {
        if (!ar::core::validate_image(image) || (roi && !ar::core::valid_roi(*roi, image.size())))
            return ar::core::failure(ar::core::ErrorCode::invalid_argument, "ocr", "Invalid image or ROI",
                                     std::string(name()));
        try {
            return recognize(image, roi);
        } catch (const std::exception &e) {
            return ar::core::failure(ar::core::ErrorCode::backend_failed, "ocr", e.what(),
                                     std::string(name()));
        }
    }

    // The input is already a text/numeric ROI; bypass text detection.
    virtual ar::core::Result<std::vector<OcrItem>> recognize_crop_checked(const cv::Mat &,
                                                                         const cv::Rect * = nullptr, bool = false) {
        return ar::core::failure(ar::core::ErrorCode::unsupported, "ocr.crop", "Backend has no crop recognition route", std::string(name()));
    }

    // Convenience wrappers (non-virtual), successors of the old CompareText* family.
    std::optional<OcrItem> find_text(const cv::Mat &image, std::string_view text,
                                     const cv::Rect *roi = nullptr);
    bool contains_text(const cv::Mat &image, std::string_view text, const cv::Rect *roi = nullptr);
};

using OcrRegistry = ar::core::Registry<IOcrEngine, const OcrParams &, ar::log::ILogger *>;

std::unique_ptr<IOcrEngine> create_ocr(std::string_view backend, const OcrParams &params,
                                       ar::log::ILogger *logger = nullptr);
std::vector<std::string_view> ocr_backends();

} // namespace ar::vision
