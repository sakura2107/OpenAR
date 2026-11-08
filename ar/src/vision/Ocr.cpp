#include "vision/Ocr.h"

#include <format>
#include <string>

namespace ar::vision {

std::optional<OcrItem> IOcrEngine::find_text(const cv::Mat &image, std::string_view text,
                                             const cv::Rect *roi) {
    if (text.empty())
        return std::nullopt;
    for (const auto &item : recognize(image, roi)) {
        if (item.text == text)
            return item;
    }
    return std::nullopt;
}

bool IOcrEngine::contains_text(const cv::Mat &image, std::string_view text, const cv::Rect *roi) {
    return find_text(image, text, roi).has_value();
}

std::unique_ptr<IOcrEngine> create_ocr(std::string_view backend, const OcrParams &params,
                                       ar::log::ILogger *logger) {
    auto engine = OcrRegistry::instance().create(backend, params, logger);
    if (!engine && logger) {
        std::string available;
        OcrRegistry::instance().for_each_name([&available](std::string_view name) {
            available += (available.empty() ? "" : ", ");
            available += name;
        });
        if (OcrRegistry::instance().contains(backend)) {
            logger->log(ar::log::Level::error, "OCR initialization failed: " + std::string(backend));
            return nullptr;
        }
        logger->log(ar::log::Level::error,
                    std::format("Unknown OCR backend '{}' (available: {})", backend, available));
    }
    return engine;
}

std::vector<std::string_view> ocr_backends() {
    std::vector<std::string_view> names;
    OcrRegistry::instance().for_each_name([&names](std::string_view name) { names.push_back(name); });
    return names;
}

} // namespace ar::vision
