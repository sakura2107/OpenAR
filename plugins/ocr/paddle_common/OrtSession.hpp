#pragma once
// Internal to the ocr plugins. Wraps the repetitive ONNX Runtime boilerplate:
// one process-wide Env, CPU or CUDA execution provider, model loading with
// wide-char paths on Windows, and cached input/output names.
#include <onnxruntime_cxx_api.h>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "core/Str.hpp"
#include "log/Logger.h"

namespace ar::paddle {

enum class OrtDevice {
    kCpu,
    kCuda,
};

inline Ort::Env &ort_env() {
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "ar_paddle_ocr");
    return env;
}

// CUDA device id: OcrParams::device_id when set, else AR_OCR_CUDA_DEVICE, else 0.
inline int resolve_cuda_device_id(int params_device_id, ar::log::ILogger *logger) {
    if (params_device_id >= 0)
        return params_device_id;
    const char *value = std::getenv("AR_OCR_CUDA_DEVICE");
    if (!value || *value == '\0')
        return 0;
    const int device_id = std::atoi(value);
    if (device_id < 0) {
        AR_LOG(logger, ar::log::Level::warning,
               std::format("Invalid AR_OCR_CUDA_DEVICE '{}', falling back to device 0", value));
        return 0;
    }
    return device_id;
}

// CUDA EP with heuristic conv search (avoids EXHAUSTIVE benchmark stalls with
// dynamic shapes). Throws Ort::Exception on failure so the caller reports it
// loudly; never falls back to CPU silently.
inline void append_cuda_execution_provider(Ort::SessionOptions &options, int device_id,
                                           ar::log::ILogger *logger) {
    OrtCUDAProviderOptions cuda_options{};
    cuda_options.device_id = device_id;
    cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchHeuristic;
    options.AppendExecutionProvider_CUDA(cuda_options);
    AR_LOG(logger, ar::log::Level::info,
           std::format("OCR CUDA execution provider registered on device {}", device_id));
}

class OrtSession {
  public:
    OrtSession() = delete;
    OrtSession(OrtDevice device, const std::string &model_path, int cuda_device_id,
               ar::log::ILogger *logger, int threads = 2, bool optimize_all = false) {
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(threads);
        options.SetGraphOptimizationLevel(optimize_all ? GraphOptimizationLevel::ORT_ENABLE_ALL : GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        if (device == OrtDevice::kCuda)
            append_cuda_execution_provider(options, resolve_cuda_device_id(cuda_device_id, logger), logger);

        if (model_path.empty()) {
            AR_LOG(logger, ar::log::Level::error, "OCR model path is missing");
            throw std::invalid_argument("OCR model path is missing");
        }
        std::error_code ec;
        if (!std::filesystem::exists(std::filesystem::path(ar::core::utf8_to_wide(model_path)), ec)) {
            AR_LOG(logger, ar::log::Level::error, std::format("Model file {} does not exist", model_path));
            throw std::runtime_error("OCR model does not exist: " + model_path);
        }
        try {
#ifdef _WIN32
            const std::wstring wide_path = ar::core::utf8_to_wide(model_path);
            session_ = std::make_unique<Ort::Session>(ort_env(), wide_path.c_str(), options);
#else
            session_ = std::make_unique<Ort::Session>(ort_env(), model_path.c_str(), options);
#endif
        } catch (const Ort::Exception &e) {
            AR_LOG(logger, ar::log::Level::error,
                   std::format("OCR session creation failed for {}: {}", model_path, e.what()));
            throw;
        }

        Ort::AllocatorWithDefaultOptions allocator;
        input_name_storage_.reserve(session_->GetInputCount());
        output_name_storage_.reserve(session_->GetOutputCount());
        for (size_t i = 0; i < session_->GetInputCount(); ++i)
            input_name_storage_.emplace_back(session_->GetInputNameAllocated(i, allocator).get());
        for (size_t i = 0; i < session_->GetOutputCount(); ++i)
            output_name_storage_.emplace_back(session_->GetOutputNameAllocated(i, allocator).get());
        for (const auto &name : input_name_storage_)
            input_names_.push_back(name.c_str());
        for (const auto &name : output_name_storage_)
            output_names_.push_back(name.c_str());
    }

    [[nodiscard]] bool ok() const {
        return session_ != nullptr && !input_names_.empty() && !output_names_.empty();
    }

    Ort::Session &session() { return *session_; }
    const std::vector<const char *> &input_names() const { return input_names_; }
    const std::vector<const char *> &output_names() const { return output_names_; }

  private:
    std::unique_ptr<Ort::Session> session_;
    std::vector<std::string> input_name_storage_;
    std::vector<std::string> output_name_storage_;
    std::vector<const char *> input_names_;
    std::vector<const char *> output_names_;
};

} // namespace ar::paddle
