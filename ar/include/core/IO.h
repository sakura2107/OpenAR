#pragma once
#include <filesystem>
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include "core/Result.h"
#include "core/Str.hpp"
namespace ar::core {
inline Result<std::vector<uint8_t>> read_bytes(const std::filesystem::path &path,
                                               size_t limit = 64 * 1024 * 1024) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return failure(ErrorCode::io_failed, "read", "Cannot open file: " + wide_to_utf8(path.wstring()));
    const auto size = file.tellg();
    if (size < 0 || uint64_t(size) > limit)
        return failure(ErrorCode::io_failed, "read", "File exceeds byte limit");
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.seekg(0);
    if (!data.empty() && !file.read(reinterpret_cast<char *>(data.data()), data.size()))
        return failure(ErrorCode::io_failed, "read", "File read failed");
    return data;
}
inline Result<cv::Mat> read_image(const std::filesystem::path &path) {
    auto bytes = read_bytes(path);
    if (!bytes)
        return bytes.error();
    try {
        auto image = cv::imdecode(bytes.value(), cv::IMREAD_COLOR);
        if (image.empty())
            return failure(ErrorCode::resource_invalid, "image", "Image cannot be decoded");
        return image;
    } catch (const std::exception &e) {
        return failure(ErrorCode::resource_invalid, "image", e.what());
    }
}
inline Result<void> write_image(const std::filesystem::path &path, const cv::Mat &image) {
    try {
        std::vector<uint8_t> bytes;
        if (!cv::imencode(".png", image, bytes))
            return failure(ErrorCode::io_failed, "image", "Cannot encode image");
        std::ofstream file(path, std::ios::binary);
        if (!file || !file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size()))
            return failure(ErrorCode::io_failed, "image", "Cannot write image");
        return {};
    } catch (const std::exception &e) {
        return failure(ErrorCode::io_failed, "image", e.what());
    }
}
} // namespace ar::core
