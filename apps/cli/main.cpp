#include "plugin/PluginLoader.h"
#include "runtime/Tasker.h"
#include "runtime/Replay.h"
#include "vision/Matcher.h"
#include "device/DeviceController.h"
#include "log/ConsoleLogger.h"
#include "core/Json.h"
#include "examples/Demo.h"
#include <iostream>
#include <map>
#include "Capture.h"
namespace {
std::stop_source stop_source;
BOOL WINAPI interrupted(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        stop_source.request_stop();
        return TRUE;
    }
    return FALSE;
}
std::filesystem::path executable_directory() {
    std::wstring path(32768, L'\0');
    const auto n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!n || n == path.size())
        throw std::runtime_error("Executable path unavailable");
    path.resize(n);
    return std::filesystem::path(path).parent_path();
}
std::filesystem::path path(const std::string &text) {
    return ar::core::utf8_to_wide(text);
}
int error(const ar::core::Error &value) {
    std::cerr << ar::core::error_name(value.code) << ": " << value.operation << ": " << value.message << "\n";
    return value.code == ar::core::ErrorCode::cancelled ? 130 : 2;
}
const char *device_schema =
    R"({"type":"object","additionalProperties":false,"required":["backend","device_id"],"properties":{"backend":{"enum":["adb","mumu"]},"device_id":{"type":"string","minLength":1},"serial":{"type":"string","minLength":1},"adb_path":{"type":"string"},"adb_port":{"type":"integer","minimum":1,"maximum":65535},"mumu_path":{"type":"string"},"mumu_index":{"type":"integer","minimum":0},"command_timeout_ms":{"type":"integer","minimum":1,"maximum":3600000},"rotation":{"enum":[0,90,180,270]},"viewport":{"type":"array","minItems":4,"maxItems":4,"items":{"type":"integer","minimum":0,"maximum":16384}}}})";
void usage() {
    std::cout << "OpenAR: generic Android UI automation\n"
                 "  list | doctor [--plugins DIR]\n"
                 "  make-demo --output DIR\n"
                 "  schemas --output DIR\n"
                 "  validate --resource DIR [--models DIR]\n"
                 "  recognize --image FILE --template FILE [--backend ncc-cpu] [--threshold 0.9]\n"
                 "  capture --device FILE --language ja|zh-Hant --output DIR [--interval-ms 3000] [--count 300]\n"
                 "  ocr | ocr-crop | ocr-numbers --ocr-profile FILE --image FILE [--backend paddle-ocr-cpu]\n"
                 "  replay --resource DIR --replay FILE [--entry NODE] [--trace DIR]\n"
                 "  run --resource DIR --device FILE [--models DIR] [--entry NODE] [--trace DIR]\n"
                 "Options: --plugins DIR, --timeout-ms N. Ctrl+C cancels a running task.\n";
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCtrlHandler(interrupted, TRUE);
    try {
        if (argc < 2) {
            usage();
            return 0;
        }
        const auto command = ar::core::wide_to_utf8(argv[1]);
        std::map<std::string, std::string> args;
        const std::set<std::string> known{"plugins",  "output",  "resource",  "models", "image",
                                          "template", "backend", "threshold", "replay", "entry",
                                          "trace",    "device",  "timeout-ms", "language", "interval-ms", "count", "ocr-profile"};
        for (int i = 2; i < argc; ++i) {
            auto key = ar::core::wide_to_utf8(argv[i]);
            if (!key.starts_with("--") || !known.contains(key.substr(2)) || i + 1 >= argc)
                throw std::invalid_argument("Unknown option or missing value: " + key);
            if (!args.emplace(key.substr(2), ar::core::wide_to_utf8(argv[++i])).second)
                throw std::invalid_argument("Duplicate option: " + key);
        }
        auto get = [&](const char *key, std::string fallback = {}) {
            auto it = args.find(key);
            return it == args.end() ? fallback : it->second;
        };
        auto required = [&](const char *key) {
            auto value = get(key);
            if (value.empty())
                throw std::invalid_argument(std::string("Missing --") + key);
            return path(value);
        };
        const auto exe = executable_directory();
        ar::log::ConsoleLogger logger;
        if (command == "make-demo") {
            auto result = ar::examples::make_demo(required("output"));
            if (!result)
                return error(result.error());
            std::cout << "Demo generated\n";
            return 0;
        }
        if (command == "schemas") {
            auto out = required("output");
            std::filesystem::create_directories(out);
            for (auto item : std::vector<std::pair<const wchar_t *, const char *>>{
                     {L"manifest.schema.json", ar::runtime::manifest_schema()},
                     {L"pipeline.schema.json", ar::runtime::pipeline_schema()},
                     {L"replay.schema.json", ar::runtime::replay_schema()},
                     {L"device.schema.json", device_schema}}) {
                std::ofstream file(out / item.first);
                if (!(file << item.second))
                    throw std::runtime_error("Cannot write schema");
            }
            return 0;
        }
        const auto names = ar::plugin::load_plugins(
            path(get("plugins", ar::core::wide_to_utf8((exe / L"plugins").wstring()))), &logger);
        ar::plugin::freeze_registries();
        if (command == "list" || command == "doctor") {
            auto show = [](const char *family, const auto &names) {
                std::cout << family << ":";
                for (auto name : names)
                    std::cout << " " << name;
                std::cout << "\n";
            };
            show("matcher", ar::vision::matcher_backends());
            show("ocr", ar::vision::ocr_backends());
            show("controller", ar::control::controller_backends());
            show("device", ar::device::device_controller_backends());
            if (command == "doctor") {
                std::cout << "plugins=" << names.size() << " adb="
                          << (std::filesystem::is_regular_file(exe / L"adb.exe") ? "available" : "missing")
                          << "\n";
                if (names.empty())
                    return 2;
            }
            return 0;
        }
        if (command == "recognize") {
            auto image = ar::core::read_image(required("image")),
                 templ = ar::core::read_image(required("template"));
            if (!image)
                return error(image.error());
            if (!templ)
                return error(templ.error());
            auto matcher = ar::vision::create_matcher(get("backend", "ncc-cpu"));
            if (!matcher)
                throw std::runtime_error("Matcher unavailable");
            const auto value = get("threshold", "0.9");
            size_t consumed = 0;
            auto threshold = std::stof(value, &consumed);
            if (consumed != value.size())
                throw std::invalid_argument("Invalid threshold");
            auto found = matcher->find_checked(image.value(), templ.value(), threshold);
            if (!found)
                return error(found.error());
            std::cout << "found=" << found.value().found << " score=" << found.value().score
                      << " center=" << found.value().center.x << "," << found.value().center.y << "\n";
            return found.value().found ? 0 : 1;
        }
        if (command == "ocr" || command == "ocr-crop" || command == "ocr-numbers") {
            ar::vision::OcrParams params;
            params.profile = ar::core::wide_to_utf8(required("ocr-profile").wstring());
            auto image = ar::core::read_image(required("image"));
            if (!image) return error(image.error());
            auto engine = ar::vision::create_ocr(get("backend", "paddle-ocr-cpu"), params, &logger);
            if (!engine) throw std::runtime_error("OCR profile initialization failed");
            auto result = command == "ocr" ? engine->recognize_checked(image.value()) :
                engine->recognize_crop_checked(image.value(), nullptr, command == "ocr-numbers");
            if (!result) return error(result.error());
            for (const auto &item : result.value()) {
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> json(buffer);
                json.StartObject();
                json.Key("text"); json.String(item.text.c_str());
                json.Key("score"); json.Double(item.score);
                json.Key("box"); json.StartArray(); json.Int(item.box.x); json.Int(item.box.y);
                json.Int(item.box.width); json.Int(item.box.height); json.EndArray(); json.EndObject();
                std::cout << buffer.GetString() << "\n";
            }
            return 0;
        }
        if (command == "capture") {
            const auto file = required("device");
            auto parsed = ar::core::read_json(file, device_schema);
            if (!parsed)
                return error(parsed.error());
            const auto &doc = parsed.value();
            if (std::string_view(doc["backend"].GetString()) != "mumu" ||
                !doc.HasMember("mumu_path") || !doc.HasMember("mumu_index"))
                throw std::invalid_argument("Capture requires a configured native MuMu device");
            auto relative = [&](const char *key) {
                auto p = path(doc[key].GetString());
                return p.is_absolute() ? p : std::filesystem::absolute(file).parent_path() / p;
            };
            ar::control::ControllerConfig config;
            config.logger = &logger;
            config.mumu_path = relative("mumu_path");
            config.mumu_index = doc["mumu_index"].GetInt();
            config.adb_path = doc.HasMember("adb_path") ? relative("adb_path") : exe / L"adb.exe";
            if (doc.HasMember("serial")) config.serial = doc["serial"].GetString();
            if (doc.HasMember("adb_port")) config.adb_port = static_cast<uint16_t>(doc["adb_port"].GetUint());
            if (doc.HasMember("command_timeout_ms")) config.command_timeout_ms = doc["command_timeout_ms"].GetUint();
            ar::runtime::SessionConfig session_config;
            session_config.device_id = doc["device_id"].GetString();
            if (doc.HasMember("rotation")) session_config.rotation = doc["rotation"].GetInt();
            if (doc.HasMember("viewport")) {
                const auto &v = doc["viewport"];
                session_config.viewport = {v[0].GetInt(), v[1].GetInt(), v[2].GetInt(), v[3].GetInt()};
            }
            auto number = [&](const char *key, const char *fallback) {
                auto value = get(key, fallback);
                if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
                    throw std::invalid_argument(std::string("Invalid --") + key);
                const auto n = std::stoull(value);
                if (n > UINT32_MAX) throw std::invalid_argument(std::string("Invalid --") + key);
                return static_cast<uint32_t>(n);
            };
            ar::cli::CaptureOptions options{required("output"), get("language"),
                number("interval-ms", "3000"), number("count", "300"),
                number("timeout-ms", "5000")};
            options.max_transient_frames = 60; // Allow up to 3 min of portrait startup at the default cadence.
            auto controller = ar::control::create_controller("mumu", config);
            if (!controller) throw std::runtime_error("MuMu controller initialization failed");
            const auto report = ar::cli::capture_only(controller, session_config, options, stop_source.get_token());
            std::cout << "saved=" << report.saved << " directory="
                      << ar::core::wide_to_utf8(report.directory.wstring()) << "\n";
            return report.cancelled ? 130 : 0;
        }
        if (command != "validate" && command != "run" && command != "replay") {
            usage();
            return 2;
        }
        auto loaded = ar::runtime::ResourceBundle::load(required("resource"), path(get("models")));
        if (!loaded)
            return error(loaded.error());
        auto resource = loaded.value();
        for (const auto &warning : resource->warnings)
            std::cerr << "warning: " << warning << "\n";
        if (command == "validate") {
            std::cout << resource->name << "@" << resource->version << " nodes=" << resource->nodes.size()
                      << " templates=" << resource->templates.size() << " valid\n";
            return 0;
        }
        std::shared_ptr<ar::control::IController> controller;
        std::shared_ptr<ar::runtime::IClock> clock;
        ar::runtime::SessionConfig session_config;
        session_config.reference_size = resource->reference_size;
        std::shared_ptr<ar::runtime::ReplayController> replay;
        if (command == "replay") {
            clock = std::make_shared<ar::runtime::ManualClock>();
            auto result = ar::runtime::ReplayController::load(required("replay"), clock);
            if (!result)
                return error(result.error());
            controller = replay = result.value();
            session_config.device_id = "replay";
        } else {
            const auto file = required("device");
            auto parsed = ar::core::read_json(file, device_schema);
            if (!parsed)
                return error(parsed.error());
            const auto &doc = parsed.value();
            ar::control::ControllerConfig config;
            config.logger = &logger;
            config.adb_path = exe / L"adb.exe";
            auto relative = [&](const char *key) {
                auto p = path(doc[key].GetString());
                return p.is_absolute() ? p : std::filesystem::absolute(file).parent_path() / p;
            };
            if (doc.HasMember("adb_path"))
                config.adb_path = relative("adb_path");
            if (doc.HasMember("serial"))
                config.serial = doc["serial"].GetString();
            if (doc.HasMember("adb_port"))
                config.adb_port = static_cast<uint16_t>(doc["adb_port"].GetUint());
            if (doc.HasMember("mumu_path"))
                config.mumu_path = relative("mumu_path");
            if (doc.HasMember("mumu_index"))
                config.mumu_index = doc["mumu_index"].GetInt();
            if (doc.HasMember("command_timeout_ms"))
                config.command_timeout_ms = doc["command_timeout_ms"].GetUint();
            session_config.device_id = doc["device_id"].GetString();
            if (doc.HasMember("rotation"))
                session_config.rotation = doc["rotation"].GetInt();
            if (doc.HasMember("viewport")) {
                const auto &v = doc["viewport"];
                session_config.viewport = {v[0].GetInt(), v[1].GetInt(), v[2].GetInt(), v[3].GetInt()};
            }
            controller = ar::control::create_controller(doc["backend"].GetString(), config);
            if (!controller)
                throw std::runtime_error("Controller initialization failed");
            clock = std::make_shared<ar::runtime::SteadyClock>();
        }
        ar::runtime::DeviceSession session(controller, session_config, clock);
        ar::runtime::TaskOptions options;
        options.trace_directory = path(get("trace", ar::core::wide_to_utf8((exe / L"traces").wstring())));
        if (args.contains("timeout-ms")) {
            size_t consumed = 0;
            const auto value = get("timeout-ms");
            auto timeout = std::stoul(value, &consumed);
            if (consumed != value.size() || timeout == 0 || timeout > 86400000)
                throw std::invalid_argument("Invalid timeout-ms");
            options.limits = resource->limits;
            options.limits->timeout_ms = static_cast<uint32_t>(timeout);
        }
        auto report = ar::runtime::TaskRunner::run(session, *resource, get("entry"), {},
                                                   stop_source.get_token(), options);
        std::cout << "run=" << report.run_id << " status="
                  << (report.status == ar::runtime::RunStatus::completed   ? "completed"
                      : report.status == ar::runtime::RunStatus::cancelled ? "cancelled"
                                                                           : "failed")
                  << " frames=" << report.frames << " transitions=" << report.transitions
                  << " recoveries=" << report.recoveries << " elapsed_ms=" << report.elapsed_ms
                  << " trace=" << ar::core::wide_to_utf8(report.trace_directory.wstring()) << "\n";
        if (report.diagnostic_error)
            std::cerr << "trace error: " << report.diagnostic_error->message << "\n";
        if (report.error)
            return error(*report.error);
        if (replay && !replay->complete())
            return error(ar::core::failure(ar::core::ErrorCode::backend_failed, "replay",
                                           "Task ended before all expected actions were consumed"));
        return report.diagnostic_error ? 2 : 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 2;
    }
}
