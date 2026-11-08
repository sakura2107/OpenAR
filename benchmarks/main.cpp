#include "core/IO.h"
#include "plugin/PluginLoader.h"
#include "vision/Matcher.h"
#include "vision/Ocr.h"
#include "log/ConsoleLogger.h"
#include <iostream>
#include <chrono>
#include <algorithm>
int wmain(int argc, wchar_t **argv) {
    SetConsoleOutputCP(CP_UTF8);
    try {
        if (argc < 2) {
            std::cerr << "Usage: OpenARBenchmark <source-root> [iterations=10]\n";
            return 2;
        }
        std::wstring buffer(32768, L'\0');
        buffer.resize(GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size())));
        const auto exe = std::filesystem::path(buffer).parent_path();
        const auto root = std::filesystem::absolute(argv[1]);
        const int iterations = argc > 2 ? std::stoi(argv[2]) : 10;
        if (iterations < 1 || iterations > 10000)
            throw std::invalid_argument("Iterations outside 1..10000");
        ar::log::ConsoleLogger log(ar::log::Level::warning);
        ar::plugin::load_plugins(exe / L"plugins", &log);
        ar::plugin::freeze_registries();
        auto frame = ar::core::read_image(root / L"data/samples/1.png"),
             templ = ar::core::read_image(root / L"data/samples/2.png");
        if (!frame || !templ)
            throw std::runtime_error("Sample images missing");
        std::cout << "backend,median_ms,p95_ms,score,x,y,count\n";
        auto measure = [&](const std::string &name, auto operation) {
            std::vector<double> timings;
            for (int i = 0; i < iterations + 1; ++i) {
                const auto start = std::chrono::steady_clock::now();
                operation();
                if (i)
                    timings.push_back(
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                            .count());
            }
            std::sort(timings.begin(), timings.end());
            std::cout << name << "," << timings[timings.size() / 2] << ","
                      << timings[std::min(timings.size() - 1, size_t(std::ceil(timings.size() * 0.95)) - 1)]
                      << ",";
        };
        for (auto name : ar::vision::matcher_backends()) {
            auto matcher = ar::vision::create_matcher(name);
            if (!matcher)
                throw std::runtime_error("Backend initialization failed");
            ar::vision::MatchResult match;
            measure(std::string(name), [&] {
                auto r = matcher->find_checked(frame.value(), templ.value(), 0.9f, nullptr);
                if (!r)
                    throw std::runtime_error(r.error().message);
                match = r.value();
            });
            std::cout << match.score << "," << match.center.x << "," << match.center.y << ",1\n";
        }
        ar::vision::OcrParams p;
        auto utf8 = [&](const wchar_t *file) {
            return ar::core::wide_to_utf8((root / L"models" / file).wstring());
        };
        p.det_model = utf8(L"det.sim.onnx");
        p.rec_model = utf8(L"rec.sim.onnx");
        p.dictionary = utf8(L"ppocr_keys_v1.txt");
        auto ocr_input = ar::core::read_image(root / L"data/samples/3.jpg");
        if (!ocr_input)
            throw std::runtime_error("OCR sample missing");
        for (auto name : ar::vision::ocr_backends()) {
            auto ocr = ar::vision::create_ocr(name, p, &log);
            if (!ocr)
                throw std::runtime_error("OCR initialization failed");
            std::vector<ar::vision::OcrItem> items;
            measure(std::string(name), [&] {
                auto r = ocr->recognize_checked(ocr_input.value(), nullptr);
                if (!r)
                    throw std::runtime_error(r.error().message);
                items = std::move(r.value());
            });
            std::cout << "0,0,0," << items.size() << "\n";
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 2;
    }
}
