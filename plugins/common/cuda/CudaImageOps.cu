#include "CudaImageOps.h"

#include "core/CudaCheck.hpp"

#include <cstdint>

namespace {

constexpr int kBlockSide = 16;
constexpr int kMaxChannels = 3;

__global__ void NearestNeighborKernel(unsigned char *out, size_t out_pitch, const unsigned char *in,
                                      size_t in_pitch, int32_t in_w, int32_t in_h, int32_t out_w,
                                      int32_t out_h, int32_t channels) {
    const int32_t x = blockIdx.x * blockDim.x + threadIdx.x;
    const int32_t y = blockIdx.y * blockDim.y + threadIdx.y;
    const int32_t c = blockIdx.z * blockDim.z + threadIdx.z;
    if (x >= out_w || y >= out_h || c >= channels)
        return;

    const float scale_x = static_cast<float>(in_w) / out_w;
    const float scale_y = static_cast<float>(in_h) / out_h;
    int32_t src_x = static_cast<int32_t>(roundf(x * scale_x));
    int32_t src_y = static_cast<int32_t>(roundf(y * scale_y));
    src_x = min(src_x, in_w - 1);
    src_y = min(src_y, in_h - 1);

    out[y * out_pitch + x * channels + c] = in[src_y * in_pitch + src_x * channels + c];
}

__global__ void InterLinearKernel(unsigned char *out, size_t out_pitch, const unsigned char *in,
                                  size_t in_pitch, int32_t in_w, int32_t in_h, int32_t out_w, int32_t out_h,
                                  int32_t channels) {
    const int32_t x = blockIdx.x * blockDim.x + threadIdx.x;
    const int32_t y = blockIdx.y * blockDim.y + threadIdx.y;
    const int32_t c = blockIdx.z * blockDim.z + threadIdx.z;
    if (x >= out_w || y >= out_h || c >= channels)
        return;

    const float scale_x = static_cast<float>(in_w) / out_w;
    const float scale_y = static_cast<float>(in_h) / out_h;

    // Half-pixel centres, clamped to the source area.
    const float src_x = (x + 0.5f) * scale_x - 0.5f;
    const float src_y = (y + 0.5f) * scale_y - 0.5f;
    int32_t x0 = min(max(static_cast<int32_t>(floorf(src_x)), 0), in_w - 1);
    int32_t y0 = min(max(static_cast<int32_t>(floorf(src_y)), 0), in_h - 1);
    const int32_t x1 = min(x0 + 1, in_w - 1);
    const int32_t y1 = min(y0 + 1, in_h - 1);
    const float dx = src_x - x0;
    const float dy = src_y - y0;

    const float p00 = in[y0 * in_pitch + x0 * channels + c];
    const float p01 = in[y0 * in_pitch + x1 * channels + c];
    const float p10 = in[y1 * in_pitch + x0 * channels + c];
    const float p11 = in[y1 * in_pitch + x1 * channels + c];

    const float top = p00 * (1.0f - dx) + p01 * dx;
    const float bottom = p10 * (1.0f - dx) + p11 * dx;
    out[y * out_pitch + x * channels + c] =
        static_cast<unsigned char>(roundf(top * (1.0f - dy) + bottom * dy));
}

__constant__ float kGaussianKernel[5][5] = {
    {1 / 256.0f, 4 / 256.0f, 6 / 256.0f, 4 / 256.0f, 1 / 256.0f},
    {4 / 256.0f, 16 / 256.0f, 24 / 256.0f, 16 / 256.0f, 4 / 256.0f},
    {6 / 256.0f, 24 / 256.0f, 36 / 256.0f, 24 / 256.0f, 6 / 256.0f},
    {4 / 256.0f, 16 / 256.0f, 24 / 256.0f, 16 / 256.0f, 4 / 256.0f},
    {1 / 256.0f, 4 / 256.0f, 6 / 256.0f, 4 / 256.0f, 1 / 256.0f},
};

__global__ void GaussianBlurKernel(unsigned char *out, size_t out_pitch, const unsigned char *in,
                                   size_t in_pitch, int32_t width, int32_t height, int32_t channels) {
    const int32_t x = blockIdx.x * blockDim.x + threadIdx.x;
    const int32_t y = blockIdx.y * blockDim.y + threadIdx.y;
    const int32_t c = blockIdx.z * blockDim.z + threadIdx.z;
    if (x >= width || y >= height || c >= channels)
        return;

    float pixel = 0.0f;
    for (int32_t ky = -2; ky <= 2; ++ky) {
        for (int32_t kx = -2; kx <= 2; ++kx) {
            int32_t sx = x + kx;
            int32_t sy = y + ky;
            if (sx < 0)
                sx = -sx;
            if (sx >= width)
                sx = 2 * width - sx - 2;
            if (sy < 0)
                sy = -sy;
            if (sy >= height)
                sy = 2 * height - sy - 2;
            pixel += kGaussianKernel[ky + 2][kx + 2] * in[sy * in_pitch + sx * channels + c];
        }
    }
    out[y * out_pitch + x * channels + c] = static_cast<unsigned char>(pixel);
}

dim3 make_grid(int32_t width, int32_t height, int32_t channels) {
    return dim3((width + kBlockSide - 1) / kBlockSide, (height + kBlockSide - 1) / kBlockSide,
                (channels + kMaxChannels - 1) / kMaxChannels);
}

dim3 make_block() {
    return dim3(kBlockSide, kBlockSide, kMaxChannels);
}

} // namespace

namespace ar {
namespace vision {

bool resize_cuda(unsigned char *dst, size_t dst_pitch, const unsigned char *src, size_t src_pitch,
                 int32_t in_width, int32_t in_height, int32_t out_width, int32_t out_height, int32_t channels,
                 Interp interpolation, cudaStream_t stream, std::string *error) {
    if (!dst || !src || channels < 1 || channels > kMaxChannels || in_width <= 0 || in_height <= 0 ||
        out_width <= 0 || out_height <= 0) {
        if (error)
            *error = "resize_cuda: invalid arguments";
        return false;
    }

    const dim3 grid = make_grid(out_width, out_height, channels);
    const dim3 block = make_block();
    cudaError_t err = cudaSuccess;
    switch (interpolation) {
    case Interp::nearest:
        NearestNeighborKernel<<<grid, block, 0, stream>>>(dst, dst_pitch, src, src_pitch, in_width, in_height,
                                                          out_width, out_height, channels);
        err = cudaGetLastError();
        break;
    case Interp::linear:
        InterLinearKernel<<<grid, block, 0, stream>>>(dst, dst_pitch, src, src_pitch, in_width, in_height,
                                                      out_width, out_height, channels);
        err = cudaGetLastError();
        break;
    default:
        if (error)
            *error = "resize_cuda: unsupported interpolation";
        return false;
    }
    if (!ar::core::check_cuda(err, error))
        return false;
    return true; // work is queued on `stream`; the caller synchronizes
}

bool gaussian_blur_cuda(unsigned char *dst, size_t dst_pitch, const unsigned char *src, size_t src_pitch,
                        int32_t width, int32_t height, int32_t channels, cudaStream_t stream,
                        std::string *error) {
    if (!dst || !src || channels < 1 || channels > kMaxChannels || width <= 0 || height <= 0) {
        if (error)
            *error = "gaussian_blur_cuda: invalid arguments";
        return false;
    }

    GaussianBlurKernel<<<make_grid(width, height, channels), make_block(), 0, stream>>>(
        dst, dst_pitch, src, src_pitch, width, height, channels);
    if (!ar::core::check_cuda(cudaGetLastError(), error))
        return false;
    return true; // work is queued on `stream`; the caller synchronizes
}

} // namespace vision
} // namespace ar
