#include "core/ImageHash.h"
#include "NccCpu.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace ar {

namespace {

constexpr int32_t kWorkingSize = 640;
// A template whose standard deviation falls below this (8-bit gray levels)
// carries no structure to correlate; TM_CCOEFF_NORMED would divide 0 by 0 on
// it. Flat IMAGE windows are left to OpenCV: they yield NaN cells, which
// minMaxLoc skips.
constexpr float kMinStdDev = 0.5f;

bool validate_inputs(const cv::Mat &image, const cv::Mat &templ, int32_t max_input_bytes,
                     ar::log::ILogger *logger) {
    if (image.empty() || templ.empty()) {
        AR_LOG(logger, ar::log::Level::error, "Image empty !");
        return false;
    }
    if (templ.rows > image.rows || templ.cols > image.cols) {
        AR_LOG(logger, ar::log::Level::error, "Template is larger than the image !");
        return false;
    }
    if (image.total() * image.elemSize() > static_cast<size_t>(max_input_bytes) ||
        templ.total() * templ.elemSize() > static_cast<size_t>(max_input_bytes)) {
        AR_LOG(logger, ar::log::Level::error, "Image too large is not allow !");
        return false;
    }
    if (image.type() != templ.type() || (image.type() != CV_8UC3 && image.type() != CV_8UC1)) {
        AR_LOG(logger, ar::log::Level::error, "Both image type must be bgr or gray !");
        return false;
    }
    return true;
}

cv::Mat to_gray(const cv::Mat &src) {
    if (src.channels() == 1)
        return src;
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

// FNV-1a over the template bytes (chunked), the same cache key the ssim-cuda
// pipeline uses.
uint64_t hash_bytes(const unsigned char *data, size_t bytes) {
    uint64_t hash = 14695981039346656037ull;
    size_t i = 0;
    for (; i + 8 <= bytes; i += 8) {
        uint64_t chunk;
        std::memcpy(&chunk, data + i, 8);
        hash = (hash ^ chunk) * 1099511628211ull;
    }
    for (; i < bytes; ++i)
        hash = (hash ^ data[i]) * 1099511628211ull;
    return hash;
}

} // namespace

ar::vision::MatchResult NccCpuMatcher::find(const cv::Mat &image, const cv::Mat &templ, float threshold,
                                            const cv::Rect *roi, ar::log::ILogger *logger) {
    ar::vision::MatchResult result;
    if (!validate_inputs(image, templ, max_input_bytes_, logger))
        return result;

    cv::Rect search_rect(0, 0, image.cols, image.rows);
    if (roi) {
        if (roi->x < 0 || roi->y < 0 || roi->width <= 0 || roi->height <= 0 ||
            roi->x + roi->width > image.cols || roi->y + roi->height > image.rows) {
            AR_LOG(logger, ar::log::Level::error, "Image match range is outside image bounds");
            return result;
        }
        if (templ.cols > roi->width || templ.rows > roi->height)
            return result;
        search_rect = *roi;
    }

    // Grayscale first (UI matching is a luminance problem), then one 1-channel
    // downscale instead of three.
    const float ratio = static_cast<float>(kWorkingSize) /
                        static_cast<float>(std::max(search_rect.width, search_rect.height));
    const auto scaled = [ratio](int32_t value) {
        return std::max(1, static_cast<int32_t>(std::lround(value * ratio)));
    };

    // Warm-template cache keyed by content hash + working ratio: the typical
    // script loop reuses one template across frames, so gray+resize (and the
    // flat-template check) only run when the template or the ratio changes.
    const uint64_t hash =
        ar::core::image_hash(templ.data, templ.step, templ.cols, templ.rows, templ.channels());
    if (!cache_valid_ || cache_hash_ != hash || cache_ratio_ != ratio) {
        const cv::Mat templ_gray = to_gray(templ);
        cv::resize(templ_gray, cache_templ_, cv::Size(scaled(templ_gray.cols), scaled(templ_gray.rows)), 0, 0,
                   cv::INTER_LINEAR);
        cv::Scalar templ_mean, templ_stddev;
        cv::meanStdDev(cache_templ_, templ_mean, templ_stddev);
        cache_stddev_ = static_cast<float>(templ_stddev[0]);
        cache_hash_ = hash;
        cache_ratio_ = ratio;
        cache_valid_ = true;
    }
    if (cache_stddev_ < kMinStdDev)
        return result; // flat template: nothing to correlate
    const cv::Mat &templ_resized = cache_templ_;

    const cv::Mat search_gray = to_gray(image(search_rect));
    cv::Mat search_resized;
    cv::resize(search_gray, search_resized, cv::Size(scaled(search_gray.cols), scaled(search_gray.rows)), 0,
               0, cv::INTER_LINEAR);
    if (templ_resized.cols > search_resized.cols || templ_resized.rows > search_resized.rows)
        return result;

    // Zero-mean NCC map in [-1, 1] (TM_CCOEFF_NORMED is the Pearson
    // correlation of every window with the template).
    cv::Mat ncc;
    cv::matchTemplate(search_resized, templ_resized, ncc, cv::TM_CCOEFF_NORMED);

    double best_score = -1.0;
    cv::Point best_pos(-1, -1);
    cv::minMaxLoc(ncc, nullptr, &best_score, nullptr, &best_pos);

    result.score = static_cast<float>(best_score);
    if (result.score >= threshold) {
        const cv::Point top_left(static_cast<int32_t>(std::lround(best_pos.x / ratio)) + search_rect.x,
                                 static_cast<int32_t>(std::lround(best_pos.y / ratio)) + search_rect.y);
        result.found = true;
        result.box = cv::Rect(top_left, templ.size());
        result.center = cv::Point(top_left.x + templ.cols / 2, top_left.y + templ.rows / 2);
    }
    return result;
}

} // namespace ar
