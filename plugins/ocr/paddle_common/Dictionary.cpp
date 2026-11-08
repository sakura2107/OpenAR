#include "Dictionary.h"

#include <format>
#include <fstream>
#include <filesystem>
#include <sstream>
#include "core/Str.hpp"

namespace ar::paddle {

bool Dictionary::load(const std::string &path) {
    std::ifstream file(std::filesystem::path(ar::core::utf8_to_wide(path)), std::ios::binary);
    if (!file.is_open()) {
        AR_LOG(logger_, ar::log::Level::error, std::format("Can not open dictionary : {}", path));
        return false;
    }
    std::string bytes{std::istreambuf_iterator<char>(file), {}};
    if (bytes.starts_with("\xEF\xBB\xBF"))
        bytes.erase(0, 3);
    if (bytes.empty() || bytes.size() > 16 * 1024 * 1024)
        return false;
    // Windows 本地模型包可能附带 GBK / GB18030 字典，而插件 ABI 要求 UTF-8。
    // 对整份文件只选择一次编码，避免逐行把 GBK 中碰巧有效的 UTF-8 字节误解码。
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()),
                             nullptr, 0)) {
        const int length = MultiByteToWideChar(54936, MB_ERR_INVALID_CHARS, bytes.data(),
                                               static_cast<int>(bytes.size()), nullptr, 0);
        if (length <= 0)
            return false;
        std::wstring wide(static_cast<size_t>(length), L'\0');
        if (!MultiByteToWideChar(54936, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()),
                                 wide.data(), length))
            return false;
        bytes = ar::core::wide_to_utf8(wide);
        AR_LOG(logger_, ar::log::Level::warning, "Legacy GB18030 OCR dictionary converted to UTF-8");
    }
    std::istringstream input(bytes);
    std::vector<std::string> characters;
    std::unordered_map<std::string, size_t> indices;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        // 空行不能静默跳过，否则所有后续模型类别会错位；单空格是有效字典项。
        if (line.empty() || line.find('\0') != std::string::npos)
            return false;
        indices[line] = characters.size();
        characters.push_back(line);
    }
    if (characters.empty())
        return false;
    dictionary_ = std::move(characters);
    char_to_index_ = std::move(indices);
    AR_LOG(logger_, ar::log::Level::debug,
           std::format("Success to load dictionary, mount : {}", dictionary_.size()));
    return true;
}

} // namespace ar::paddle
