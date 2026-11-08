#include "NccMatcher.h"

#include <algorithm>

namespace ar {
namespace ncc {

bool NccMatcher::find(const uint8_t *image_data, int64_t image_step, int32_t image_w, int32_t image_h,
                      const uint8_t *templ_data, int64_t templ_step, int32_t templ_w, int32_t templ_h,
                      int32_t channels, float threshold, int32_t roi_x, int32_t roi_y, int32_t roi_w,
                      int32_t roi_h, NccMatch &out, std::string &error) {
    out = NccMatch{};

    if (!image_data || !templ_data || (channels != 1 && channels != 3) || image_w <= 0 || image_h <= 0 ||
        templ_w <= 0 || templ_h <= 0) {
        error = "Invalid image or template";
        return false;
    }
    if (static_cast<int64_t>(image_w) * image_h * channels > max_input_bytes_) {
        error = "Image exceeds the configured maximum size";
        return false;
    }
    if (templ_w > image_w || templ_h > image_h) {
        error = "Template is larger than the image";
        return false;
    }

    // ROI: slice the pointer, keep the parent row stride, remember the offset.
    if (roi_w > 0 && roi_h > 0) {
        if (roi_x < 0 || roi_y < 0 || roi_w <= 0 || roi_h <= 0 || roi_x + roi_w > image_w ||
            roi_y + roi_h > image_h) {
            error = "ROI is outside image bounds";
            return false;
        }
        if (templ_w > roi_w || templ_h > roi_h)
            return true; // cannot fit: not found
        image_data += static_cast<int64_t>(roi_y) * image_step + static_cast<int64_t>(roi_x) * channels;
        image_w = roi_w;
        image_h = roi_h;
    } else {
        roi_x = 0;
        roi_y = 0;
    }

    float score = -1.0f;
    int32_t x = -1, y = -1;
    if (!pipeline_.find(image_data, static_cast<size_t>(image_step), image_w, image_h, templ_data,
                        static_cast<size_t>(templ_step), templ_w, templ_h, channels, threshold, &score, &x,
                        &y, error))
        return false;

    out.score = score;
    if (x >= 0 && y >= 0) {
        out.x = x + roi_x;
        out.y = y + roi_y;
    }
    return true;
}

} // namespace ncc
} // namespace ar
