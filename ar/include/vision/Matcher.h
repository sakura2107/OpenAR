#pragma once
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include <opencv2/opencv.hpp>

#include "core/Registry.h"
#include "log/Logger.h"
#include "core/Image.h"
#include <cmath>

namespace ar::vision {

struct MatchResult {
    bool found = false;
    float score = 0.0f; // similarity of the best location; `found` means score >= threshold
    cv::Rect box;       // hit area, original-image coordinates
    cv::Point center;   // box centre, original-image coordinates
};

struct MatcherConfig {
    ar::log::ILogger *logger = nullptr;
    // Largest single frame a backend will buffer, in bytes (width*height*channels).
    int32_t max_input_bytes = 4096 * 2160 * 3;
};

// Template matching interface. Concrete algorithms (ssim-cuda today, others
// later) register themselves under a backend name.
class IImageMatcher {
  public:
    virtual ~IImageMatcher() = default;

    virtual std::string_view name() const = 0;

    // Optional hint that `templ` will be reused across frames (the typical
    // "grab frame -> match -> repeat" loop); backends may pre-process it once.
    // Backends must still handle a different template arriving in find().
    virtual bool prepare(const cv::Mat &) { return true; }

    // Searches `image` for `templ`. When `roi` is given the search is limited
    // to that area and box/center are mapped back to full-image coordinates.
    // found=false when nothing scores >= threshold.
    virtual MatchResult find(const cv::Mat &image, const cv::Mat &templ, float threshold = 0.80f,
                             const cv::Rect *roi = nullptr) = 0;

    virtual ar::core::Result<MatchResult> find_checked(const cv::Mat &image, const cv::Mat &templ,
                                                       float threshold = 0.80f,
                                                       const cv::Rect *roi = nullptr) {
        if (!ar::core::validate_image(image) || !ar::core::validate_image(templ) ||
            image.type() != templ.type() || !std::isfinite(threshold) || threshold < -1 || threshold > 1 ||
            (roi && !ar::core::valid_roi(*roi, image.size())))
            return ar::core::failure(ar::core::ErrorCode::invalid_argument, "match",
                                     "Invalid image, template, threshold or ROI", std::string(name()));
        try {
            return find(image, templ, threshold, roi);
        } catch (const std::exception &e) {
            return ar::core::failure(ar::core::ErrorCode::backend_failed, "match", e.what(),
                                     std::string(name()));
        }
    }
};

using MatcherRegistry = ar::core::Registry<IImageMatcher, const MatcherConfig &>;

std::unique_ptr<IImageMatcher> create_matcher(std::string_view backend, const MatcherConfig &config = {});
std::vector<std::string_view> matcher_backends();

} // namespace ar::vision
