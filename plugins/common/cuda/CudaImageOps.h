#pragma once
#include <cuda_runtime.h>

#include <cstdint>
#include <string>

namespace ar {
namespace vision {

enum class Interp : int32_t {
    nearest = 0,
    linear = 1,
};

// Device-to-device image ops shared by the CUDA backends (ssim-cuda today,
// yolo-gpu later). All pitches are row strides in BYTES, so strided sources
// (cv::Mat ROIs) can be consumed without repacking on the host. Interleaved
// channel layout: pixel(x, y, c) = base[y * pitch + x * channels + c].
// The ops only QUEUE work on `stream`; callers synchronize when they need the
// result, which lets a whole pipeline run with a single sync at the end.

bool resize_cuda(unsigned char *dst, size_t dst_pitch, const unsigned char *src, size_t src_pitch,
                 int32_t in_width, int32_t in_height, int32_t out_width, int32_t out_height, int32_t channels,
                 Interp interpolation, cudaStream_t stream, std::string *error = nullptr);

bool gaussian_blur_cuda(unsigned char *dst, size_t dst_pitch, const unsigned char *src, size_t src_pitch,
                        int32_t width, int32_t height, int32_t channels, cudaStream_t stream,
                        std::string *error = nullptr);

} // namespace vision
} // namespace ar
