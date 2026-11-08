#include "runtime/Resource.h"
#include "core/Str.hpp"
#include "core/IO.h"
#include "core/Json.h"
#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <regex>
#include <set>
#include <functional>
namespace ar::runtime {
using ar::core::ErrorCode;
using ar::core::failure;
using rapidjson::Value;
const char *manifest_schema() {
    return R"json({"type":"object","additionalProperties":false,"required":["schema_version","name","version","reference_size","pipeline","templates"],"properties":{
"schema_version":{"enum":[1]},"name":{"type":"string","minLength":1,"maxLength":128},"version":{"type":"string","minLength":1,"maxLength":128},
"reference_size":{"type":"array","minItems":2,"maxItems":2,"items":{"type":"integer","minimum":1,"maximum":8192}},
"pipeline":{"type":"string","minLength":1},"templates":{"type":"object","additionalProperties":{"type":"string","minLength":1}},
"ocr":{"type":"object","additionalProperties":false,"oneOf":[{"required":["profile"]},{"required":["det_model","rec_model","dictionary"]}],"properties":{"profile":{"type":"string","minLength":1},"backend":{"type":"string"},"det_model":{"type":"string"},"rec_model":{"type":"string"},"dictionary":{"type":"string"},"text_threshold":{"type":"number","minimum":0,"maximum":1},"box_threshold":{"type":"number","minimum":0,"maximum":1},"unclip_ratio":{"type":"number","exclusiveMinimum":true,"minimum":0},"det_line_merge_ratio":{"type":"number","minimum":0},"device_id":{"type":"integer","minimum":-1}}}}})json";
}
const char *pipeline_schema() {
    return R"json({"type":"object","additionalProperties":false,"required":["entry","nodes"],"properties":{
"entry":{"type":"string","minLength":1},"limits":{"type":"object","additionalProperties":false,"properties":{"timeout_ms":{"type":"integer","minimum":1,"maximum":86400000},"poll_ms":{"type":"integer","minimum":1,"maximum":60000},"max_transitions":{"type":"integer","minimum":1,"maximum":10000000},"max_recoveries":{"type":"integer","minimum":0,"maximum":1000000}}},
"nodes":{"type":"object","minProperties":1,"maxProperties":10000,"additionalProperties":{"type":"object","additionalProperties":false,"required":["recognition","action"],"properties":{
"recognition":{"$ref":"#/definitions/recognition"},"action":{"$ref":"#/definitions/action"},"next":{"type":"array","maxItems":100,"items":{"type":"string","minLength":1}},"on_error":{"type":"array","maxItems":100,"items":{"type":"string","minLength":1}},"timeout_ms":{"type":"integer","minimum":1,"maximum":3600000},"max_visits":{"type":"integer","minimum":1,"maximum":1000000}}}}},
"definitions":{"point":{"type":"array","minItems":2,"maxItems":2,"items":{"type":"integer","minimum":0,"maximum":8191}},"rect":{"type":"array","minItems":4,"maxItems":4,"items":{"type":"integer","minimum":0,"maximum":8192}},
"recognition":{"type":"object","additionalProperties":false,"required":["type"],"properties":{"type":{"enum":["Direct","Template","OCR","All","Any","Custom"]},"template":{"type":"string","minLength":1},"backend":{"type":"string","minLength":1},"text":{"type":"string","minLength":1},"mode":{"enum":["Exact","Contains","Regex"]},"threshold":{"type":"number","minimum":-1,"maximum":1},"roi":{"$ref":"#/definitions/rect"},"negate":{"type":"boolean"},"conditions":{"type":"array","minItems":1,"maxItems":64,"items":{"$ref":"#/definitions/recognition"}},"custom":{"type":"string","minLength":1},"params":{}}},
"action":{"type":"object","additionalProperties":false,"required":["type"],"properties":{"type":{"enum":["None","Click","LongPress","Swipe","Key","Wait","StartApp","StopApp","Reconnect","Custom"]},"target":{"$ref":"#/definitions/point"},"to":{"$ref":"#/definitions/point"},"duration_ms":{"type":"integer","minimum":0,"maximum":3600000},"keycode":{"type":"integer","minimum":0,"maximum":10000},"package":{"type":"string","minLength":1},"custom":{"type":"string","minLength":1},"params":{}}}}})json";
}
namespace {
std::string text(const Value &v, const char *key, std::string fallback = {}) {
    return v.HasMember(key) ? std::string(v[key].GetString(), v[key].GetStringLength()) : std::move(fallback);
}
uint32_t number(const Value &v, const char *key, uint32_t fallback) {
    return v.HasMember(key) ? v[key].GetUint() : fallback;
}
float real(const Value &v, const char *key, float fallback) {
    return v.HasMember(key) ? v[key].GetFloat() : fallback;
}
std::string params(const Value &v) {
    if (!v.HasMember("params"))
        return "{}";
    rapidjson::StringBuffer out;
    rapidjson::Writer<rapidjson::StringBuffer> writer(out);
    v["params"].Accept(writer);
    return out.GetString();
}
ar::core::Result<rapidjson::Document> json(const std::filesystem::path &path, const char *schema) {
    return ar::core::read_json(path, schema);
}
std::filesystem::path inside(const std::filesystem::path &root, const std::string &relative) {
    if (relative.find('\0') != std::string::npos)
        throw std::invalid_argument("NUL in asset path");
    const auto path =
        std::filesystem::weakly_canonical(root / std::filesystem::path(ar::core::utf8_to_wide(relative)));
    auto r = root.begin(), p = path.begin();
    for (; r != root.end(); ++r, ++p)
        if (p == path.end() || *r != *p)
            throw std::invalid_argument("Asset path escapes its declared root: " + relative);
    return path;
}
Recognition recognition(const Value &v, const ResourceBundle &bundle, int depth) {
    if (depth > 16)
        throw std::invalid_argument("Recognition nesting exceeds 16 levels");
    Recognition r;
    const auto type = text(v, "type");
    if (type == "Template")
        r.type = RecognitionType::templ;
    else if (type == "OCR")
        r.type = RecognitionType::ocr;
    else if (type == "All")
        r.type = RecognitionType::all;
    else if (type == "Any")
        r.type = RecognitionType::any;
    else if (type == "Custom")
        r.type = RecognitionType::custom;
    r.asset = text(v, "template");
    r.backend = text(v, "backend", "ncc-cpu");
    r.text = text(v, "text");
    r.custom = text(v, "custom");
    r.params = params(v);
    r.threshold = real(v, "threshold", r.type == RecognitionType::ocr ? 0.5f : 0.9f);
    r.negate = v.HasMember("negate") && v["negate"].GetBool();
    const auto mode = text(v, "mode", "Exact");
    r.text_mode = mode == "Regex"      ? TextMode::regex
                  : mode == "Contains" ? TextMode::contains
                                       : TextMode::exact;
    if (v.HasMember("roi")) {
        const auto &a = v["roi"];
        r.roi = cv::Rect(a[0].GetInt(), a[1].GetInt(), a[2].GetInt(), a[3].GetInt());
        if (!ar::core::valid_roi(*r.roi, bundle.reference_size))
            throw std::invalid_argument("ROI outside reference screen");
    }
    if (r.type == RecognitionType::templ && !bundle.templates.contains(r.asset))
        throw std::invalid_argument("Unknown template: " + r.asset);
    if (r.type == RecognitionType::ocr) {
        if (r.text.empty() || !bundle.ocr || r.threshold < 0)
            throw std::invalid_argument("OCR needs text, model config and nonnegative threshold");
        if (r.text_mode == TextMode::regex)
            std::regex(r.text);
    }
    if (r.type == RecognitionType::custom && r.custom.empty())
        throw std::invalid_argument("Custom recognition needs a name");
    if (r.type == RecognitionType::all || r.type == RecognitionType::any) {
        if (!v.HasMember("conditions"))
            throw std::invalid_argument("All/Any needs conditions");
        for (const auto &child : v["conditions"].GetArray())
            r.conditions.push_back(recognition(child, bundle, depth + 1));
    }
    return r;
}
Action action(const Value &v, const cv::Size &size) {
    Action a;
    const auto type = text(v, "type");
    const std::map<std::string, ActionType> types{{"None", ActionType::none},
                                                  {"Click", ActionType::click},
                                                  {"LongPress", ActionType::long_press},
                                                  {"Swipe", ActionType::swipe},
                                                  {"Key", ActionType::key},
                                                  {"Wait", ActionType::wait},
                                                  {"StartApp", ActionType::start_app},
                                                  {"StopApp", ActionType::stop_app},
                                                  {"Reconnect", ActionType::reconnect},
                                                  {"Custom", ActionType::custom}};
    a.type = types.at(type);
    a.duration_ms = number(v, "duration_ms", 0);
    a.keycode = number(v, "keycode", 0);
    a.package = text(v, "package");
    a.custom = text(v, "custom");
    auto point = [&](const char *key) {
        const auto &p = v[key];
        cv::Point value(p[0].GetInt(), p[1].GetInt());
        if (value.x >= size.width || value.y >= size.height)
            throw std::invalid_argument(std::string(key) + " outside reference screen");
        return value;
    };
    if (v.HasMember("target"))
        a.target = point("target");
    if (v.HasMember("to"))
        a.to = point("to");
    if (a.type == ActionType::swipe && !v.HasMember("to"))
        throw std::invalid_argument("Swipe needs destination");
    if ((a.type == ActionType::wait || a.type == ActionType::long_press || a.type == ActionType::swipe) &&
        !v.HasMember("duration_ms"))
        throw std::invalid_argument("Action needs duration_ms");
    if (a.type == ActionType::key && !v.HasMember("keycode"))
        throw std::invalid_argument("Key needs keycode");
    if ((a.type == ActionType::start_app || a.type == ActionType::stop_app) && a.package.empty())
        throw std::invalid_argument("App action needs package");
    if (a.type == ActionType::custom && a.custom.empty())
        throw std::invalid_argument("Custom action needs name");
    return a;
}
} // namespace
ar::core::Result<std::shared_ptr<const ResourceBundle>>
ResourceBundle::load(const std::filesystem::path &directory, const std::filesystem::path &models) {
    try {
        auto bundle = std::make_shared<ResourceBundle>();
        bundle->root = std::filesystem::canonical(directory);
        auto manifest = json(bundle->root / L"manifest.json", manifest_schema());
        if (!manifest)
            return manifest.error();
        const auto &m = manifest.value();
        bundle->name = text(m, "name");
        bundle->version = text(m, "version");
        bundle->reference_size = {m["reference_size"][0].GetInt(), m["reference_size"][1].GetInt()};
        if (uint64_t(bundle->reference_size.width) * bundle->reference_size.height > 16 * 1024 * 1024)
            throw std::invalid_argument("Reference screen exceeds pixel budget");
        size_t template_bytes = 0;
        for (auto it = m["templates"].MemberBegin(); it != m["templates"].MemberEnd(); ++it) {
            auto image = ar::core::read_image(inside(bundle->root, it->value.GetString()));
            if (!image)
                return image.error();
            template_bytes += image.value().total() * image.value().elemSize();
            if (template_bytes > 256 * 1024 * 1024)
                throw std::invalid_argument("Templates exceed 256 MiB budget");
            if (image.value().cols > bundle->reference_size.width ||
                image.value().rows > bundle->reference_size.height)
                throw std::invalid_argument("Template larger than reference screen");
            bundle->templates.emplace(it->name.GetString(), image.value());
        }
        if (m.HasMember("ocr")) {
            const auto &o = m["ocr"];
            ar::vision::OcrParams p;
            const auto model_root = models.empty() ? bundle->root : std::filesystem::canonical(models);
            auto model = [&](const char *key) {
                auto path = inside(model_root, text(o, key));
                if (!std::filesystem::is_regular_file(path))
                    throw std::invalid_argument(std::string("Missing OCR ") + key);
                return ar::core::wide_to_utf8(path.wstring());
            };
            if (o.HasMember("profile")) {
                for (auto it = o.MemberBegin(); it != o.MemberEnd(); ++it) {
                    const std::string_view key = it->name.GetString();
                    if (key != "profile" && key != "backend" && key != "device_id")
                        throw std::invalid_argument("OCR profile cannot be mixed with legacy model/preprocessing parameters");
                }
                p.profile = model("profile");
            } else {
                p.det_model = model("det_model");
                p.rec_model = model("rec_model");
                p.dictionary = model("dictionary");
            }
            p.text_threshold = real(o, "text_threshold", 0.5f);
            p.box_threshold = real(o, "box_threshold", 0.5f);
            p.unclip_ratio = real(o, "unclip_ratio", 1.5f);
            p.det_line_merge_ratio = real(o, "det_line_merge_ratio", 0.5f);
            p.device_id = o.HasMember("device_id") ? o["device_id"].GetInt() : -1;
            bundle->ocr = p;
            bundle->ocr_backend = text(o, "backend", "paddle-ocr-cpu");
        }
        auto pipeline = json(inside(bundle->root, text(m, "pipeline")), pipeline_schema());
        if (!pipeline)
            return pipeline.error();
        const auto &p = pipeline.value();
        bundle->entry = text(p, "entry");
        if (p.HasMember("limits")) {
            const auto &l = p["limits"];
            bundle->limits = {number(l, "timeout_ms", 60000), number(l, "poll_ms", 100),
                              number(l, "max_transitions", 1000), number(l, "max_recoveries", 3)};
        }
        for (auto it = p["nodes"].MemberBegin(); it != p["nodes"].MemberEnd(); ++it) {
            const auto &v = it->value;
            Node n;
            n.name = it->name.GetString();
            n.timeout_ms = number(v, "timeout_ms", 5000);
            n.max_visits = number(v, "max_visits", 10);
            n.recognition = recognition(v["recognition"], *bundle, 0);
            n.action = action(v["action"], bundle->reference_size);
            n.action_params = params(v["action"]);
            for (const char *key : {"next", "on_error"})
                if (v.HasMember(key))
                    for (const auto &item : v[key].GetArray())
                        (std::string_view(key) == "next" ? n.next : n.on_error)
                            .emplace_back(item.GetString());
            if ((n.action.type == ActionType::click || n.action.type == ActionType::long_press ||
                 n.action.type == ActionType::swipe) &&
                !n.action.target && (n.recognition.type == RecognitionType::direct || n.recognition.negate))
                throw std::invalid_argument("Input node needs an explicit target: " + n.name);
            bundle->nodes.emplace(n.name, std::move(n));
        }
        if (!bundle->nodes.contains(bundle->entry))
            throw std::invalid_argument("Unknown entry node: " + bundle->entry);
        for (const auto &[name, node] : bundle->nodes)
            for (const auto *list : {&node.next, &node.on_error})
                for (const auto &target : *list)
                    if (!bundle->nodes.contains(target))
                        throw std::invalid_argument(name + " references missing node " + target);
        std::set<std::string> reachable;
        std::vector<std::string> pending{bundle->entry};
        while (!pending.empty()) {
            auto name = std::move(pending.back());
            pending.pop_back();
            if (!reachable.insert(name).second)
                continue;
            const auto &node = bundle->nodes.at(name);
            for (const auto *list : {&node.next, &node.on_error})
                for (const auto &child : *list)
                    pending.push_back(child);
        }
        for (const auto &[name, node] : bundle->nodes)
            if (!reachable.contains(name))
                bundle->warnings.push_back("Unreachable node: " + name);
        return std::shared_ptr<const ResourceBundle>(std::move(bundle));
    } catch (const std::exception &e) {
        return failure(ErrorCode::resource_invalid, "resource", e.what());
    }
}
} // namespace ar::runtime
