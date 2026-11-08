#pragma once
// Internal to the MatcherSsimCuda plugin DLL. Pure raw-pointer API: the DLL
// has no OpenCV/STL-interface dependency and only links the (shared) CUDA
// runtime. <format>-free so nvcc can consume SsimPipeline.h transitively.
#include <cstdint>
#include <string>

#include "SsimPipeline.h"

namespace ar {
namespace ssim {

struct SsimMatch {
    float score = -1.0f;
    int32_t x = -1; // best window top-left in full-image coordinates, -1 below threshold
    int32_t y = -1;
};

class SsimMatcher {
  public:
    explicit SsimMatcher(int32_t max_input_bytes)
        : max_input_bytes_(max_input_bytes), pipeline_(max_input_bytes) {}

    bool ok(std::string &error) const { return pipeline_.ok(error); }

    // Same semantics as SsimPipeline::find, plus ROI slicing (coordinates are
    // mapped back into the full image) and input validation that used to live
    // in the framework side of the old ssim_cuda backend.
    bool find(const uint8_t *image_data, int64_t image_step, int32_t image_w, int32_t image_h,
              const uint8_t *templ_data, int64_t templ_step, int32_t templ_w, int32_t templ_h,
              int32_t channels, float threshold, int32_t roi_x, int32_t roi_y, int32_t roi_w, int32_t roi_h,
              SsimMatch &out, std::string &error);

  private:
    int32_t max_input_bytes_ = 0;
    SsimPipeline pipeline_;
};

} // namespace ssim
} // namespace ar
