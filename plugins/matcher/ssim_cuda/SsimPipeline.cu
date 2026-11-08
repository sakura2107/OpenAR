#include "core/ImageHash.h"
#include "SsimPipeline.h"

#include "core/CudaCheck.hpp"
#include "../../common/cuda/CudaImageOps.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {

// SSIM constants: C1 = (0.01 * 255)^2, C2 = (0.03 * 255)^2, C3 = C2 / 2.
// Initialised in place; constant symbols cannot be read by host code, so no
// cudaMemcpyToSymbol round-trip is needed.
__constant__ float kC1 = 6.5025f;
__constant__ float kC2 = 58.5225f;
__constant__ float kC3 = 29.26125f;

constexpr int kBlockSide = 16;
constexpr int kReduceBlock = 256;
constexpr unsigned int kNoIndex = 0xFFFFFFFFu;

// ---------------------------------------------------------------- integral --
// Two-pass integral images. sum(I) feeds the window mean; sum(I^2) gives the
// window variance in O(1) so the SSIM kernel's window loop only computes the
// covariance term (the dominant per-frame cost otherwise).
__global__ void IntegralRowsKernel(float *integral, const unsigned char *image, int32_t width, int32_t height,
                                   int32_t channels, bool squared) {
    const int32_t y = blockIdx.x * blockDim.x + threadIdx.x;
    const int32_t c = blockIdx.y * blockDim.y + threadIdx.y;
    if (y >= height || c >= channels)
        return;

    float sum = 0.0f;
    const size_t row = static_cast<size_t>(y) * width * channels + c;
    for (int32_t x = 0; x < width; ++x) {
        const float value = static_cast<float>(image[row + static_cast<size_t>(x) * channels]);
        sum += squared ? value * value : value;
        integral[row + static_cast<size_t>(x) * channels] = sum;
    }
}

__global__ void IntegralColumnsKernel(float *integral, int32_t width, int32_t height, int32_t channels) {
    const int32_t x = blockIdx.x * blockDim.x + threadIdx.x;
    const int32_t c = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || c >= channels)
        return;

    float sum = 0.0f;
    const size_t column = static_cast<size_t>(x) * channels + c;
    for (int32_t y = 0; y < height; ++y) {
        sum += integral[column + static_cast<size_t>(y) * width * channels];
        integral[column + static_cast<size_t>(y) * width * channels] = sum;
    }
}

// -------------------------------------------------------------------- SSIM --
// One thread per position: mean and variance come from the integral images in
// O(1); the window loop only accumulates the covariance term against the
// pre-centered template (sum(I * (T - mean_T)) = N * cov, because the centered
// template sums to zero). Per-channel scores are averaged here, so the result
// holds a single float per position.
__global__ void SsimKernel(const unsigned char *image, const float *integral, const float *integral_sq,
                           const float *templ_centered, const float *templ_mean, const float *templ_var,
                           float *res, int32_t image_width, int32_t image_height, int32_t templ_width,
                           int32_t templ_height, int32_t channels) {
    const int32_t tx = blockIdx.x * blockDim.x + threadIdx.x;
    const int32_t ty = blockIdx.y * blockDim.y + threadIdx.y;
    if (tx >= image_width - templ_width + 1 || ty >= image_height - templ_height + 1)
        return;

    const float area = static_cast<float>(templ_width * templ_height);
    const int32_t x2 = tx + templ_width - 1;
    const int32_t y2 = ty + templ_height - 1;
    const size_t image_stride = static_cast<size_t>(image_width) * channels;
    const size_t templ_stride = static_cast<size_t>(templ_width) * channels;

    float ssim_sum = 0.0f;
    for (int32_t c = 0; c < channels; ++c) {
        const float mean_temp = templ_mean[c];
        const float var_temp = templ_var[c];

        const size_t window = (static_cast<size_t>(y2) * image_width + x2) * channels + c;
        const size_t left = (static_cast<size_t>(y2) * image_width + (tx - 1)) * channels + c;
        const size_t up = (static_cast<size_t>(ty - 1) * image_width + x2) * channels + c;
        const size_t corner = (static_cast<size_t>(ty - 1) * image_width + (tx - 1)) * channels + c;

        float sum_i = integral[window];
        float sum_i2 = integral_sq[window];
        if (tx > 0) {
            sum_i -= integral[left];
            sum_i2 -= integral_sq[left];
        }
        if (ty > 0) {
            sum_i -= integral[up];
            sum_i2 -= integral_sq[up];
        }
        if (tx > 0 && ty > 0) {
            sum_i += integral[corner];
            sum_i2 += integral_sq[corner];
        }
        const float mean_image = sum_i / area;
        float var_image = sum_i2 / area - mean_image * mean_image;
        var_image = var_image > 0.0f ? var_image : 0.0f;

        // cov(I, T) = sum(I * (T - mean_T)) / area, since sum(T - mean_T) == 0.
        float sum_it = 0.0f;
        const unsigned char *image_row =
            image + static_cast<size_t>(ty) * image_stride + static_cast<size_t>(tx) * channels + c;
        const float *centered = templ_centered + c;
        for (int32_t h = 0; h < templ_height; ++h) {
            const unsigned char *image_pixel = image_row + static_cast<size_t>(h) * image_stride;
            const float *templ_pixel = centered + static_cast<size_t>(h) * templ_stride;
            for (int32_t w = 0; w < templ_width; ++w)
                sum_it += static_cast<float>(image_pixel[static_cast<size_t>(w) * channels]) *
                          templ_pixel[static_cast<size_t>(w) * channels];
        }
        const float covar = sum_it / area;

        const float luminance =
            (2 * mean_image * mean_temp + kC1) / (mean_image * mean_image + mean_temp * mean_temp + kC1);
        const float contrast = (2 * sqrtf(var_image) * sqrtf(var_temp) + kC2) / (var_image + var_temp + kC2);
        const float structure = (covar + kC3) / (sqrtf(var_image) * sqrtf(var_temp) + kC3);

        ssim_sum += luminance * contrast * structure;
    }
    res[static_cast<size_t>(ty) * (image_width - templ_width + 1) + tx] = ssim_sum / channels;
}

// ---------------------------------------------------------------- reduction --
// Block-level argmax over the per-position scores; each block atomically
// publishes (order-preserving score key << 32 | position index).
__global__ void BestPositionKernel(const float *res, int32_t total, unsigned long long *best) {
    __shared__ float s_score[kReduceBlock];
    __shared__ unsigned int s_index[kReduceBlock];

    const int tid = threadIdx.x;
    float best_score = -2.0f; // SSIM lies in [-1, 1]
    unsigned int best_idx = kNoIndex;
    for (int32_t i = tid + blockIdx.x * blockDim.x; i < total; i += gridDim.x * blockDim.x) {
        const float score = res[i];
        if (score > best_score) {
            best_score = score;
            best_idx = static_cast<unsigned int>(i);
        }
    }
    s_score[tid] = best_score;
    s_index[tid] = best_idx;
    __syncthreads();
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride && s_score[tid + stride] > s_score[tid]) {
            s_score[tid] = s_score[tid + stride];
            s_index[tid] = s_index[tid + stride];
        }
        __syncthreads();
    }
    if (tid == 0 && s_index[0] != kNoIndex) {
        unsigned int key = __float_as_uint(s_score[0]);
        key ^= (key >> 31) ? 0xFFFFFFFFu : 0x80000000u; // order-preserving for >= 0 floats
        const unsigned long long packed = (static_cast<unsigned long long>(key) << 32) | s_index[0];
        atomicMax(best, packed);
    }
}

// ------------------------------------------------------------------- utils --
// Byte-order-independent 64-bit hash over the template bytes (chunked so a
// full-screen template hashes in well under a millisecond).
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

namespace ar {
namespace ssim {

namespace {
constexpr int32_t kMaxResizePixels = SsimPipeline::kWorkingSize * SsimPipeline::kWorkingSize;
constexpr size_t kMaxResizeBytes = static_cast<size_t>(kMaxResizePixels) * 3;
constexpr size_t kMaxResizeFloats = static_cast<size_t>(kMaxResizePixels) * 3;
} // namespace

SsimPipeline::SsimPipeline(int32_t max_input_bytes) : max_input_bytes_(max_input_bytes) {
    if (!allocate(init_error_))
        return;
    init_ok_ = true;
}

SsimPipeline::~SsimPipeline() {
    const auto free_host = [](void *pointer) {
        if (pointer)
            cudaFreeHost(pointer);
    };
    const auto free_device = [](void *pointer) {
        if (!pointer)
            return;
        // cudaFree errors cannot be propagated from a destructor; print them so
        // the failure stays visible instead of leaving a stale CUDA error.
        const cudaError_t err = cudaFree(pointer);
        if (err != cudaSuccess)
            std::fprintf(stderr, "SsimPipeline cudaFree failed: %s\n", cudaGetErrorString(err));
    };
    free_host(host_temp_resized_);
    free_host(host_temp_centered_);
    free_host(host_best_);
    free_device(dev_image_);
    free_device(dev_image_resized_);
    free_device(dev_integral_);
    free_device(dev_integral_sq_);
    free_device(dev_result_);
    free_device(dev_best_);
    free_device(dev_temp_);
    free_device(dev_temp_resized_);
    free_device(dev_temp_centered_);
    free_device(dev_temp_mean_);
    free_device(dev_temp_var_);
    if (stream_)
        cudaStreamDestroy(stream_);
}

bool SsimPipeline::allocate(std::string &error) {
    const int32_t input_capacity = std::max(max_input_bytes_, static_cast<int32_t>(kMaxResizeBytes));

    const auto alloc_host = [&](auto **pointer, size_t bytes, const char *what) {
        return ar::core::check_cuda(cudaMallocHost(reinterpret_cast<void **>(pointer), bytes), &error) ||
               (error = std::string("SsimPipeline: alloc host ") + what + ": " + error, false);
    };
    const auto alloc_device = [&](auto **pointer, size_t bytes, const char *what) {
        return ar::core::check_cuda(cudaMalloc(reinterpret_cast<void **>(pointer), bytes), &error) ||
               (error = std::string("SsimPipeline: alloc device ") + what + ": " + error, false);
    };

    if (!alloc_host(&host_temp_resized_, kMaxResizeBytes, "temp_resized"))
        return false;
    if (!alloc_host(&host_temp_centered_, kMaxResizeFloats * sizeof(float), "temp_centered"))
        return false;
    if (!alloc_host(&host_best_, sizeof(unsigned long long), "best"))
        return false;

    if (!alloc_device(&dev_image_, input_capacity, "image"))
        return false;
    if (!alloc_device(&dev_image_resized_, kMaxResizeBytes, "image_resized"))
        return false;
    if (!alloc_device(&dev_integral_, kMaxResizeFloats * sizeof(float), "integral"))
        return false;
    if (!alloc_device(&dev_integral_sq_, kMaxResizeFloats * sizeof(float), "integral_sq"))
        return false;
    if (!alloc_device(&dev_result_, static_cast<size_t>(kMaxResizePixels) * sizeof(float), "result"))
        return false;
    if (!alloc_device(&dev_best_, sizeof(unsigned long long), "best"))
        return false;
    if (!alloc_device(&dev_temp_, input_capacity, "temp"))
        return false;
    if (!alloc_device(&dev_temp_resized_, kMaxResizeBytes, "temp_resized"))
        return false;
    if (!alloc_device(&dev_temp_centered_, kMaxResizeFloats * sizeof(float), "temp_centered"))
        return false;
    if (!alloc_device(&dev_temp_mean_, 3 * sizeof(float), "temp_mean"))
        return false;
    if (!alloc_device(&dev_temp_var_, 3 * sizeof(float), "temp_var"))
        return false;

    if (!ar::core::check_cuda(cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking), &error))
        return false;
    return true;
}

bool SsimPipeline::ok(std::string &error) const {
    if (init_ok_)
        return true;
    error = init_error_.empty() ? "SsimPipeline: device buffers were not initialized" : init_error_;
    return false;
}

bool SsimPipeline::upload(const unsigned char *host, size_t host_pitch, int32_t width, int32_t height,
                          int32_t channels, unsigned char *device, std::string &error) {
    const size_t row_bytes = static_cast<size_t>(width) * channels;
    return ar::core::check_cuda(cudaMemcpy2DAsync(device, row_bytes, host, host_pitch, row_bytes,
                                                  static_cast<size_t>(height), cudaMemcpyHostToDevice,
                                                  stream_),
                                &error);
}

bool SsimPipeline::ensure_template(const unsigned char *templ_data, size_t templ_pitch, int32_t templ_width,
                                   int32_t templ_height, int32_t channels, float ratio, std::string &error) {
    const uint64_t hash = ar::core::image_hash(templ_data, templ_pitch, templ_width, templ_height, channels);
    if (cache_valid_ && cache_hash_ == hash && cache_width_ == templ_width && cache_height_ == templ_height &&
        cache_channels_ == channels && cache_ratio_ == ratio)
        return true;

    const auto checked = [&](cudaError_t err, const char *what) {
        return ar::core::check_cuda(err, &error) || (error = std::string(what) + ": " + error, false);
    };
    const auto scaled = [ratio](int32_t value) {
        return std::max(1, static_cast<int32_t>(std::lround(value * ratio)));
    };
    const int32_t width_resized = scaled(templ_width);
    const int32_t height_resized = scaled(templ_height);
    if (static_cast<size_t>(width_resized) * height_resized * channels > kMaxResizeBytes) {
        error = "Resized SSIM template exceeds the configured buffer";
        return false;
    }

    // Raw upload -> resize -> download once; the statistics below are then
    // computed from the host copy and stay valid for every following frame.
    if (!upload(templ_data, templ_pitch, templ_width, templ_height, channels, dev_temp_, error))
        return false;
    if (!ar::vision::resize_cuda(dev_temp_resized_, static_cast<size_t>(width_resized) * channels, dev_temp_,
                                 static_cast<size_t>(templ_width) * channels, templ_width, templ_height,
                                 width_resized, height_resized, channels, ar::vision::Interp::linear, stream_,
                                 &error)) {
        error = "resize template: " + error;
        return false;
    }
    if (!checked(cudaMemcpyAsync(host_temp_resized_, dev_temp_resized_,
                                 static_cast<size_t>(width_resized) * height_resized * channels,
                                 cudaMemcpyDeviceToHost, stream_),
                 "download resized template"))
        return false;
    if (!checked(cudaStreamSynchronize(stream_), "sync after template rebuild"))
        return false;

    // Per-channel mean and variance of the resized template, plus a centered
    // float copy so the per-frame kernel needs no template mean subtraction.
    float mean[3] = {0.0f, 0.0f, 0.0f};
    float variance[3] = {0.0f, 0.0f, 0.0f};
    const int32_t resized_pixels = width_resized * height_resized;
    for (int32_t c = 0; c < channels; ++c) {
        double sum = 0.0;
        for (int32_t i = 0; i < resized_pixels; ++i)
            sum += host_temp_resized_[static_cast<size_t>(i) * channels + c];
        mean[c] = static_cast<float>(sum / resized_pixels);
    }
    for (int32_t c = 0; c < channels; ++c) {
        double sum = 0.0;
        for (int32_t i = 0; i < resized_pixels; ++i) {
            const float value = static_cast<float>(host_temp_resized_[static_cast<size_t>(i) * channels + c]);
            sum += (value - mean[c]) * (value - mean[c]);
        }
        variance[c] = static_cast<float>(sum / resized_pixels);
    }
    for (int32_t i = 0; i < resized_pixels; ++i)
        for (int32_t c = 0; c < channels; ++c)
            host_temp_centered_[static_cast<size_t>(i) * channels + c] =
                static_cast<float>(host_temp_resized_[static_cast<size_t>(i) * channels + c]) - mean[c];

    if (!checked(cudaMemcpyAsync(dev_temp_centered_, host_temp_centered_,
                                 static_cast<size_t>(resized_pixels) * channels * sizeof(float),
                                 cudaMemcpyHostToDevice, stream_),
                 "upload centered template"))
        return false;
    if (!checked(
            cudaMemcpyAsync(dev_temp_mean_, mean, channels * sizeof(float), cudaMemcpyHostToDevice, stream_),
            "upload template mean"))
        return false;
    if (!checked(cudaMemcpyAsync(dev_temp_var_, variance, channels * sizeof(float), cudaMemcpyHostToDevice,
                                 stream_),
                 "upload template variance"))
        return false;
    if (!checked(cudaStreamSynchronize(stream_), "sync after template upload"))
        return false;

    cache_hash_ = hash;
    cache_width_ = templ_width;
    cache_height_ = templ_height;
    cache_channels_ = channels;
    cache_ratio_ = ratio;
    cache_width_resized_ = width_resized;
    cache_height_resized_ = height_resized;
    cache_valid_ = true;
    return true;
}

bool SsimPipeline::find(const unsigned char *image_data, size_t image_pitch, int32_t image_width,
                        int32_t image_height, const unsigned char *templ_data, size_t templ_pitch,
                        int32_t templ_width, int32_t templ_height, int32_t channels, float threshold,
                        float *out_score, int32_t *out_x, int32_t *out_y, std::string &error) {
    *out_score = -1.0f;
    *out_x = -1;
    *out_y = -1;

    if (!init_ok_) {
        error = init_error_.empty() ? "SsimPipeline: device buffers were not initialized" : init_error_;
        return false;
    }
    if (!image_data || !templ_data || channels < 1 || channels > 3 || image_width <= 0 || image_height <= 0 ||
        templ_width <= 0 || templ_height <= 0 || templ_width > image_width || templ_height > image_height) {
        error = "Invalid SSIM input dimensions";
        return false;
    }

    const float ratio =
        static_cast<float>(kWorkingSize) / static_cast<float>(std::max(image_width, image_height));
    const auto scaled = [ratio](int32_t value) {
        return std::max(1, static_cast<int32_t>(std::lround(value * ratio)));
    };
    const int32_t image_width_resized = scaled(image_width);
    const int32_t image_height_resized = scaled(image_height);
    if (static_cast<size_t>(image_width_resized) * image_height_resized * channels > kMaxResizeBytes) {
        error = "Resized SSIM input exceeds the configured buffer";
        return false;
    }

    if (!ensure_template(templ_data, templ_pitch, templ_width, templ_height, channels, ratio, error))
        return false;
    const int32_t templ_width_resized = cache_width_resized_;
    const int32_t templ_height_resized = cache_height_resized_;
    if (templ_width_resized > image_width_resized || templ_height_resized > image_height_resized)
        return true; // template cannot fit the frame this time

    const auto checked = [&](cudaError_t err, const char *what) {
        return ar::core::check_cuda(err, &error) || (error = std::string(what) + ": " + error, false);
    };

    // Per-frame path: queue everything on one stream, sync once at the end.
    if (!upload(image_data, image_pitch, image_width, image_height, channels, dev_image_, error)) {
        error = "upload image: " + error;
        return false;
    }
    if (!ar::vision::resize_cuda(dev_image_resized_, static_cast<size_t>(image_width_resized) * channels,
                                 dev_image_, static_cast<size_t>(image_width) * channels, image_width,
                                 image_height, image_width_resized, image_height_resized, channels,
                                 ar::vision::Interp::linear, stream_, &error)) {
        error = "resize image: " + error;
        return false;
    }

    {
        // Integral images of the resized frame (rows, then columns), one for
        // sum(I) and one for sum(I^2).
        const dim3 row_block(32, 8);
        const dim3 row_grid((image_height_resized + row_block.x - 1) / row_block.x,
                            (channels + row_block.y - 1) / row_block.y);
        IntegralRowsKernel<<<row_grid, row_block, 0, stream_>>>(
            dev_integral_, dev_image_resized_, image_width_resized, image_height_resized, channels, false);
        if (!checked(cudaGetLastError(), "launch integral rows"))
            return false;
        IntegralRowsKernel<<<row_grid, row_block, 0, stream_>>>(
            dev_integral_sq_, dev_image_resized_, image_width_resized, image_height_resized, channels, true);
        if (!checked(cudaGetLastError(), "launch integral-sq rows"))
            return false;
        const dim3 column_block(32, 8);
        const dim3 column_grid((image_width_resized + column_block.x - 1) / column_block.x,
                               (channels + column_block.y - 1) / column_block.y);
        IntegralColumnsKernel<<<column_grid, column_block, 0, stream_>>>(dev_integral_, image_width_resized,
                                                                         image_height_resized, channels);
        if (!checked(cudaGetLastError(), "launch integral columns"))
            return false;
        IntegralColumnsKernel<<<column_grid, column_block, 0, stream_>>>(
            dev_integral_sq_, image_width_resized, image_height_resized, channels);
        if (!checked(cudaGetLastError(), "launch integral-sq columns"))
            return false;
    }

    const int32_t result_width = image_width_resized - templ_width_resized + 1;
    const int32_t result_height = image_height_resized - templ_height_resized + 1;
    {
        const dim3 block(kBlockSide, kBlockSide);
        const dim3 grid((result_width + kBlockSide - 1) / kBlockSide,
                        (result_height + kBlockSide - 1) / kBlockSide);
        SsimKernel<<<grid, block, 0, stream_>>>(dev_image_resized_, dev_integral_, dev_integral_sq_,
                                                dev_temp_centered_, dev_temp_mean_, dev_temp_var_,
                                                dev_result_, image_width_resized, image_height_resized,
                                                templ_width_resized, templ_height_resized, channels);
        if (!checked(cudaGetLastError(), "launch SSIM kernel"))
            return false;
    }

    const int32_t total_positions = result_width * result_height;
    if (!checked(cudaMemsetAsync(dev_best_, 0, sizeof(unsigned long long), stream_), "reset best"))
        return false;
    {
        const int block = kReduceBlock;
        const int grid = std::min((total_positions + block - 1) / block, 4096);
        BestPositionKernel<<<grid, block, 0, stream_>>>(dev_result_, total_positions, dev_best_);
        if (!checked(cudaGetLastError(), "launch best-position reduction"))
            return false;
    }
    if (!checked(cudaMemcpyAsync(host_best_, dev_best_, sizeof(unsigned long long), cudaMemcpyDeviceToHost,
                                 stream_),
                 "download best position"))
        return false;
    if (!checked(cudaStreamSynchronize(stream_), "sync after find"))
        return false;

    const unsigned long long packed = *host_best_;
    if (packed == 0)
        return true; // no position produced (degenerate input)

    unsigned int key = static_cast<unsigned int>(packed >> 32);
    const unsigned int index = static_cast<unsigned int>(packed & 0xFFFFFFFFu);
    const unsigned int bits = (key & 0x80000000u) ? (key ^ 0x80000000u) : ~key;
    float best_ssim = -1.0f;
    std::memcpy(&best_ssim, &bits, sizeof(bits));

    *out_score = best_ssim;
    if (best_ssim >= threshold) {
        const int32_t best_x = static_cast<int32_t>(index % static_cast<unsigned int>(result_width));
        const int32_t best_y = static_cast<int32_t>(index / static_cast<unsigned int>(result_width));
        *out_x = static_cast<int32_t>(std::lround(best_x / ratio));
        *out_y = static_cast<int32_t>(std::lround(best_y / ratio));
    }
    return true;
}

} // namespace ssim
} // namespace ar
