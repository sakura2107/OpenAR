#pragma once
#include <cstdint>
#include <string>

#include <opencv2/opencv.hpp>

#include "vision/Matcher.h"

namespace ar {

// SSIM template matching on the CPU (the "ssim-cpu" plugin). Mirrors the
// ssim-cuda algorithm: the frame and template are downscaled to a 640px
// working size, then windowed SSIM is evaluated for every template position.
// The per-position statistics come from OpenCV box filters and a template
// correlation (filter2D), so the whole map is computed vectorized instead of
// with per-position loops.
//
// Plain class (no framework interface): the plugin DLL's C ABI exports in
// PluginExports.cpp wrap it behind ar::vision::IImageMatcher.
class SsimCpuMatcher {
  public:
    explicit SsimCpuMatcher(int32_t max_input_bytes) : max_input_bytes_(max_input_bytes) {}

    ar::vision::MatchResult find(const cv::Mat &image, const cv::Mat &templ, float threshold,
                                 const cv::Rect *roi, ar::log::ILogger *logger);

  private:
    int32_t max_input_bytes_ = 0;
};

} // namespace ar
