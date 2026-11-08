#pragma once
// Internal header for the ssim-cuda backend; nothing outside the plugin
// includes this. Deliberately <format>-free so nvcc can consume it.
#include <cuda_runtime.h>

#include <cstdint>
#include <string>

namespace ar {
namespace ssim {

// Optimized SSIM template matching pipeline. Per frame it only uploads the
// frame, then runs resize -> integral image -> SSIM -> argmax reduction on the
// device and downloads ~8 bytes; everything derived from the TEMPLATE (resize,
// mean, variance, centered floats) is cached on device and rebuilt only when
// the template or the working ratio changes (the typical script loop reuses
// one template across frames at up to ~60 fps).
//
// All work is queued on one stream and synchronized once per call.
class SsimPipeline {
  public:
    static constexpr int32_t kWorkingSize = 640;

    explicit SsimPipeline(int32_t max_input_bytes);
    ~SsimPipeline();

    SsimPipeline(const SsimPipeline &) = delete;
    SsimPipeline &operator=(const SsimPipeline &) = delete;

    bool ok(std::string &error) const;

    // image is interleaved bytes; image_pitch is the row stride in bytes
    // (cv::Mat::step), so strided ROIs are consumed without repacking.
    // templ_* describe the raw template; its processed form comes from the
    // cache (rebuilt here when the template or the ratio changed).
    // On success *out_score is the best SSIM value (channels averaged) and
    // *out_x/*out_y the best location in source coordinates, or -1 when the
    // score is below `threshold`.
    bool find(const unsigned char *image_data, size_t image_pitch, int32_t image_width, int32_t image_height,
              const unsigned char *templ_data, size_t templ_pitch, int32_t templ_width, int32_t templ_height,
              int32_t channels, float threshold, float *out_score, int32_t *out_x, int32_t *out_y,
              std::string &error);

  private:
    bool allocate(std::string &error);
    bool upload(const unsigned char *host, size_t host_pitch, int32_t width, int32_t height, int32_t channels,
                unsigned char *device, std::string &error);

    // Template cache (space-for-time). Rebuilt when the template content
    // (64-bit hash), geometry or working ratio changed.
    bool ensure_template(const unsigned char *templ_data, size_t templ_pitch, int32_t templ_width,
                         int32_t templ_height, int32_t channels, float ratio, std::string &error);

    int32_t max_input_bytes_ = 0;

    // Host side (page-locked: async copies and reused across calls).
    unsigned char *host_temp_resized_ = nullptr; // rebuild path only
    float *host_temp_centered_ = nullptr;        // rebuild path only
    unsigned long long *host_best_ = nullptr;    // packed best(score, index)

    // Device side.
    unsigned char *dev_image_ = nullptr;
    unsigned char *dev_image_resized_ = nullptr;
    float *dev_integral_ = nullptr;    // sum(I) integral, wr*hr*ch floats
    float *dev_integral_sq_ = nullptr; // sum(I^2) integral, wr*hr*ch floats
    float *dev_result_ = nullptr;      // one score per position
    unsigned long long *dev_best_ = nullptr;
    unsigned char *dev_temp_ = nullptr;         // rebuild path only
    unsigned char *dev_temp_resized_ = nullptr; // rebuild path only
    float *dev_temp_centered_ = nullptr;        // (T - mean) floats, cached
    float *dev_temp_mean_ = nullptr;            // per-channel means, cached
    float *dev_temp_var_ = nullptr;             // per-channel variances, cached

    cudaStream_t stream_ = nullptr;
    bool init_ok_ = false;
    std::string init_error_;

    // Cache key/state.
    uint64_t cache_hash_ = 0;
    int32_t cache_width_ = 0;
    int32_t cache_height_ = 0;
    int32_t cache_channels_ = 0;
    float cache_ratio_ = 0.0f;
    int32_t cache_width_resized_ = 0;
    int32_t cache_height_resized_ = 0;
    bool cache_valid_ = false;
};

} // namespace ssim
} // namespace ar
