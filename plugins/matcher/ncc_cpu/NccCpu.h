#pragma once
#include <cstdint>

#include <opencv2/opencv.hpp>

#include "vision/Matcher.h"

namespace ar {

// NCC (zero-mean normalized cross-correlation) template matching on the CPU
// (the "ncc-cpu" plugin). Mirrors the ssim-cpu pipeline: frame and template
// are converted to grayscale and downscaled to a 640px working size, then NCC
// is evaluated for every template position via cv::matchTemplate
// (TM_CCOEFF_NORMED, DFT-backed internally). NCC normalizes each window's
// mean and variance, so global brightness/contrast differences between
// devices do not move the score; the score itself lies in [-1, 1] (unlike
// SSIM's [0, 1], so thresholds are not interchangeable; ~0.9 works well for
// UI buttons).
//
// The gray working-size template is cached (content-hash + working-ratio
// keyed), so the typical script loop of "grab frame -> match -> repeat" only
// pays the frame-side gray+resize and the matchTemplate call.
//
// Plain class (no framework interface): the plugin DLL's C ABI exports in
// PluginExports.cpp wrap it behind ar::vision::IImageMatcher.
class NccCpuMatcher {
  public:
    explicit NccCpuMatcher(int32_t max_input_bytes) : max_input_bytes_(max_input_bytes) {}

    ar::vision::MatchResult find(const cv::Mat &image, const cv::Mat &templ, float threshold,
                                 const cv::Rect *roi, ar::log::ILogger *logger);

  private:
    int32_t max_input_bytes_ = 0;

    // Warm-template cache, rebuilt when the template content or the working
    // ratio changed (a different ROI size changes the ratio).
    bool cache_valid_ = false;
    uint64_t cache_hash_ = 0;
    float cache_ratio_ = 0.0f;
    float cache_stddev_ = 0.0f;
    cv::Mat cache_templ_; // grayscale template at working size
};

} // namespace ar
