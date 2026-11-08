#pragma once
#include "vision/Ocr.h"
#include "core/Json.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <set>
#include <regex>
#include <cctype>

namespace ar::paddle {
struct Normalization {
    bool rgb = true;
    bool normalize_before_swap = true; // Preserve the v1/v2 baseline explicitly.
    float scale = 1.0f / 255.0f;
    std::array<float, 3> mean{0.5f, 0.5f, 0.5f};
    std::array<float, 3> stddev{0.5f, 0.5f, 0.5f};
};
struct RecConfig {
    int threads = 2;
    bool optimize_all = false;
    Normalization norm;
    int height = 0, dynamic_height = 0;
    int min_width = 320, max_width = 4096, width_multiple = 32;
    int max_batch = 8;
    bool left_align = false, normalized_padding = false, use_space = true, sequence_mean = false;
    float padding = 114;
    std::string number_characters = "0123456789,/xX×.:%+- ";
    int number_min_width = 64;
    bool katakana_only = false;
    std::set<std::string> blocked_characters;
    float context_horizontal = 0, context_vertical = 0;
};

inline bool katakana_token(const std::string &token) {
    const auto value = ar::core::utf8_to_wide(token);
    if (value.size() != 1) return false;
    const auto c = value.front();
    return (c >= 0x30a1 && c <= 0x30fa) || (c >= 0x30fc && c <= 0x30ff) ||
           c == L'(' || c == L')' || c == 0xff08 || c == 0xff09 || c == L' ';
}

inline cv::Mat recognition_context(const cv::Mat &crop, const RecConfig &config) {
    if (!config.context_horizontal && !config.context_vertical) return crop;
    const int horizontal = static_cast<int>(std::ceil(crop.rows * config.context_horizontal));
    const int vertical = static_cast<int>(std::ceil(crop.rows * config.context_vertical));
    if (int64_t(crop.cols + 2 * horizontal) * (crop.rows + 2 * vertical) > 16 * 1024 * 1024)
        throw std::invalid_argument("OCR context padding exceeds image budget");
    cv::Mat result;
    cv::copyMakeBorder(crop, result, vertical, vertical, horizontal, horizontal, cv::BORDER_REPLICATE);
    return result;
}
struct DetConfig {
    int threads = 2;
    bool optimize_all = false;
    Normalization norm{true, true, 1.0f / 255.0f, {0.485f, 0.456f, 0.406f}, {0.229f, 0.224f, 0.225f}};
    int limit_side = 0;
    bool limit_min = false, ceil_resize = true;
    float bitmap_threshold = 0.5f;
};
struct PaddleConfig {
    ar::vision::OcrParams params;
    RecConfig rec;
    DetConfig det;
    bool separate_numbers = false;
    ar::vision::OcrParams number_params;
    RecConfig number_rec;
    bool number_fallback = false;
    bool number_fallback_on_invalid_syntax = false;
    float fallback_below_confidence = 0.85f;
    ar::vision::OcrParams fallback_params;
    RecConfig fallback_rec;
    bool number_second_fallback = false;
    ar::vision::OcrParams second_fallback_params;
    RecConfig second_fallback_rec;
};

// A routing check only: never inserts, removes or repairs operators in output.
// In particular, "66" is valid and cannot be inferred to mean "6/6".
inline bool numeric_syntax_valid(std::string text) {
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); }), text.end());
    for (auto &c : text) if (c == 'X') c = 'x';
    for (size_t at = 0; (at = text.find("\xC3\x97", at)) != std::string::npos; ++at) text.replace(at, 2, "x");
    static const std::string number = "(?:[0-9]{1,3}(?:,[0-9]{3})+|[0-9]+)(?:\\.[0-9]+)?";
    static const std::regex grammar("(?:" + number + "/" + number + "|" + number + "-" + number +
        "|x" + number + "|[+-]?" + number + "%|" + number + "-|[+-]?" + number + ")");
    return std::regex_match(text, grammar);
}

inline void config_fields(const rapidjson::Value &v, std::initializer_list<const char *> required,
                           std::initializer_list<const char *> optional = {}) {
    if (!v.IsObject()) throw std::invalid_argument("OCR config section must be an object");
    std::set<std::string> keys;
    for (const auto *key : required) {
        keys.insert(key);
        if (!v.HasMember(key)) throw std::invalid_argument(std::string("OCR config missing: ") + key);
    }
    for (const auto *key : optional) keys.insert(key);
    for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it)
        if (!keys.contains(it->name.GetString()))
            throw std::invalid_argument(std::string("Unknown OCR config field: ") + it->name.GetString());
}
inline std::string config_text(const rapidjson::Value &v, const char *key) {
    if (!v[key].IsString() || v[key].GetStringLength() == 0)
        throw std::invalid_argument(std::string("OCR config string expected: ") + key);
    return {v[key].GetString(), v[key].GetStringLength()};
}
inline int config_int(const rapidjson::Value &v, const char *key, int low, int high) {
    if (!v[key].IsInt() || v[key].GetInt() < low || v[key].GetInt() > high)
        throw std::invalid_argument(std::string("OCR config integer out of range: ") + key);
    return v[key].GetInt();
}
inline float config_real(const rapidjson::Value &v, const char *key, float low, float high) {
    if (!v[key].IsNumber() || !std::isfinite(v[key].GetDouble()) || v[key].GetDouble() < low || v[key].GetDouble() > high)
        throw std::invalid_argument(std::string("OCR config number out of range: ") + key);
    return static_cast<float>(v[key].GetDouble());
}
inline bool config_bool(const rapidjson::Value &v, const char *key) {
    if (!v[key].IsBool()) throw std::invalid_argument(std::string("OCR config bool expected: ") + key);
    return v[key].GetBool();
}
inline bool config_choice(const rapidjson::Value &v, const char *key, const char *yes, const char *no) {
    const auto value = config_text(v, key);
    if (value != yes && value != no) throw std::invalid_argument(std::string("Unsupported OCR config: ") + key);
    return value == yes;
}
inline Normalization config_norm(const rapidjson::Value &v) {
    Normalization result;
    result.rgb = config_choice(v, "channel_order", "RGB", "BGR");
    result.normalize_before_swap = config_choice(v, "normalization_order", "before_channel_order", "after_channel_order");
    result.scale = config_real(v, "scale", 1e-9f, 1);
    for (auto pair : {std::pair{"mean", &result.mean}, std::pair{"std", &result.stddev}}) {
        const auto &a = v[pair.first];
        if (!a.IsArray() || a.Size() != 3) throw std::invalid_argument("OCR mean/std must have three channels");
        for (size_t i = 0; i < 3; ++i) {
            if (!a[static_cast<rapidjson::SizeType>(i)].IsNumber()) throw std::invalid_argument("OCR mean/std must be numeric");
            const double x = a[static_cast<rapidjson::SizeType>(i)].GetDouble();
            if (!std::isfinite(x) || std::abs(x) > 1000 || (pair.first == std::string_view("std") && x <= 0))
                throw std::invalid_argument("OCR normalization is invalid");
            (*pair.second)[i] = static_cast<float>(x);
        }
    }
    return result;
}
inline cv::Mat normalize_ocr(const cv::Mat &input, const Normalization &norm) {
    cv::Mat result;
    if (norm.rgb && !norm.normalize_before_swap) cv::cvtColor(input, result, cv::COLOR_BGR2RGB);
    else result = input;
    result.convertTo(result, CV_32FC3, norm.scale);
    cv::subtract(result, cv::Scalar(norm.mean[0], norm.mean[1], norm.mean[2]), result);
    cv::divide(result, cv::Scalar(norm.stddev[0], norm.stddev[1], norm.stddev[2]), result);
    if (norm.rgb && norm.normalize_before_swap) cv::cvtColor(result, result, cv::COLOR_BGR2RGB);
    return result;
}
inline PaddleConfig resolve_config(const ar::vision::OcrParams &params) {
    PaddleConfig config;
    config.params = params;
    config.det.bitmap_threshold = params.box_threshold;
    if (params.profile.empty()) return config;
    const auto file = std::filesystem::absolute(ar::core::utf8_to_wide(params.profile));
    auto loaded = ar::core::read_json(file);
    if (!loaded) throw std::invalid_argument(loaded.error().message);
    const auto &root = loaded.value();
    config_fields(root, {"schema_version", "id", "language", "detector", "recognizer", "numbers"}, {"status", "provenance", "runtime"});
    config_int(root, "schema_version", 1, 1);
    config_text(root, "id"); config_text(root, "language"); // Metadata, never a language switch.
    auto model = [&](const rapidjson::Value &v, const char *key) {
        auto value = std::filesystem::path(ar::core::utf8_to_wide(config_text(v, key)));
        value = std::filesystem::weakly_canonical(value.is_absolute() ? value : file.parent_path() / value);
        if (!std::filesystem::is_regular_file(value)) throw std::invalid_argument("Missing OCR asset: " + ar::core::wide_to_utf8(value.wstring()));
        return ar::core::wide_to_utf8(value.wstring());
    };
    const auto &d = root["detector"], &r = root["recognizer"], &n = root["numbers"];
    config_fields(d, {"model", "channel_order", "normalization_order", "scale", "mean", "std", "limit_type", "limit_side_len",
                      "resize_rounding", "bitmap_threshold", "box_threshold", "unclip_ratio", "score_mode", "line_merge_ratio"});
    config_fields(r, {"model", "dictionary", "input_height", "dynamic_height", "minimum_width", "maximum_width", "width_multiple",
                      "max_batch_size", "channel_order", "normalization_order", "scale", "mean", "std", "alignment", "padding_stage",
                      "padding_value", "blank_index", "use_space_char", "confidence_mode", "text_threshold"}, {"character_set", "context_padding", "ctc_mask"});
    config_fields(n, {"allowed_characters", "minimum_width"}, {"model", "dictionary", "input_height", "dynamic_height", "use_space_char", "text_threshold", "intra_op_threads", "graph_optimization", "fallback"});
    config.params.det_model = model(d, "model");
    config.params.rec_model = model(r, "model");
    config.params.dictionary = model(r, "dictionary");
    config.det.norm = config_norm(d); config.rec.norm = config_norm(r);
    config.det.limit_side = config_int(d, "limit_side_len", 0, 8192);
    config.det.limit_min = config_choice(d, "limit_type", "min", "max");
    config.det.ceil_resize = config_choice(d, "resize_rounding", "ceil", "nearest");
    config.det.bitmap_threshold = config_real(d, "bitmap_threshold", 0, 1);
    config.params.box_threshold = config_real(d, "box_threshold", 0, 1);
    config.params.unclip_ratio = config_real(d, "unclip_ratio", 0.01f, 10);
    config.params.score_mode = config_choice(d, "score_mode", "fast", "slow");
    config.params.det_line_merge_ratio = config_real(d, "line_merge_ratio", 0, 10);
    if (r["input_height"].IsString()) {
        if (config_text(r, "input_height") != "model") throw std::invalid_argument("input_height must be model or an integer");
    } else config.rec.height = config_int(r, "input_height", 8, 256);
    config.rec.dynamic_height = r["dynamic_height"].IsNull() ? 0 : config_int(r, "dynamic_height", 8, 256);
    config.rec.min_width = config_int(r, "minimum_width", 8, 4096);
    config.rec.max_width = config_int(r, "maximum_width", config.rec.min_width, 8192);
    config.rec.width_multiple = config_int(r, "width_multiple", 1, 128);
    config.rec.max_batch = config_int(r, "max_batch_size", 1, 32);
    config.rec.left_align = config_choice(r, "alignment", "left", "center");
    config.rec.normalized_padding = config_choice(r, "padding_stage", "normalized", "raw");
    config.rec.padding = config_real(r, "padding_value", -255, 255);
    config_int(r, "blank_index", 0, 0);
    config.rec.use_space = config_bool(r, "use_space_char");
    config.rec.sequence_mean = config_choice(r, "confidence_mode", "sequence_mean", "character_filter");
    config.params.text_threshold = config_real(r, "text_threshold", 0, 1);
    if (r.HasMember("character_set")) config.rec.katakana_only = config_choice(r, "character_set", "katakana", "unrestricted");
    if (r.HasMember("ctc_mask")) {
        const auto &mask = r["ctc_mask"];
        config_fields(mask, {"version", "source_sha256", "blocked_characters"});
        config_text(mask, "version");
        const auto hash = config_text(mask, "source_sha256");
        if (hash.size() != 64 || hash.find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::invalid_argument("Invalid CTC mask source SHA256");
        const auto chars = ar::core::utf8_to_wide(config_text(mask, "blocked_characters"));
        for (size_t i=0;i<chars.size();++i) {
            const auto c=chars[i];std::wstring token(1,c);
            if (c<0x80 || (c>=0xdc00 && c<=0xdfff)) throw std::invalid_argument("Invalid CTC blocked character");
            if (c>=0xd800 && c<=0xdbff) {
                if (++i>=chars.size() || chars[i]<0xdc00 || chars[i]>0xdfff) throw std::invalid_argument("Invalid CTC Unicode scalar");
                token+=chars[i];
            }
            config.rec.blocked_characters.insert(ar::core::wide_to_utf8(token));
        }
    }
    if (r.HasMember("context_padding")) {
        const auto &padding = r["context_padding"];
        config_fields(padding, {"horizontal_ratio", "vertical_ratio", "mode"});
        if (config_text(padding, "mode") != "replicate") throw std::invalid_argument("OCR context padding supports replicate only");
        config.rec.context_horizontal = config_real(padding, "horizontal_ratio", 0, 1);
        config.rec.context_vertical = config_real(padding, "vertical_ratio", 0, 1);
    }
    config.rec.number_characters = config_text(n, "allowed_characters");
    config.rec.number_min_width = config_int(n, "minimum_width", 8, config.rec.max_width);
    if (root.HasMember("runtime")) {
        const auto &runtime = root["runtime"];
        if (!runtime.IsObject()) throw std::invalid_argument("OCR runtime must be an object");
        if (runtime.HasMember("detector")) {
            config_fields(runtime, {"detector", "recognizer"});
            for (const auto *key : {"detector", "recognizer"})
                config_fields(runtime[key], {"intra_op_threads", "graph_optimization"});
            config.det.threads = config_int(runtime["detector"], "intra_op_threads", 1, 32);
            config.rec.threads = config_int(runtime["recognizer"], "intra_op_threads", 1, 32);
            config.det.optimize_all = config_choice(runtime["detector"], "graph_optimization", "all", "extended");
            config.rec.optimize_all = config_choice(runtime["recognizer"], "graph_optimization", "all", "extended");
        } else {
            config_fields(runtime, {"intra_op_threads", "graph_optimization"});
            config.rec.threads = config.det.threads = config_int(runtime, "intra_op_threads", 1, 32);
            config.rec.optimize_all = config.det.optimize_all = config_choice(runtime, "graph_optimization", "all", "extended");
        }
    }
    if (n.HasMember("model")) {
        if (!n.HasMember("dictionary")) throw std::invalid_argument("Numeric model requires its own dictionary");
        config.separate_numbers = true;
        config.number_params = config.params;
        config.number_rec = config.rec;
        // Text-only options must not silently alter the numeric models.
        config.number_rec.katakana_only = false;
        config.number_rec.blocked_characters.clear();
        config.number_rec.context_horizontal = config.number_rec.context_vertical = 0;
        config.number_params.rec_model = model(n, "model");
        config.number_params.dictionary = model(n, "dictionary");
        config.number_rec.height = 0;
        config.number_rec.dynamic_height = n.HasMember("dynamic_height") && !n["dynamic_height"].IsNull() ? config_int(n, "dynamic_height", 8, 256) : 0;
        if (n.HasMember("input_height")) {
            if (n["input_height"].IsString()) {
                if (config_text(n, "input_height") != "model") throw std::invalid_argument("Invalid numeric input_height");
            } else config.number_rec.height = config_int(n, "input_height", 8, 256);
        }
        if (n.HasMember("use_space_char")) config.number_rec.use_space = config_bool(n, "use_space_char");
        if (n.HasMember("text_threshold")) config.number_params.text_threshold = config_real(n, "text_threshold", 0, 1);
        if (n.HasMember("intra_op_threads")) config.number_rec.threads = config_int(n, "intra_op_threads", 1, 32);
        if (n.HasMember("graph_optimization")) config.number_rec.optimize_all = config_choice(n, "graph_optimization", "all", "extended");
        if (n.HasMember("fallback")) {
            const auto &f = n["fallback"];
            config_fields(f, {"model", "dictionary", "below_confidence", "use_space_char", "text_threshold", "intra_op_threads", "graph_optimization"}, {"on_invalid_syntax", "next"});
            config.number_fallback = true;
            config.number_fallback_on_invalid_syntax = f.HasMember("on_invalid_syntax") && config_bool(f, "on_invalid_syntax");
            config.fallback_below_confidence = config_real(f, "below_confidence", 0, 1);
            config.fallback_params = config.number_params;
            config.fallback_rec = config.number_rec;
            config.fallback_params.rec_model = model(f, "model");
            config.fallback_params.dictionary = model(f, "dictionary");
            config.fallback_params.text_threshold = config_real(f, "text_threshold", 0, 1);
            config.fallback_rec.height = config.fallback_rec.dynamic_height = 0;
            config.fallback_rec.use_space = config_bool(f, "use_space_char");
            config.fallback_rec.threads = config_int(f, "intra_op_threads", 1, 32);
            config.fallback_rec.optimize_all = config_choice(f, "graph_optimization", "all", "extended");
            if (f.HasMember("next")) {
                const auto &next=f["next"];
                config_fields(next,{"model","dictionary","use_space_char","text_threshold","intra_op_threads","graph_optimization"});
                config.number_second_fallback=true;
                config.second_fallback_params=config.fallback_params;
                config.second_fallback_rec=config.fallback_rec;
                config.second_fallback_params.rec_model=model(next,"model");
                config.second_fallback_params.dictionary=model(next,"dictionary");
                config.second_fallback_params.text_threshold=config_real(next,"text_threshold",0,1);
                config.second_fallback_rec.use_space=config_bool(next,"use_space_char");
                config.second_fallback_rec.threads=config_int(next,"intra_op_threads",1,32);
                config.second_fallback_rec.optimize_all=config_choice(next,"graph_optimization","all","extended");
            }
        }
    } else {
        for (auto it = n.MemberBegin(); it != n.MemberEnd(); ++it)
            if (std::string_view(it->name.GetString()) != "allowed_characters" && std::string_view(it->name.GetString()) != "minimum_width")
                throw std::invalid_argument("Numeric model options require a numeric model");
    }
    return config;
}
} // namespace ar::paddle
