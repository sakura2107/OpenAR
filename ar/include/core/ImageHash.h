#pragma once
#include <cstddef>
#include <cstdint>
namespace ar::core {
inline uint64_t image_hash(const unsigned char *data, size_t pitch, int width, int height, int channels) {
    uint64_t hash = 14695981039346656037ull;
    const auto feed = [&](uint8_t byte) { hash = (hash ^ byte) * 1099511628211ull; };
    const int geometry[]{width, height, channels};
    for (int value : geometry)
        for (int i = 0; i < 4; ++i)
            feed(static_cast<uint8_t>(static_cast<uint32_t>(value) >> (8 * i)));
    for (int row = 0; row < height; ++row)
        for (size_t col = 0; col < static_cast<size_t>(width) * channels; ++col)
            feed(data[row * pitch + col]);
    return hash;
}
} // namespace ar::core
