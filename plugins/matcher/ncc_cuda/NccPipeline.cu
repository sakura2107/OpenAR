#include "core/ImageHash.h"
#include "NccPipeline.h"

#include "core/CudaCheck.hpp"
#include "../../common/cuda/CudaImageOps.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {

// Windows whose standard deviation falls below this (8-bit gray levels) are
// structureless: matching them is meaningless, so their score is forced to 0.
// Mirrors the ncc-cpu backend; the matcher also bails out early on a flat
// template, so the denominator here is only ~0 for flat IMAGE windows.
constexpr float kMinStdDev = 0.5f;

constexpr int kBlockSide = 16;
constexpr int kReduceBlock = 256;
constexpr unsigned int kNoIndex = 0xFFFFFFFFu;

// ---------------------------------------------------------------- grayscale --
// Rec.601 luma with the same fixed-point rounding as OpenCV's BGR2GRAY
// ((B*1868 + G*9617 + R*4899 + 8192) >> 14), so both backends correlate the
// same luminance signal.
__global__ void GrayBgrKernel(const unsigned char *src, size_t src_pitch, unsigned char *dst,
                              size_t dst_pitch, int32_t width, int32_t height) {
    const int32_t x = blockIdx.x * blockDim.x + threadIdx.x;
    const int32_t y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height)
        return;
    const unsigned char *pixel = src + static_cast<size_t>(y) * src_pitch + static_cast<size_t>(x) * 3;
    dst[static_cast<size_t>(y) * dst_pitch + x] =
        static_cast<unsigned char>((pixel[0] * 1868 + pixel[1] * 9617 + pixel[2] * 4899 + 8192) >> 14);
}

// ---------------------------------------------------------------- integral --
// Two-pass integral images (single channel). sum(I) feeds the window mean;
// sum(I^2) gives the window variance in O(1) so the NCC kernel's window loop
// only computes the covariance term.
__global__ void IntegralRowsKernel(float *integral, const unsigned char *image, int32_t width, int32_t height,
                                   bool squared) {
    const int32_t y = blockIdx.x * blockDim.x + threadIdx.x;
    if (y >= height)
        return;

    float sum = 0.0f;
    const size_t row = static_cast<size_t>(y) * width;
    for (int32_t x = 0; x < width; ++x) {
        const float value = static_cast<float>(image[row + x]);
        sum += squared ? value * value : value;
        integral[row + x] = sum;
    }
}

__global__ void IntegralColumnsKernel(float *integral, int32_t width, int32_t height) {
    const int32_t x = blockIdx.x * blockDim.x + threadIdx.x;
    if (x >= width)
        return;

    float sum = 0.0f;
    for (int32_t y = 0; y < height; ++y) {
        sum += integral[static_cast<size_t>(y) * width + x];
        integral[static_cast<size_t>(y) * width + x] = sum;
    }
}

// --------------------------------------------------------------------- NCC --
// One thread per position: mean and variance come from the integral images in
// O(1); the window loop only accumulates the covariance term against the
// pre-centered template (sum(I * (T - mean_T)) = area * cov, because the
// centered template sums to zero). Score = cov / (sigma_I * sigma_T), the
// zero-mean normalized cross-correlation in [-1, 1].
__global__ void NccKernel(const unsigned char *image, const float *integral, const float *integral_sq,
                          const float *templ_centered, const float *templ_var, float *res,
                          int32_t image_width, int32_t image_height, int32_t templ_width,
                          int32_t templ_height) {
    const int32_t tx = blockIdx.x * blockDim.x + threadIdx.x;
    const int32_t ty = blockIdx.y * blockDim.y + threadIdx.y;
    if (tx >= image_width - templ_width + 1 || ty >= image_height - templ_height + 1)
        return;

    const float area = static_cast<float>(templ_width * templ_height);
    const int32_t x2 = tx + templ_width - 1;
    const int32_t y2 = ty + templ_height - 1;

    float sum_i = integral[static_cast<size_t>(y2) * image_width + x2];
    float sum_i2 = integral_sq[static_cast<size_t>(y2) * image_width + x2];
    if (tx > 0) {
        sum_i -= integral[static_cast<size_t>(y2) * image_width + (tx - 1)];
        sum_i2 -= integral_sq[static_cast<size_t>(y2) * image_width + (tx - 1)];
    }
    if (ty > 0) {
        sum_i -= integral[static_cast<size_t>(ty - 1) * image_width + x2];
        sum_i2 -= integral_sq[static_cast<size_t>(ty - 1) * image_width + x2];
    }
    if (tx > 0 && ty > 0) {
        sum_i += integral[static_cast<size_t>(ty - 1) * image_width + (tx - 1)];
        sum_i2 += integral_sq[static_cast<size_t>(ty - 1) * image_width + (tx - 1)];
    }
    const float mean_image = sum_i / area;
    float var_image = sum_i2 / area - mean_image * mean_image;
    var_image = var_image > 0.0f ? var_image : 0.0f;

    const float sqrt_var_image = sqrtf(var_image);
    float score = 0.0f;
    if (sqrt_var_image > kMinStdDev) {
        float sum_it = 0.0f;
        const unsigned char *image_row = image + static_cast<size_t>(ty) * image_width + tx;
        for (int32_t h = 0; h < templ_height; ++h) {
            const unsigned char *image_pixel = image_row + static_cast<size_t>(h) * image_width;
            const float *templ_pixel = templ_centered + static_cast<size_t>(h) * templ_width;
            for (int32_t w = 0; w < templ_width; ++w)
                sum_it += static_cast<float>(image_pixel[w]) * templ_pixel[w];
        }
        score = (sum_it / area) / (sqrt_var_image * sqrtf(*templ_var));
    }
    res[static_cast<size_t>(ty) * (image_width - templ_width + 1) + tx] = score;
}

// ---------------------------------------------------------------- reduction --
// Block-level argmax over the per-position scores; each block atomically
// publishes (order-preserving score key << 32 | position index).
__global__ void BestPositionKernel(const float *res, int32_t total, unsigned long long *best) {
    __shared__ float s_score[kReduceBlock];
    __shared__ unsigned int s_index[kReduceBlock];

    const int tid = threadIdx.x;
    float best_score = -2.0f; // NCC lies in [-1, 1]
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
namespace ncc {

namespace {
constexpr int32_t kMaxResizePixels = NccPipeline::kWorkingSize * NccPipeline::kWorkingSize;
constexpr size_t kMaxResizeBytes = static_cast<size_t>(kMaxResizePixels) * 3;
} // namespace

NccPipeline::NccPipeline(int32_t max_input_bytes) : max_input_bytes_(max_input_bytes) {
    if (!allocate(init_error_))
        return;
    init_ok_ = true;
}

NccPipeline::~NccPipeline() {
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
            std::fprintf(stderr, "NccPipeline cudaFree failed: %s\n", cudaGetErrorString(err));
    };
    free_host(host_templ_resized_);
    free_host(host_templ_centered_);
    free_host(host_best_);
    free_device(dev_image_);
    free_device(dev_gray_);
    free_device(dev_image_resized_);
    free_device(dev_integral_);
    free_device(dev_integral_sq_);
    free_device(dev_result_);
    free_device(dev_best_);
    free_device(dev_templ_);
    free_device(dev_templ_gray_);
    free_device(dev_templ_resized_);
    free_device(dev_templ_centered_);
    free_device(dev_templ_var_);
    if (stream_)
        cudaStreamDestroy(stream_);
}

bool NccPipeline::allocate(std::string &error) {
    // Raw inputs are interleaved color (3 bytes/pixel) or gray (1); the gray
    // mirrors hold one byte per pixel, so a third of the color capacity.
    const int32_t input_capacity = std::max(max_input_bytes_, static_cast<int32_t>(kMaxResizeBytes));
    const size_t gray_capacity = input_capacity / 3 + 16;

    const auto alloc_host = [&](auto **pointer, size_t bytes, const char *what) {
        return ar::core::check_cuda(cudaMallocHost(reinterpret_cast<void **>(pointer), bytes), &error) ||
               (error = std::string("NccPipeline: alloc host ") + what + ": " + error, false);
    };
    const auto alloc_device = [&](auto **pointer, size_t bytes, const char *what) {
        return ar::core::check_cuda(cudaMalloc(reinterpret_cast<void **>(pointer), bytes), &error) ||
               (error = std::string("NccPipeline: alloc device ") + what + ": " + error, false);
    };

    if (!alloc_host(&host_templ_resized_, kMaxResizePixels, "templ_resized"))
        return false;
    if (!alloc_host(&host_templ_centered_, kMaxResizePixels * sizeof(float), "templ_centered"))
        return false;
    if (!alloc_host(&host_best_, sizeof(unsigned long long), "best"))
        return false;

    if (!alloc_device(&dev_image_, input_capacity, "image"))
        return false;
    if (!alloc_device(&dev_gray_, gray_capacity, "gray"))
        return false;
    if (!alloc_device(&dev_image_resized_, kMaxResizePixels, "image_resized"))
        return false;
    if (!alloc_device(&dev_integral_, kMaxResizePixels * sizeof(float), "integral"))
        return false;
    if (!alloc_device(&dev_integral_sq_, kMaxResizePixels * sizeof(float), "integral_sq"))
        return false;
    if (!alloc_device(&dev_result_, static_cast<size_t>(kMaxResizePixels) * sizeof(float), "result"))
        return false;
    if (!alloc_device(&dev_best_, sizeof(unsigned long long), "best"))
        return false;
    if (!alloc_device(&dev_templ_, input_capacity, "templ"))
        return false;
    if (!alloc_device(&dev_templ_gray_, gray_capacity, "templ_gray"))
        return false;
    if (!alloc_device(&dev_templ_resized_, kMaxResizePixels, "templ_resized"))
        return false;
    if (!alloc_device(&dev_templ_centered_, kMaxResizePixels * sizeof(float), "templ_centered"))
        return false;
    if (!alloc_device(&dev_templ_var_, sizeof(float), "templ_var"))
        return false;

    if (!ar::core::check_cuda(cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking), &error))
        return false;
    return true;
}

bool NccPipeline::ok(std::string &error) const {
    if (init_ok_)
        return true;
    error = init_error_.empty() ? "NccPipeline: device buffers were not initialized" : init_error_;
    return false;
}

bool NccPipeline::upload(const unsigned char *host, size_t host_pitch, int32_t width, int32_t height,
                         int32_t channels, unsigned char *device, std::string &error) {
    const size_t row_bytes = static_cast<size_t>(width) * channels;
    return ar::core::check_cuda(cudaMemcpy2DAsync(device, row_bytes, host, host_pitch, row_bytes,
                                                  static_cast<size_t>(height), cudaMemcpyHostToDevice,
                                                  stream_),
                                &error);
}

bool NccPipeline::ensure_template(const unsigned char *templ_data, size_t templ_pitch, int32_t templ_width,
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
    if (static_cast<size_t>(width_resized) * height_resized > kMaxResizePixels) {
        error = "Resized NCC template exceeds the configured buffer";
        return false;
    }

    // Raw upload -> grayscale -> resize -> download once; the statistics below
    // are then computed from the host copy and stay valid for every frame.
    if (!upload(templ_data, templ_pitch, templ_width, templ_height, channels, dev_templ_, error))
        return false;
    const size_t templ_gray_pitch = static_cast<size_t>(templ_width);
    if (channels == 3) {
        const dim3 block(32, 8);
        const dim3 grid((templ_width + block.x - 1) / block.x, (templ_height + block.y - 1) / block.y);
        GrayBgrKernel<<<grid, block, 0, stream_>>>(dev_templ_, static_cast<size_t>(templ_width) * 3,
                                                   dev_templ_gray_, templ_gray_pitch, templ_width,
                                                   templ_height);
        if (!checked(cudaGetLastError(), "launch template grayscale"))
            return false;
    }
    const unsigned char *templ_gray = channels == 3 ? dev_templ_gray_ : dev_templ_;
    if (!ar::vision::resize_cuda(dev_templ_resized_, static_cast<size_t>(width_resized), templ_gray,
                                 templ_gray_pitch, templ_width, templ_height, width_resized, height_resized,
                                 1, ar::vision::Interp::linear, stream_, &error)) {
        error = "resize template: " + error;
        return false;
    }
    if (!checked(cudaMemcpyAsync(host_templ_resized_, dev_templ_resized_,
                                 static_cast<size_t>(width_resized) * height_resized, cudaMemcpyDeviceToHost,
                                 stream_),
                 "download resized template"))
        return false;
    if (!checked(cudaStreamSynchronize(stream_), "sync after template rebuild"))
        return false;

    // Mean/variance of the resized gray template, plus a centered float copy
    // so the per-frame kernel needs no template mean subtraction.
    const int32_t resized_pixels = width_resized * height_resized;
    double sum = 0.0;
    for (int32_t i = 0; i < resized_pixels; ++i)
        sum += host_templ_resized_[i];
    const float mean = static_cast<float>(sum / resized_pixels);
    double sum_sq = 0.0;
    for (int32_t i = 0; i < resized_pixels; ++i) {
        const float value = static_cast<float>(host_templ_resized_[i]) - mean;
        sum_sq += value * value;
    }
    const float variance = static_cast<float>(sum_sq / resized_pixels);
    if (variance < kMinStdDev * kMinStdDev) {
        error = "NCC template has no structure (flat)";
        return false;
    }
    for (int32_t i = 0; i < resized_pixels; ++i)
        host_templ_centered_[i] = static_cast<float>(host_templ_resized_[i]) - mean;

    if (!checked(cudaMemcpyAsync(dev_templ_centered_, host_templ_centered_,
                                 static_cast<size_t>(resized_pixels) * sizeof(float), cudaMemcpyHostToDevice,
                                 stream_),
                 "upload centered template"))
        return false;
    if (!checked(cudaMemcpyAsync(dev_templ_var_, &variance, sizeof(float), cudaMemcpyHostToDevice, stream_),
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

bool NccPipeline::find(const unsigned char *image_data, size_t image_pitch, int32_t image_width,
                       int32_t image_height, const unsigned char *templ_data, size_t templ_pitch,
                       int32_t templ_width, int32_t templ_height, int32_t channels, float threshold,
                       float *out_score, int32_t *out_x, int32_t *out_y, std::string &error) {
    *out_score = -1.0f;
    *out_x = -1;
    *out_y = -1;

    if (!init_ok_) {
        error = init_error_.empty() ? "NccPipeline: device buffers were not initialized" : init_error_;
        return false;
    }
    if (!image_data || !templ_data || (channels != 1 && channels != 3) || image_width <= 0 ||
        image_height <= 0 || templ_width <= 0 || templ_height <= 0 || templ_width > image_width ||
        templ_height > image_height) {
        error = "Invalid NCC input dimensions";
        return false;
    }

    const float ratio =
        static_cast<float>(kWorkingSize) / static_cast<float>(std::max(image_width, image_height));
    const auto scaled = [ratio](int32_t value) {
        return std::max(1, static_cast<int32_t>(std::lround(value * ratio)));
    };
    const int32_t image_width_resized = scaled(image_width);
    const int32_t image_height_resized = scaled(image_height);
    if (static_cast<size_t>(image_width_resized) * image_height_resized > kMaxResizePixels) {
        error = "Resized NCC input exceeds the configured buffer";
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
    const size_t image_gray_pitch = static_cast<size_t>(image_width);
    if (channels == 3) {
        const dim3 block(32, 8);
        const dim3 grid((image_width + block.x - 1) / block.x, (image_height + block.y - 1) / block.y);
        GrayBgrKernel<<<grid, block, 0, stream_>>>(dev_image_, static_cast<size_t>(image_width) * 3,
                                                   dev_gray_, image_gray_pitch, image_width, image_height);
        if (!checked(cudaGetLastError(), "launch grayscale"))
            return false;
    }
    const unsigned char *image_gray = channels == 3 ? dev_gray_ : dev_image_;
    if (!ar::vision::resize_cuda(dev_image_resized_, static_cast<size_t>(image_width_resized), image_gray,
                                 image_gray_pitch, image_width, image_height, image_width_resized,
                                 image_height_resized, 1, ar::vision::Interp::linear, stream_, &error)) {
        error = "resize image: " + error;
        return false;
    }

    // Integral images of the resized gray frame (rows, then columns).
    {
        const dim3 row_block(128, 1);
        const dim3 row_grid((image_height_resized + row_block.x - 1) / row_block.x, 1);
        IntegralRowsKernel<<<row_grid, row_block, 0, stream_>>>(
            dev_integral_, dev_image_resized_, image_width_resized, image_height_resized, false);
        if (!checked(cudaGetLastError(), "launch integral rows"))
            return false;
        IntegralRowsKernel<<<row_grid, row_block, 0, stream_>>>(
            dev_integral_sq_, dev_image_resized_, image_width_resized, image_height_resized, true);
        if (!checked(cudaGetLastError(), "launch integral-sq rows"))
            return false;
        const dim3 column_block(128, 1);
        const dim3 column_grid((image_width_resized + column_block.x - 1) / column_block.x, 1);
        IntegralColumnsKernel<<<column_grid, column_block, 0, stream_>>>(dev_integral_, image_width_resized,
                                                                         image_height_resized);
        if (!checked(cudaGetLastError(), "launch integral columns"))
            return false;
        IntegralColumnsKernel<<<column_grid, column_block, 0, stream_>>>(
            dev_integral_sq_, image_width_resized, image_height_resized);
        if (!checked(cudaGetLastError(), "launch integral-sq columns"))
            return false;
    }

    const int32_t result_width = image_width_resized - templ_width_resized + 1;
    const int32_t result_height = image_height_resized - templ_height_resized + 1;
    {
        const dim3 block(kBlockSide, kBlockSide);
        const dim3 grid((result_width + kBlockSide - 1) / kBlockSide,
                        (result_height + kBlockSide - 1) / kBlockSide);
        NccKernel<<<grid, block, 0, stream_>>>(dev_image_resized_, dev_integral_, dev_integral_sq_,
                                               dev_templ_centered_, dev_templ_var_, dev_result_,
                                               image_width_resized, image_height_resized, templ_width_resized,
                                               templ_height_resized);
        if (!checked(cudaGetLastError(), "launch NCC kernel"))
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
    float best_ncc = -1.0f;
    std::memcpy(&best_ncc, &bits, sizeof(bits));

    *out_score = best_ncc;
    if (best_ncc >= threshold) {
        const int32_t best_x = static_cast<int32_t>(index % static_cast<unsigned int>(result_width));
        const int32_t best_y = static_cast<int32_t>(index / static_cast<unsigned int>(result_width));
        *out_x = static_cast<int32_t>(std::lround(best_x / ratio));
        *out_y = static_cast<int32_t>(std::lround(best_y / ratio));
    }
    return true;
}

} // namespace ncc
} // namespace ar
