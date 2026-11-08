#pragma once
#include <cuda_runtime.h>

#include <string>

namespace ar {
namespace core {

// Checks the result of a CUDA runtime call or a kernel launch expression.
// Returns true on success. On failure, error_string (if provided) receives
// a readable message so callers can forward it to their own diagnostics.
//
// Examples:
//   if (!ar::core::check_cuda(cudaMalloc(&ptr, size), &msg)) return false;
//   const cudaError_t err = my_kernel<<<grid, block>>>(args);
//   if (!ar::core::check_cuda(err, &msg)) return false;
inline bool check_cuda(cudaError_t err, std::string *error_string = nullptr) {
    if (err == cudaSuccess)
        return true;
    if (error_string)
        *error_string = std::string("CUDA error: ") + cudaGetErrorString(err);
    return false;
}

} // namespace core
} // namespace ar
