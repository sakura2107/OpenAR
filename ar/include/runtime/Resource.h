#pragma once
#include <map>
#include "runtime/Session.h"
#include "vision/Ocr.h"
namespace ar::runtime {
enum class RecognitionType { direct, templ, ocr, all, any, custom };
enum class TextMode { exact, contains, regex };
struct Recognition {
    RecognitionType type = RecognitionType::direct;
    std::string asset, backend = "ncc-cpu", text, custom, params = "{}";
    float threshold = 0.9f;
    TextMode text_mode = TextMode::exact;
    std::optional<cv::Rect> roi;
    bool negate = false;
    std::vector<Recognition> conditions;
};
struct Node {
    std::string name;
    Recognition recognition;
    Action action;
    std::string action_params = "{}";
    std::vector<std::string> next, on_error;
    uint32_t timeout_ms = 5000, max_visits = 10;
};
struct RunLimits {
    uint32_t timeout_ms = 60000, poll_ms = 100, max_transitions = 1000, max_recoveries = 3;
};
struct ResourceBundle {
    std::string name, version, entry;
    std::filesystem::path root;
    cv::Size reference_size;
    RunLimits limits;
    std::map<std::string, cv::Mat> templates;
    std::map<std::string, Node> nodes;
    std::optional<ar::vision::OcrParams> ocr;
    std::string ocr_backend = "paddle-ocr-cpu";
    std::vector<std::string> warnings;
    static ar::core::Result<std::shared_ptr<const ResourceBundle>>
    load(const std::filesystem::path &directory, const std::filesystem::path &shared_model_directory = {});
};
const char *manifest_schema();
const char *pipeline_schema();
} // namespace ar::runtime
