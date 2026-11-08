#pragma once
#include <cstdint>
#include <opencv2/core.hpp>
#include "core/Result.h"
#include "core/ImageHash.h"
namespace ar::core {
inline Result<void> validate_image(const cv::Mat &image, size_t max_bytes = 256 * 1024 * 1024) {
    if (image.empty() || image.dims != 2 || image.depth() != CV_8U ||
        (image.channels() != 1 && image.channels() != 3 && image.channels() != 4) ||
        image.step < static_cast<size_t>(image.cols) * image.elemSize() ||
        image.total() > max_bytes / image.elemSize())
        return failure(ErrorCode::invalid_argument, "image", "Invalid dimensions, format or byte limit");
    return {};
}
inline bool valid_roi(const cv::Rect &roi, const cv::Size &size) noexcept {
    return roi.x >= 0 && roi.y >= 0 && roi.width > 0 && roi.height > 0 &&
           int64_t(roi.x) + roi.width <= size.width && int64_t(roi.y) + roi.height <= size.height;
}
inline uint64_t image_hash(const cv::Mat &image) {
    auto valid = validate_image(image);
    if (!valid)
        throw std::invalid_argument(valid.error().message);
    return image_hash(image.data, image.step, image.cols, image.rows, image.channels());
}
} // namespace ar::core
