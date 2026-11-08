// Demo / smoke test for the framework modules: SSIM template matching on the
// GPU, PaddleOCR (cpu + gpu backends), MuMu device management and the proxy
// client. Behaviour mirrors the original main.cpp.
#include <control/Controller.h>
#include <core/Path.hpp>
#include <core/Str.hpp>
#include <device/DeviceController.h>
#include <log/ConsoleLogger.h>
#include <net/ProxyClient.h>
#include <plugin/PluginLoader.h>
#include <vision/Matcher.h>
#include <vision/Ocr.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <numeric>
#include <string>
#include <tuple>

#include <opencv2/opencv.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace {

std::string env_or_empty(const char *name) {
    const char *value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

} // namespace

int main() {
    ar::log::ConsoleLogger logger;

    // Locate the project root from the exe position (independent of cwd).
    const auto root = ar::core::find_root_containing("models");
    const auto model_dir = root.empty() ? std::filesystem::path("models") : root / "models";
    const auto sample_dir = root.empty() ? std::filesystem::path("data\\samples") : root / "data" / "samples";

    // Plugin DLLs (matcher / controller / device / OCR backends) live in the
    // plugins\ directory next to the executable.
    const auto plugin_backends = ar::plugin::load_plugins(ar::core::exe_dir() / "plugins", &logger);
    logger.log(
        ar::log::Level::info,
        std::format("Plugin backends: {}",
                    plugin_backends.empty()
                        ? std::string("(none)")
                        : std::accumulate(plugin_backends.begin(), plugin_backends.end(), std::string(),
                                          [](const std::string &acc, const std::string &n) {
                                              return acc + (acc.empty() ? "" : ", ") + n;
                                          })));

    // --- template matching -------------------------------------------------
    const auto image = cv::imread((sample_dir / "1.png").string());
    const auto template_image = cv::imread((sample_dir / "2.png").string());
    if (image.empty() || template_image.empty()) {
        logger.log(ar::log::Level::error, "Unable to load image recognition samples");
        return 1;
    }

    auto matcher = ar::vision::create_matcher("ssim-cuda", {&logger});
    if (!matcher) {
        logger.log(ar::log::Level::error, "Image matcher is unavailable");
        return 1;
    }
    const auto match = matcher->find(image, template_image, 0.80f);
    logger.log(ar::log::Level::info,
               std::format("Image recognition result: {}, {} (found: {}, score: {:.4f})", match.center.x,
                           match.center.y, match.found, match.score));

    // Cross-backend sanity check: the CPU backend should agree on the hit.
    if (auto cpu_matcher = ar::vision::create_matcher("ssim-cpu", {&logger})) {
        const auto cpu_match = cpu_matcher->find(image, template_image, 0.80f);
        logger.log(ar::log::Level::info,
                   std::format("Image recognition result (ssim-cpu): {}, {} (found: {}, score: {:.4f})",
                               cpu_match.center.x, cpu_match.center.y, cpu_match.found, cpu_match.score));
    }

    // NCC backends (ncc-cuda / ncc-cpu plugins). Score domain differs from
    // SSIM: NCC lies in [-1, 1], so thresholds are not interchangeable (~0.9
    // suits crisp UI templates). All backends should agree on the hit.
    if (auto ncc_matcher = ar::vision::create_matcher("ncc-cuda", {&logger})) {
        const auto ncc_match = ncc_matcher->find(image, template_image, 0.90f);
        logger.log(ar::log::Level::info,
                   std::format("Image recognition result (ncc-cuda): {}, {} (found: {}, score: {:.4f})",
                               ncc_match.center.x, ncc_match.center.y, ncc_match.found, ncc_match.score));
        // ROI smoke test: searching a sub-rectangle that contains the hit must
        // land on the same absolute position.
        const cv::Rect roi(400, 50, 1800, 1100);
        const auto roi_match = ncc_matcher->find(image, template_image, 0.90f, &roi);
        logger.log(ar::log::Level::info,
                   std::format("Image recognition result (ncc-cuda, roi): {}, {} (found: {}, score: {:.4f})",
                               roi_match.center.x, roi_match.center.y, roi_match.found, roi_match.score));
    } else {
        logger.log(ar::log::Level::error, "NCC CUDA matcher is unavailable");
    }
    if (auto ncc_cpu_matcher = ar::vision::create_matcher("ncc-cpu", {&logger})) {
        const auto ncc_match = ncc_cpu_matcher->find(image, template_image, 0.90f);
        logger.log(ar::log::Level::info,
                   std::format("Image recognition result (ncc-cpu): {}, {} (found: {}, score: {:.4f})",
                               ncc_match.center.x, ncc_match.center.y, ncc_match.found, ncc_match.score));
    }

    // --- optional benchmark (AR_BENCH=1): the script-typical loop of repeated
    // find() with a stable template, cold cache vs warm cache ----------------
    if (std::getenv("AR_BENCH") != nullptr) {
        auto template_copy = template_image.clone(); // different memory -> cache miss
        const auto timed = [&](bool warm_cache) {
            constexpr int kFrames = 300;
            const auto begin = std::chrono::steady_clock::now();
            for (int frame = 0; frame < kFrames; ++frame) {
                const cv::Mat &templ =
                    warm_cache ? template_image : (frame % 2 == 0 ? template_image : template_copy);
                matcher->find(image, templ, 0.80f);
            }
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                         begin)
                       .count() /
                   static_cast<double>(kFrames) / 1000.0;
        };
        timed(false); // warm-up allocations
        logger.log(
            ar::log::Level::info,
            std::format("SSIM benchmark cold (template rebuilt per frame): {:.2f} ms/frame", timed(false)));
        logger.log(ar::log::Level::info,
                   std::format("SSIM benchmark warm (cached template): {:.2f} ms/frame", timed(true)));

        if (auto cpu_matcher = ar::vision::create_matcher("ssim-cpu", {&logger})) {
            constexpr int kCpuFrames = 30;
            cpu_matcher->find(image, template_image, 0.80f); // warm-up
            const auto cpu_begin = std::chrono::steady_clock::now();
            for (int frame = 0; frame < kCpuFrames; ++frame)
                cpu_matcher->find(image, template_image, 0.80f);
            const double cpu_ms = std::chrono::duration_cast<std::chrono::microseconds>(
                                      std::chrono::steady_clock::now() - cpu_begin)
                                      .count() /
                                  static_cast<double>(kCpuFrames) / 1000.0;
            logger.log(ar::log::Level::info, std::format("SSIM-CPU benchmark: {:.2f} ms/frame", cpu_ms));
        }

        // --- NCC backends under the same warm-template loop ----------------
        const auto measure_ms = [](auto &&fn, int frames) {
            fn(); // warm-up: allocations, CUDA context, template caches
            const auto begin = std::chrono::steady_clock::now();
            for (int frame = 0; frame < frames; ++frame)
                fn();
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                         begin)
                       .count() /
                   static_cast<double>(frames) / 1000.0;
        };

        if (auto ncc_matcher = ar::vision::create_matcher("ncc-cuda", {&logger})) {
            const double ms = measure_ms([&] { ncc_matcher->find(image, template_image, 0.90f); }, 300);
            logger.log(ar::log::Level::info,
                       std::format("NCC-CUDA benchmark (cached template): {:.2f} ms/frame", ms));
        }
        if (auto ncc_cpu_matcher = ar::vision::create_matcher("ncc-cpu", {&logger})) {
            const double ms = measure_ms([&] { ncc_cpu_matcher->find(image, template_image, 0.90f); }, 30);
            logger.log(ar::log::Level::info,
                       std::format("NCC-CPU benchmark (cached template): {:.2f} ms/frame", ms));
        }

        // --- OpenCV matchTemplate NCC at full resolution, for context -------
        // ncc-cpu runs this same cv::matchTemplate call on the 640px
        // working-size frame; this shows what the downscale buys.
        {
            cv::Mat templ_gray_full;
            cv::cvtColor(template_image, templ_gray_full, cv::COLOR_BGR2GRAY);
            cv::Mat frame_full;
            cv::cvtColor(image, frame_full, cv::COLOR_BGR2GRAY);

            double score = 0.0;
            cv::Point best;
            const double full_ms = measure_ms(
                [&] {
                    cv::Mat result;
                    cv::matchTemplate(frame_full, templ_gray_full, result, cv::TM_CCOEFF_NORMED);
                    cv::minMaxLoc(result, nullptr, &score, nullptr, &best);
                },
                10);
            logger.log(ar::log::Level::info,
                       std::format("OpenCV NCC@full benchmark: {:.2f} ms/frame", full_ms));
        }
    }

    // --- OCR (cpu + gpu backends share the paddle core) --------------------
    const ar::vision::OcrParams ocr_params{(model_dir / "det.sim.onnx").string(),
                                           (model_dir / "rec.sim.onnx").string(),
                                           (model_dir / "ppocr_keys_v1.txt").string(), 0.8f};
    auto ocr_cpu = ar::vision::create_ocr("paddle-ocr-cpu", ocr_params, &logger);
    auto ocr_gpu = ar::vision::create_ocr("paddle-ocr-gpu", ocr_params, &logger);
    if (!ocr_cpu || !ocr_gpu) {
        logger.log(ar::log::Level::error, "OCR initialization failed");
        return 1;
    }

    const auto ocr_input = cv::imread((sample_dir / "3.jpg").string());
    if (ocr_input.empty()) {
        logger.log(ar::log::Level::error, "Unable to load OCR sample image");
        return 1;
    }

    const auto run_ocr = [&](ar::vision::IOcrEngine &ocr) {
        const auto warmup = ocr.recognize(ocr_input); // first call warms the session
        const auto begin = std::chrono::steady_clock::now();
        const auto items = ocr.recognize(ocr_input);
        const auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin)
                .count();
        std::string joined;
        for (const auto &item : items)
            joined += (joined.empty() ? "" : " | ") + item.text;
        return std::make_tuple(elapsed_ms, items.size(), joined);
    };

    const auto [cpu_ms, cpu_count, cpu_text] = run_ocr(*ocr_cpu);
    logger.log(ar::log::Level::info,
               std::format("CPU OCR: {} regions, {} ms, text: {}", cpu_count, cpu_ms, cpu_text));
    const auto [gpu_ms, gpu_count, gpu_text] = run_ocr(*ocr_gpu);
    logger.log(ar::log::Level::info,
               std::format("GPU OCR: {} regions, {} ms, text: {}", gpu_count, gpu_ms, gpu_text));

    if (gpu_count == 0) {
        logger.log(ar::log::Level::error, "GPU OCR recognized no text regions");
        return 1;
    }

    // --- device management + proxy (opt-in via environment variables) ------
    const std::string mumu_path = env_or_empty("AR_MUMU_PATH");
    const std::string subscribe_url = env_or_empty("AR_SUBSCRIBE_URL");
    const std::string index_text = env_or_empty("AR_MUMU_INDEX");
    const std::string port_text = env_or_empty("AR_ADB_PORT");
    if (!mumu_path.empty() && !subscribe_url.empty() && !index_text.empty() && !port_text.empty()) {
        const int32_t device_index = std::atoi(index_text.c_str());
        const int32_t adb_port = std::atoi(port_text.c_str());
        const std::filesystem::path mumu_fs_path(ar::core::utf8_to_wide(mumu_path));

        auto device = ar::device::create_device_controller("mumu", mumu_fs_path, device_index, &logger);
        ar::control::ControllerConfig controller_config;
        controller_config.adb_port = static_cast<uint16_t>(adb_port);
        controller_config.mumu_path = mumu_fs_path;
        controller_config.mumu_index = device_index;
        controller_config.logger = &logger;
        auto controller = ar::control::create_controller("mumu", controller_config);
        if (device && controller && device->launch() == ar::device::Status::ok) {
            ar::net::ProxyClient proxy(subscribe_url, adb_port, &logger);
            if (proxy.Initialize()) {
                const auto nodes = proxy.GetNodeNames();
                if (!nodes.empty()) {
                    proxy.EnableProxy(nodes.front());
                    proxy.DisableProxy();
                }
            }
        }
    }
    return 0;
}
