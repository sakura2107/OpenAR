#include "core/Process.h"
#include "core/Json.h"
#include "runtime/Tasker.h"
#include "runtime/Replay.h"
#include "plugin/PluginLoader.h"
#include "vision/Matcher.h"
#include "examples/Demo.h"
#include "../plugins/ocr/paddle_common/Dictionary.h"
#include "../plugins/ocr/paddle_common/OcrConfig.h"
#include "CaptureTests.h"
#include <iostream>
#include <functional>
#include <io.h>
#include <fcntl.h>
using namespace ar::runtime;
using ar::core::ErrorCode;
int device_integration(const std::filesystem::path &, const std::filesystem::path &,
                       const std::filesystem::path &, int, const std::string &);
void http_contract_tests();
void host_contract_tests(const std::filesystem::path&,const std::function<void(const char*,const std::function<void()>&)>&);
int multi_device_test(const std::filesystem::path &, const std::filesystem::path &);
int soak_test(const std::filesystem::path &, const std::filesystem::path &, int);
void loop_contract_tests(const std::filesystem::path &,const std::filesystem::path &,
                        const std::function<void(const char*,const std::function<void()>&)>&);
namespace {
int passed = 0, failed = 0;
std::filesystem::path exe, root, temp;
#define CHECK(EXPRESSION)                                                                                    \
    do {                                                                                                     \
        if (!(EXPRESSION))                                                                                   \
            throw std::runtime_error(std::string(#EXPRESSION) + " at line " + std::to_string(__LINE__));     \
    } while (false)
void test(const char *name, const std::function<void()> &function) {
    try {
        function();
        ++passed;
        std::cout << "PASS " << name << "\n";
    } catch (const std::exception &e) {
        ++failed;
        std::cout << "FAIL " << name << ": " << e.what() << "\n";
    }
}
void write(const std::filesystem::path &file, const std::string &data) {
    std::ofstream out(file, std::ios::binary);
    CHECK(bool(out << data));
}
RunReport replay_run(const ResourceBundle &bundle, std::vector<ReplayStep> steps,
                     std::shared_ptr<ReplayController> *out = nullptr, const Extensions &extensions = {},
                     const TaskOptions &options = {}) {
    auto clock = std::make_shared<ManualClock>();
    auto replay = std::make_shared<ReplayController>(std::move(steps), clock);
    DeviceSession session(replay, {"fake", bundle.reference_size}, clock);
    if (out)
        *out = replay;
    return TaskRunner::run(session, bundle, {}, extensions, {}, options);
}
ResourceBundle direct_bundle() {
    ResourceBundle bundle;
    bundle.name = "test";
    bundle.version = "1";
    bundle.entry = "entry";
    bundle.reference_size = {640, 360};
    Node node;
    node.name = "entry";
    bundle.nodes.emplace(node.name, node);
    bundle.limits = {1000, 50, 10, 2};
    return bundle;
}
class SlowController final : public ar::control::IController {
  public:
    explicit SlowController(std::shared_ptr<IClock> clock) : clock(std::move(clock)) {}
    std::string_view name() const override { return "slow"; }
    bool connect() override {
        clock->sleep_ms(100, {});
        return true;
    }
    bool disconnect() override { return true; }
    cv::Mat screenshot() override {
        ++captures;
        clock->sleep_ms(100, {});
        return cv::Mat(360, 640, CV_8UC3, cv::Scalar(0));
    }
    bool click(cv::Point) override { return true; }
    bool long_press(cv::Point, int32_t) override { return true; }
    bool swipe(cv::Point, cv::Point, int32_t) override { return true; }
    bool press_key(int32_t) override { return true; }
    int captures = 0;
    std::shared_ptr<IClock> clock;
    bool supports_context() const override { return true; }
};
class Log final : public ar::log::ILogger {
  public:
    std::string messages;
    bool enabled(ar::log::Level) const noexcept override { return true; }
    void log(ar::log::Level, std::string_view message) noexcept override {
        try {
            messages += message;
            messages += '\n';
        } catch (...) {
        }
    }
};
int child(int argc, wchar_t **argv) {
    _setmode(_fileno(stdout), _O_BINARY);
    const std::wstring mode = argv[2];
    if (mode == L"hang") {
        Sleep(10000);
        return 0;
    }
    if (mode == L"streams") {
        _setmode(_fileno(stdout), _O_BINARY);
        const unsigned char bytes[]{0, 1, 2, 255};
        fwrite(bytes, 1, 4, stdout);
        fflush(stdout);
        fputs("diagnostic", stderr);
        return 7;
    }
    if (mode == L"flood") {
        std::string bytes(4096, 'x');
        for (;;) {
            fwrite(bytes.data(), 1, bytes.size(), stdout);
            fflush(stdout);
        }
    }
    if (mode == L"args") {
        for (int i = 3; i < argc; ++i)
            std::cout << ar::core::wide_to_utf8(argv[i]) << "\n";
        return 0;
    }
    if (mode == L"grandchild") {
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        std::wstring command = L"\"" + exe.wstring() + L"\" --child hang";
        if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                            nullptr, &startup, &process))
            return 2;
        std::cout << process.dwProcessId << std::endl;
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        Sleep(10000);
        return 0;
    }
    return 2;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    SetConsoleOutputCP(CP_UTF8);
    std::wstring buffer(32768, L'\0');
    buffer.resize(GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size())));
    exe = buffer;
    if (argc > 2 && std::wstring_view(argv[1]) == L"--child")
        return child(argc, argv);
    if (argc == 6 && std::wstring_view(argv[1]) == L"--integration")
        return device_integration(exe, std::filesystem::absolute(argv[2]), std::filesystem::absolute(argv[3]),
                                  std::stoi(argv[4]), ar::core::wide_to_utf8(argv[5]));
    if (argc == 3 && std::wstring_view(argv[1]) == L"--multi-device")
        return multi_device_test(exe, std::filesystem::absolute(argv[2]));
    if (argc == 4 && std::wstring_view(argv[1]) == L"--soak")
        return soak_test(exe, std::filesystem::absolute(argv[3]), std::stoi(argv[2]));
    root = argc > 1 ? std::filesystem::absolute(argv[1])
                    : exe.parent_path().parent_path().parent_path().parent_path();
    temp =
        exe.parent_path() / L"test-artifacts" / ar::core::utf8_to_wide(std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(temp);
    Log log;
    ar::plugin::load_plugins(exe.parent_path() / L"plugins", &log);
    auto fixtures = ar::plugin::load_plugins(exe.parent_path() / L"fixtures", &log);
    auto duplicate = ar::plugin::load_plugins(exe.parent_path() / L"fixtures", &log);
    ar::plugin::freeze_registries();
    loop_contract_tests(root,temp / L"loop",test);
    host_contract_tests(temp / L"host",test);
    test("capture writes native frames at reference size without input", [&] {
        auto controller = std::make_shared<capture_tests::Controller>();
        ar::cli::CaptureOptions options{temp / L"capture-测试", "ja", 100, 2, 1000};
        auto report = ar::cli::capture_only(controller, {"capture-test", {1280, 720}}, options);
        CHECK(report.saved == 2 && !report.cancelled);
        CHECK(controller->captures == 2 && controller->inputs == 0 && controller->connects == 1);
        CHECK(controller->disconnects == 1);
        auto image = ar::core::read_image(report.directory / L"images/000001.png");
        CHECK(image && image.value().size() == cv::Size(1280, 720));
        CHECK(cv::norm(image.value().at<cv::Vec3b>(0, 0), cv::Vec3b(17, 91, 203)) == 0);
        std::ifstream file(report.directory / L"capture.jsonl");
        std::string line; int lines = 0;
        while (std::getline(file, line)) {
            rapidjson::Document doc; doc.Parse(line.c_str());
            CHECK(!doc.HasParseError() && doc["size"][0].GetInt() == 1280);
            CHECK(std::string(doc["source"].GetString()) == "OpenAR/MuMu/native");
            CHECK(doc["source_size"][0].GetInt() == 1920);
            ++lines;
        }
        CHECK(lines == 2);
        options.count = 1;
        auto second = ar::cli::capture_only(controller, {"capture-test", {1280, 720}}, options);
        CHECK(second.directory != report.directory);
        CHECK(std::filesystem::is_regular_file(report.directory / L"images/000002.png"));
    });
    test("capture rejects invalid options and aspect without input", [&] {
        auto controller = std::make_shared<capture_tests::Controller>();
        ar::cli::CaptureOptions options{temp / L"capture-invalid", "ja", 100, 0, 1000};
        bool rejected = false;
        try { ar::cli::capture_only(controller, {"capture-invalid", {1280, 720}}, options); }
        catch (const std::invalid_argument &) { rejected = true; }
        CHECK(rejected && controller->captures == 0 && controller->connects == 0);
        options.count = 1;
        controller->frame = cv::Mat(800, 1200, CV_8UC3, cv::Scalar(0));
        rejected = false;
        try { ar::cli::capture_only(controller, {"capture-invalid", {1280, 720}}, options); }
        catch (const std::runtime_error &) { rejected = true; }
        CHECK(rejected && controller->inputs == 0);
    });
    test("capture cancellation interrupts interval and preserves saved files", [&] {
        auto controller = std::make_shared<capture_tests::Controller>();
        std::stop_source stop;
        ar::cli::CaptureOptions options{temp / L"capture-cancel", "zh-Hant", 600000, 2, 1000};
        const auto started = std::chrono::steady_clock::now();
        auto task = std::async(std::launch::async, [&] {
            return ar::cli::capture_only(controller, {"capture-cancel", {1280, 720}}, options, stop.get_token());
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        stop.request_stop();
        auto report = task.get();
        CHECK(report.cancelled && report.saved <= 1 && controller->inputs == 0);
        CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds(2));
    });
    test("HTTP validates response completeness and byte limit", http_contract_tests);
    test("capture skips bounded portrait startup without input", [&] {
        auto controller = std::make_shared<capture_tests::Controller>();
        controller->on_capture = [&] {
            controller->frame = controller->captures == 1 ? cv::Mat(1280, 720, CV_8UC3, cv::Scalar(0)) :
                                                           cv::Mat(720, 1280, CV_8UC3, cv::Scalar(0));
        };
        ar::cli::CaptureOptions options{temp / L"capture-transient", "ja", 100, 1, 1000};
        options.max_transient_frames = 1;
        const auto report = ar::cli::capture_only(controller, {"capture-transient", {1280, 720}}, options);
        CHECK(report.saved == 1 && report.skipped == 1 && controller->captures == 2 && controller->inputs == 0);
    });
    test("process deadline includes idle pipe", [] {
        ar::core::ProcessOptions options;
        options.timeout = std::chrono::milliseconds(150);
        auto r = ar::core::execute_process(exe, {"--child", "hang"}, options);
        CHECK(r.state == ar::core::ProcessState::timed_out);
        CHECK(r.elapsed.count() < 1000);
    });
    test("process exit code and binary streams", [] {
        auto r = ar::core::execute_process(exe, {"--child", "streams"});
        CHECK(r.state == ar::core::ProcessState::exited);
        CHECK(r.exit_code == 7);
        CHECK(!r.success());
        CHECK(r.stdout_bytes == std::vector<uint8_t>({0, 1, 2, 255}));
        CHECK(r.stderr_text == "diagnostic");
    });
    test("process output cap", [] {
        ar::core::ProcessOptions o;
        o.max_output_bytes = 8192;
        auto r = ar::core::execute_process(exe, {"--child", "flood"}, o);
        CHECK(r.state == ar::core::ProcessState::output_limit);
        CHECK(r.stdout_bytes.size() <= 8192);
        CHECK(r.elapsed.count() < 1000);
    });
    test("process cancellation", [] {
        std::stop_source stop;
        std::jthread timer([&] {
            Sleep(80);
            stop.request_stop();
        });
        ar::core::ProcessOptions o;
        o.stop = stop.get_token();
        auto r = ar::core::execute_process(exe, {"--child", "hang"}, o);
        CHECK(r.state == ar::core::ProcessState::cancelled);
        CHECK(r.elapsed.count() < 1000);
    });
    test("process quoting and Unicode", [] {
        std::vector<std::string> args{"space argument", "quote\"here", "C:\\folder space\\", "中文路径", ""};
        auto all = args;
        all.insert(all.begin(), {"--child", "args"});
        auto r = ar::core::execute_process(exe, all);
        std::string expected;
        for (auto &arg : args)
            expected += arg + '\n';
        CHECK(r.success());
        CHECK(r.stdout_text() == expected);
    });
    test("process child tree reaped", [] {
        ar::core::ProcessOptions o;
        o.timeout = std::chrono::milliseconds(150);
        auto r = ar::core::execute_process(exe, {"--child", "grandchild"}, o);
        CHECK(r.state == ar::core::ProcessState::timed_out);
        auto id = std::stoul(r.stdout_text());
        ar::core::UniqueHandle child(OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(id)));
        CHECK(!child.get() || WaitForSingleObject(child.get(), 1000) == WAIT_OBJECT_0);
    });
    test("image hash ignores ROI padding and includes geometry", [] {
        cv::Mat image(16, 20, CV_8UC3);
        cv::randu(image, 0, 255);
        auto roi = image(cv::Rect(5, 8, 7, 8));
        auto clone = roi.clone();
        CHECK(ar::core::image_hash(roi) == ar::core::image_hash(clone));
        auto flat = clone.reshape(3, 7);
        CHECK(ar::core::image_hash(flat) != ar::core::image_hash(clone));
        CHECK(!ar::core::valid_roi({INT_MAX, 0, INT_MAX, 1}, image.size()));
    });
    test("plugins reject incompatible and incomplete families", [&] {
        CHECK(log.messages.find("incompatible") != std::string::npos);
        CHECK(log.messages.find("missing exports") != std::string::npos);
        CHECK(duplicate.empty());
        CHECK(fixtures.size() == 4);
        CHECK(!ar::vision::create_matcher("fixture-null"));
        CHECK(!ar::vision::create_matcher("fixture-missing"));
        CHECK(ar::plugin::load_plugins(exe.parent_path() / L"fixtures", &log).empty());
    });
    test("malformed matcher result rejected", [] {
        auto matcher = ar::vision::create_matcher("fixture-bad-result");
        CHECK(matcher);
        cv::Mat image(32, 32, CV_8UC3);
        auto r = matcher->find_checked(image, image(cv::Rect(0, 0, 8, 8)), 0.5f);
        CHECK(!r && r.error().code == ErrorCode::backend_failed);
    });
    test("OCR v1 growing result rejected", [] {
        auto ocr = ar::vision::create_ocr("fixture-growing-ocr", {});
        CHECK(ocr);
        auto r = ocr->recognize_checked(cv::Mat(32, 32, CV_8UC3));
        CHECK(!r);
    });
    test("OCR v2 once, UTF-8, configuration and release", [] {
        auto module = GetModuleHandleW(L"FixtureOcrOnce.dll");
        CHECK(module);
        auto runs = reinterpret_cast<int32_t (*)()>(GetProcAddress(module, "fixture_runs"));
        auto alive = reinterpret_cast<int32_t (*)()>(GetProcAddress(module, "fixture_alive"));
        auto merge = reinterpret_cast<float (*)()>(GetProcAddress(module, "fixture_merge"));
        ar::vision::OcrParams p;
        p.det_line_merge_ratio = 0.25f;
        auto ocr = ar::vision::create_ocr("fixture-ocr-once", p);
        CHECK(ocr);
        const auto before = runs();
        cv::Rect roi(8, 9, 20, 20);
        auto r = ocr->recognize_checked(cv::Mat(40, 40, CV_8UC3), &roi);
        CHECK(r && r.value().size() == 1);
        CHECK(r.value()[0].text.size() == 2400);
        CHECK(r.value()[0].box.tl() == roi.tl());
        CHECK(runs() == before + 1 && alive() == 0 && merge() == 0.25f);
    });
    test("OCR dictionary accepts UTF-8 BOM CRLF and preserves class order", [] {
        auto file = temp / "utf8-dictionary.txt";
        write(file, "\xEF\xBB\xBF"
                    "疗\r\n7\r\n \r\n");
        ar::paddle::Dictionary dictionary;
        CHECK(dictionary.load(ar::core::wide_to_utf8(file.wstring())));
        CHECK(dictionary.size() == 3 && dictionary.get_char(0) == "疗" && dictionary.get_char(1) == "7" &&
              dictionary.get_char(2) == " ");
        bool rejected = false;
        try { dictionary.get_char(3); } catch (const std::out_of_range &) { rejected = true; }
        CHECK(rejected);
    });
    test("OCR dictionary converts legacy GBK and replaces on reload", [] {
        auto file = temp / "gbk-dictionary.txt";
        write(file, "\xC1\xC6\n7\n \n");
        ar::paddle::Dictionary dictionary;
        CHECK(dictionary.load(ar::core::wide_to_utf8(file.wstring())));
        CHECK(dictionary.size() == 3 && dictionary.get_char(0) == "疗");
        write(file, "2\n3\n");
        CHECK(dictionary.load(ar::core::wide_to_utf8(file.wstring())));
        CHECK(dictionary.size() == 2 && dictionary.get_char(0) == "2");
        write(file, "2\n\n3\n");
        CHECK(!dictionary.load(ar::core::wide_to_utf8(file.wstring())));
        CHECK(dictionary.size() == 2 && dictionary.get_char(1) == "3");
        write(file, "\xFF\n");
        CHECK(!dictionary.load(ar::core::wide_to_utf8(file.wstring())));
    });
    test("OCR native profile reads real numeric crop and preserves ROI", [&] {
        ar::vision::OcrParams params;
        params.profile = ar::core::wide_to_utf8((root / L"models/ocr/ja-v3.json").wstring());
        auto engine = ar::vision::create_ocr("paddle-ocr-cpu", params, &log);
        CHECK(engine);
        auto frame = ar::core::read_image(root / L"tests/ocr_corpus/ja/regression/home.png");
        CHECK(frame);
        const cv::Rect roi(697, 23, 106, 25);
        auto result = engine->recognize_crop_checked(frame.value(), &roi, true);
        CHECK(result && result.value().size() == 1 && result.value()[0].text == "789,333,348");
        CHECK(result.value()[0].box == roi);
        CHECK(!engine->recognize_crop_checked(cv::Mat(), nullptr, true));
        auto repeated = engine->recognize_crop_checked(frame.value(), &roi, true);
        CHECK(repeated && repeated.value().size() == 1 && repeated.value()[0].text == "789,333,348");
        const cv::Rect text_roi(74, 677, 48, 20);
        auto text = engine->recognize_crop_checked(frame.value(), &text_roi);
        CHECK(text && text.value().size() == 1 && text.value()[0].text == "カフェ");
    });
    test("OCR numeric syntax routing never invents missing operators", [] {
        CHECK(ar::paddle::numeric_syntax_valid("789,333,348"));
        CHECK(ar::paddle::numeric_syntax_valid("6/6"));
        CHECK(ar::paddle::numeric_syntax_valid("66"));
        CHECK(ar::paddle::numeric_syntax_valid("+15%"));
        CHECK(ar::paddle::numeric_syntax_valid("x1"));
        CHECK(!ar::paddle::numeric_syntax_valid("789.333.348"));
        CHECK(!ar::paddle::numeric_syntax_valid(".63"));
    });
    test("OCR v5 syntax fallback reads a real thousands crop", [&] {
        ar::vision::OcrParams params;
        params.profile = ar::core::wide_to_utf8((root / L"models/ocr/ja-b1b-v3.json").wstring());
        auto engine = ar::vision::create_ocr("paddle-ocr-cpu", params, &log);
        CHECK(engine);
        auto frame = ar::core::read_image(root / L"tests/ocr_corpus/ja/captures/1793533360930-36388-0/images/000029.png");
        CHECK(frame);
        const cv::Rect roi(717, 11, 116, 19);
        auto result = engine->recognize_crop_checked(frame.value(), &roi, true);
        CHECK(result && result.value().size() == 1 && result.value()[0].text == "789,333,348");
        CHECK(result.value()[0].box == roi);
    });
    test("OCR context uses edge pixels and katakana excludes kanji", [] {
        ar::paddle::RecConfig config;
        config.context_horizontal = .25f;
        const cv::Mat crop(20, 30, CV_8UC3, cv::Scalar(13, 42, 87));
        auto padded = ar::paddle::recognition_context(crop, config);
        CHECK(padded.rows == 20 && padded.cols == 40);
        CHECK(padded.at<cv::Vec3b>(0, 0) == crop.at<cv::Vec3b>(0, 0));
        CHECK(crop.cols == 30);
        CHECK(ar::paddle::katakana_token("ミ"));
        CHECK(ar::paddle::katakana_token("ー"));
        CHECK(ar::paddle::katakana_token("（"));
        CHECK(!ar::paddle::katakana_token("三"));
        CHECK(!ar::paddle::katakana_token("へ"));
    });
    test("OCR optional katakana profile reads real Mika ROI", [&] {
        ar::vision::OcrParams params;
        params.profile = ar::core::wide_to_utf8((root / L"models/ocr/ja-katakana-b1c.json").wstring());
        auto engine = ar::vision::create_ocr("paddle-ocr-cpu", params, &log);
        CHECK(engine);
        auto frame = ar::core::read_image(root / L"tests/ocr_corpus/ja/captures/1793533360930-36388-0/images/000229.png");
        CHECK(frame);
        const cv::Rect roi(117, 263, 47, 30);
        auto result = engine->recognize_crop_checked(frame.value(), &roi);
        CHECK(result && result.value().size() == 1 && result.value()[0].text == "ミカ");
        CHECK(result.value()[0].box == roi);
    });
    test("OCR CTC mask excludes simplified classes without changing numeric routing", [&] {
        ar::vision::OcrParams params;
        params.profile=ar::core::wide_to_utf8((root/L"models/ocr/zh-Hant-b1d-v2.json").wstring());
        auto config=ar::paddle::resolve_config(params);
        CHECK(config.rec.blocked_characters.contains("荡"));
        CHECK(config.rec.blocked_characters.contains("装"));
        CHECK(!config.rec.blocked_characters.contains("蕩"));
        CHECK(!config.rec.blocked_characters.contains("裝"));
        CHECK(!config.rec.blocked_characters.contains("群"));
        CHECK(!config.rec.blocked_characters.contains("床"));
        CHECK(config.number_rec.blocked_characters.empty());
        CHECK(config.fallback_rec.blocked_characters.empty());
    });
    test("OCR second numeric fallback preserves old thousands and reads italic x1", [&] {
        ar::vision::OcrParams params;
        params.profile=ar::core::wide_to_utf8((root/L"models/ocr/ja-b1d-v2.json").wstring());
        auto engine=ar::vision::create_ocr("paddle-ocr-cpu",params,&log);CHECK(engine);
        auto old_frame=ar::core::read_image(root/L"tests/ocr_corpus/ja/captures/1793533360930-36388-0/images/000029.png");CHECK(old_frame);
        const cv::Rect old_roi(717,11,116,19);
        auto old=engine->recognize_crop_checked(old_frame.value(),&old_roi,true);
        CHECK(old && old.value().size()==1 && old.value()[0].text=="789,333,348");
        auto frame=ar::core::read_image(root/L"tests/ocr_corpus/acceptance-b1c/ja/1793548374628-23252-0/images/000041.png");CHECK(frame);
        const cv::Rect roi(318,551,33,30);
        auto result=engine->recognize_crop_checked(frame.value(),&roi,true);
        CHECK(result && result.value().size()==1 && ar::paddle::numeric_syntax_valid(result.value()[0].text));
        CHECK(result.value()[0].text=="X1" || result.value()[0].text=="x1");
        CHECK(result.value()[0].box==roi);
    });
    test("OCR native CTC mask reads traditional glyphs directly", [&] {
        ar::vision::OcrParams params;
        params.profile=ar::core::wide_to_utf8((root/L"models/ocr/zh-Hant-b1d-v2.json").wstring());
        auto engine=ar::vision::create_ocr("paddle-ocr-cpu",params,&log);CHECK(engine);
        auto frame=ar::core::read_image(root/L"tests/ocr_corpus/acceptance-b1c/zh-Hant/1793548971320-32168-0/images/000051.png");
        CHECK(frame);
        const cv::Rect roi(282,403,119,37);
        auto result=engine->recognize_crop_checked(frame.value(),&roi);
        CHECK(result && result.value().size()==1 && result.value()[0].text=="星野(泳裝)");
        CHECK(result.value()[0].box==roi);
    });
    test("OCR profiles reject height class and schema errors explicitly", [&] {
        const auto source = root / L"models/ocr/ja-v3.json";
        auto parsed = ar::core::read_json(source);
        CHECK(parsed);
        auto &doc = parsed.value();
        auto &allocator = doc.GetAllocator();
        auto absolute_asset = [&](const char *section, const char *key) {
            auto asset = std::filesystem::weakly_canonical(source.parent_path() / ar::core::utf8_to_wide(doc[section][key].GetString()));
            const auto name = ar::core::wide_to_utf8(asset.wstring());
            doc[section][key].SetString(name.c_str(), allocator);
        };
        absolute_asset("detector", "model"); absolute_asset("recognizer", "model"); absolute_asset("recognizer", "dictionary");
        auto rejects = [&](std::string_view expected) {
            rapidjson::StringBuffer buffer; rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); doc.Accept(writer);
            const auto file = temp / L"invalid-profile.json";
            write(file, buffer.GetString());
            ar::vision::OcrParams params; params.profile = ar::core::wide_to_utf8(file.wstring());
            Log errors;
            CHECK(!ar::vision::create_ocr("paddle-ocr-cpu", params, &errors));
            CHECK(errors.messages.find(expected) != std::string::npos);
        };
        doc["recognizer"]["input_height"].SetInt(32); rejects("height conflicts");
        doc["recognizer"]["input_height"].SetString("model", allocator);
        doc["recognizer"]["use_space_char"].SetBool(false); rejects("dictionary/model mismatch");
        doc["recognizer"]["use_space_char"].SetBool(true);
        doc.AddMember("unexpected", 1, allocator); rejects("Unknown OCR config field"); doc.RemoveMember("unexpected");
        doc.AddMember("runtime", rapidjson::Value("bad", allocator), allocator); rejects("runtime must be an object");
    });
    test("legacy OCR plugins refuse profiles and unsupported crop routes", [&] {
        ar::vision::OcrParams params; params.profile = "must-not-be-ignored.json";
        Log errors;
        CHECK(!ar::vision::create_ocr("fixture-ocr-once", params, &errors));
        CHECK(errors.messages.find("does not support language profiles") != std::string::npos);
        params.profile.clear();
        auto engine = ar::vision::create_ocr("fixture-ocr-once", params);
        CHECK(engine);
        auto result = engine->recognize_crop_checked(cv::Mat(32, 32, CV_8UC3));
        CHECK(!result && result.error().code == ErrorCode::unsupported);
    });
    test("missing OCR models do not create unusable object", [] {
        ar::vision::OcrParams p;
        p.det_model = "missing-det.onnx";
        p.rec_model = "missing-rec.onnx";
        p.dictionary = "missing.txt";
        CHECK(!ar::vision::create_ocr("paddle-ocr-cpu", p));
    });
    const auto demo = temp / L"中文资源包";
    auto created = ar::examples::make_demo(demo);
    if (!created) {
        std::cerr << created.error().message;
        return 2;
    }
    auto loaded = ResourceBundle::load(demo);
    if (!loaded) {
        std::cerr << loaded.error().message;
        return 2;
    }
    auto bundle = loaded.value();
    auto before = ar::core::read_image(demo / L"before.png").value(),
         after = ar::core::read_image(demo / L"after.png").value();
    Action click;
    click.type = ActionType::click;
    click.target = cv::Point(320, 180);
    for (const auto name : ar::vision::matcher_backends())
        if (name == "ncc-cpu" || name == "ssim-cpu" || name == "ncc-cuda" || name == "ssim-cuda")
            test((std::string("matcher valid ROI, cache and input: ") + std::string(name)).c_str(),
                 [&, name] {
                     auto matcher = ar::vision::create_matcher(name);
                     CHECK(matcher);
                     auto templ = before(cv::Rect(260, 156, 120, 48));
                     cv::Rect roi(230, 120, 180, 120);
                     auto r = matcher->find_checked(before, templ, 0.9f, &roi);
                     if (!r)
                         std::cerr << name << " " << r.error().message << std::endl;
                     else if (!r.value().found)
                         std::cerr << name << " score=" << r.value().score << " center=" << r.value().center.x
                                   << "," << r.value().center.y << std::endl;
                     CHECK(r && r.value().found);
                     CHECK(cv::norm(r.value().center - cv::Point(320, 180)) <= 2);
                     auto again = matcher->find_checked(before, templ.clone(), 0.9f, &roi);
                     CHECK(again && again.value().found);
                     CHECK(std::abs(again.value().score - r.value().score) < 0.001f);
                     CHECK(!matcher->find_checked(before, templ, std::numeric_limits<float>::quiet_NaN(),
                                                  nullptr));
                     cv::Rect invalid(-1, 0, 30, 30);
                     CHECK(!matcher->find_checked(before, templ, 0.9f, &invalid));
                     auto oversized = matcher->find_checked(templ, before, 0.9f, nullptr);
                     CHECK(oversized && !oversized.value().found);
                     ar::vision::MatcherConfig config;
                     config.max_input_bytes = 100;
                     auto small = ar::vision::create_matcher(name, config);
                     CHECK(small);
                     CHECK(!small->find_checked(before, templ, 0.9f, nullptr));
                 });
    test("resource rejects references, duplicate keys, traversal and schema", [&] {
        const auto broken = temp / L"broken";
        std::filesystem::create_directories(broken);
        std::filesystem::copy(demo, broken,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
        write(
            broken / L"pipeline.json",
            R"({"entry":"a","nodes":{"a":{"recognition":{"type":"Direct"},"action":{"type":"None"},"next":["missing"]}}})");
        CHECK(!ResourceBundle::load(broken));
        write(broken / L"pipeline.json", R"({"entry":"a","entry":"b","nodes":{}})");
        CHECK(!ResourceBundle::load(broken));
        write(
            broken / L"manifest.json",
            R"({"schema_version":1,"name":"bad","version":"1","reference_size":[640,360],"pipeline":"../中文资源包/pipeline.json","templates":{}})");
        CHECK(!ResourceBundle::load(broken));
        write(
            broken / L"manifest.json",
            R"({"schema_version":1,"name":"bad","version":"1","reference_size":[0,360],"pipeline":"pipeline.json","templates":{}})");
        CHECK(!ResourceBundle::load(broken));
        write(broken / L"deep.json", std::string(10000, '[') + std::string(10000, ']'));
        CHECK(!ar::core::read_json(broken / L"deep.json"));
    });
    test("coordinates scale, viewport, rotation and aspect guard", [] {
        SessionConfig c{"coord", {640, 360}, {80, 45, 1280, 720}};
        auto map = CoordinateMap::create({1440, 810}, c);
        CHECK(map);
        CHECK(map.value().to_device({320, 180}).value() == cv::Point(721, 406));
        c.viewport = {};
        CHECK(!CoordinateMap::create({640, 480}, c));
        c.rotation = 90;
        auto rotated = CoordinateMap::create({360, 640}, c);
        CHECK(rotated);
        CHECK(rotated.value().to_device({0, 0}).value() == cv::Point(0, 639));
    });
    test("connect time consumes screenshot deadline", [] {
        auto clock = std::make_shared<ManualClock>();
        auto controller = std::make_shared<SlowController>(clock);
        DeviceSession session(controller, {"slow", {640, 360}}, clock);
        auto r = session.capture({}, 100);
        CHECK(!r && r.error().code == ErrorCode::timeout);
        CHECK(controller->captures == 0);
    });
    test("fresh frames and device isolation", [&] {
        auto clock = std::make_shared<ManualClock>();
        auto controller =
            std::make_shared<ReplayController>(std::vector<ReplayStep>{{before, click}, {after, {}}}, clock);
        DeviceSession session(controller, {"one", {640, 360}}, clock);
        auto frame = session.capture({}, 1000);
        CHECK(frame);
        Frame other = frame.value();
        other.device_id = "two";
        CHECK(!session.validate(other));
        CHECK(session.perform(click, &frame.value(), {}, {}, 1000));
        CHECK(!session.validate(frame.value()));
        CHECK(!session.perform(click, &frame.value(), {}, {}, 1000));
        CHECK(controller->actions().size() == 1);
        CHECK(session.capture({}, 1000));
        auto second_controller =
            std::make_shared<ReplayController>(std::vector<ReplayStep>{{before, {}}}, clock);
        DeviceSession same_id(second_controller, {"one", {640, 360}}, clock);
        auto second_frame = same_id.capture({}, 1000);
        CHECK(second_frame && !session.validate(second_frame.value()));
        auto lease = session.acquire_run();
        CHECK(lease && !same_id.acquire_run());
        Action wait;
        wait.type = ActionType::wait;
        wait.duration_ms = 10;
        CHECK(same_id.perform(wait, &second_frame.value(), {}, {}, 1000));
        CHECK(!same_id.validate(second_frame.value()));
    });
    test("replay verifies action then confirms destination", [&] {
        std::shared_ptr<ReplayController> controller;
        auto report = replay_run(*bundle, {{before, click, 0, 20}, {after, {}}}, &controller);
        CHECK(report.status == RunStatus::completed);
        CHECK(report.frames == 2 && report.transitions == 2);
        CHECK(controller->actions().size() == 1 && controller->complete());
        CHECK(report.elapsed_ms == 20);
    });
    test("wrong click fails and preserves evidence", [&] {
        ResourceBundle modified = *bundle;
        modified.nodes["Open"].action.target = cv::Point(10, 10);
        modified.nodes["Open"].on_error.clear();
        TaskOptions options;
        options.trace_directory = temp / L"traces";
        auto report = replay_run(modified, {{before, click}, {after, {}}}, nullptr, {}, options);
        CHECK(report.status == RunStatus::failed);
        CHECK(std::filesystem::is_regular_file(report.trace_directory / L"events.jsonl"));
        CHECK(std::filesystem::is_regular_file(report.trace_directory / L"failure.png"));
    });
    test("uncertain click recovered by recognition without repeat", [&] {
        ReplayStep step{before, click};
        step.action_error = ar::core::failure(ErrorCode::offline, "fake", "uncertain");
        std::shared_ptr<ReplayController> controller;
        auto report = replay_run(*bundle, {step, {after, {}}}, &controller);
        CHECK(report.status == RunStatus::completed && report.recoveries == 1);
        CHECK(controller->actions().size() == 1);
    });
    test("capture error routes bounded recovery", [&] {
        ResourceBundle modified = *bundle;
        modified.nodes["Open"].on_error = {"Open"};
        ReplayStep step{before, click};
        step.capture_error = ar::core::failure(ErrorCode::offline, "fake", "offline");
        auto report = replay_run(modified, {step, {after, {}}});
        CHECK(report.status == RunStatus::completed && report.recoveries == 1);
    });
    test("no match timeout and bounded cyclic graph", [&] {
        auto no_recovery = *bundle;
        no_recovery.nodes["Open"].on_error.clear();
        auto timeout = replay_run(no_recovery, {{after, {}}});
        CHECK(timeout.status == RunStatus::failed && timeout.error->code == ErrorCode::timeout &&
              timeout.elapsed_ms == 1000);
        auto cycle = direct_bundle();
        cycle.nodes["entry"].next = {"entry"};
        cycle.nodes["entry"].max_visits = 3;
        auto report = replay_run(cycle, {{before, {}}});
        CHECK(report.error && report.error->code == ErrorCode::budget_exceeded);
        CHECK(report.transitions == 3);
    });
    test("custom recognition and recovery budget", [&] {
        auto custom = direct_bundle();
        custom.nodes["entry"].recognition.type = RecognitionType::custom;
        custom.nodes["entry"].recognition.custom = "throws";
        custom.nodes["entry"].on_error = {"entry"};
        Extensions extensions;
        extensions.recognitions["throws"] = [](auto &) -> ar::core::Result<RecognitionMatch> {
            return ar::core::failure(ErrorCode::backend_failed, "custom", "failure");
        };
        auto report = replay_run(custom, {{before, {}}}, nullptr, extensions);
        CHECK(report.status == RunStatus::failed && report.recoveries == 2);
    });
    test("async cancellation, conflict and Tasker lifetime", [&] {
        auto async_bundle = std::make_shared<ResourceBundle>(direct_bundle());
        async_bundle->nodes["entry"].action.type = ActionType::wait;
        async_bundle->nodes["entry"].action.duration_ms = 300;
        auto clock = std::make_shared<SteadyClock>();
        auto controller = std::make_shared<ReplayController>(std::vector<ReplayStep>{{before, {}}}, clock);
        auto session = std::make_shared<DeviceSession>(controller, SessionConfig{"async", {640, 360}}, clock);
        std::shared_ptr<TaskJob> job;
        {
            Tasker tasker(session, async_bundle);
            job = tasker.post_task();
            Sleep(40);
            auto other = tasker.post_task();
            auto report = other->wait();
            CHECK(report.error && report.error->code == ErrorCode::conflict);
        }
        job->cancel();
        auto report = job->wait();
        CHECK(report.status == RunStatus::cancelled);
        CHECK(report.elapsed_ms < 250);
    });
    test("two devices and 100 start-stop cycles", [&] {
        auto worker = [&](const std::string &id) {
            for (int i = 0; i < 100; ++i) {
                auto clock = std::make_shared<ManualClock>();
                auto controller = std::make_shared<ReplayController>(
                    std::vector<ReplayStep>{{before, click}, {after, {}}}, clock);
                DeviceSession session(controller, {id, {640, 360}}, clock);
                auto report = TaskRunner::run(session, *bundle, {});
                CHECK(report.status == RunStatus::completed);
                CHECK(controller->actions().size() == 1);
            }
        };
        auto first = std::async(std::launch::async, worker, "device-1"),
             second = std::async(std::launch::async, worker, "device-2");
        first.get();
        second.get();
    });
    test("sample recognition remains within baseline", [&] {
        if (!std::filesystem::is_regular_file(root / L"data/samples/1.png"))
            return;
        auto image = ar::core::read_image(root / L"data/samples/1.png").value(),
             templ = ar::core::read_image(root / L"data/samples/2.png").value();
        for (auto name : ar::vision::matcher_backends())
            if (name == "ncc-cpu" || name == "ssim-cpu" || name == "ncc-cuda" || name == "ssim-cuda") {
                auto matcher = ar::vision::create_matcher(name);
                CHECK(matcher);
                auto match = matcher->find_checked(image, templ, 0.9f, nullptr);
                CHECK(match && match.value().found);
                CHECK(cv::norm(match.value().center - cv::Point(1285, 546)) <= 3);
                CHECK(match.value().score > 0.985f);
            }
    });
    std::cout << "RESULT passed=" << passed << " failed=" << failed
              << " artifacts=" << ar::core::wide_to_utf8(temp.wstring()) << "\n";
    return failed ? 1 : 0;
}
