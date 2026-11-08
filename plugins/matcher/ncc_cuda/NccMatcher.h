#pragma once
// Internal to the MatcherNccCuda plugin DLL. Pure raw-pointer API: the DLL
// boundary stays plain C (ar/plugin/PluginApi.h), no C++ types cross it.
#include <cstdint>
#include <string>

#include "NccPipeline.h"

namespace ar {
namespace ncc {

struct NccMatch {
    float score = -1.0f;
    int32_t x = -1; // best window's top-left, full-image coordinates
    int32_t y = -1;
};

class NccMatcher {
  public:
    explicit NccMatcher(int32_t max_input_bytes)
        : max_input_bytes_(max_input_bytes), pipeline_(max_input_bytes) {}

    bool ok(std::string &error) const { return pipeline_.ok(error); }

    // Same semantics as NccPipeline::find, plus ROI slicing (coordinates are
    // mapped back into full-image space).
    bool find(const uint8_t *image_data, int64_t image_step, int32_t image_w, int32_t image_h,
              const uint8_t *templ_data, int64_t templ_step, int32_t templ_w, int32_t templ_h,
              int32_t channels, float threshold, int32_t roi_x, int32_t roi_y, int32_t roi_w, int32_t roi_h,
              NccMatch &out, std::string &error);

  private:
    int32_t max_input_bytes_ = 0;
    NccPipeline pipeline_;
};

} // namespace ncc
} // namespace ar
