#pragma once
#include "runtime/Session.h"
#include "core/IO.h"
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <condition_variable>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace ar::cli {
struct CaptureOptions {
    std::filesystem::path output;
    std::string language;
    uint32_t interval_ms = 3000;
    uint32_t count = 300;
    uint32_t timeout_ms = 5000;
    uint32_t max_transient_frames = 0;
};
struct CaptureReport {
    std::filesystem::path directory;
    uint32_t saved = 0;
    uint32_t skipped = 0;
    bool cancelled = false;
};

// Deliberately has no task runner, lifecycle controller or input operation.
// The caller must supply OpenAR's native MuMu controller. Tests use a fake.
inline CaptureReport capture_only(std::shared_ptr<ar::control::IController> controller,
                                  ar::runtime::SessionConfig config, const CaptureOptions &options,
                                  std::stop_token stop = {}) {
    if (!controller || controller->name() != "mumu" || options.output.empty() ||
        (options.language != "ja" && options.language != "zh-Hant") || options.count < 1 ||
        options.count > 10000 || options.interval_ms < 100 || options.interval_ms > 600000 ||
        options.timeout_ms < 1 || options.timeout_ms > 3600000)
        throw std::invalid_argument("Invalid capture options (MuMu, ja/zh-Hant, count 1..10000, interval 100..600000 ms)");
    config.reference_size = {1280, 720};
    ar::runtime::DeviceSession session(controller, config);
    auto lease = session.acquire_run();
    if (!lease)
        throw std::runtime_error(lease.error().message);
    const auto now = std::chrono::system_clock::now();
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    CaptureReport report;
    const auto base = std::filesystem::absolute(options.output);
    std::filesystem::create_directories(base);
    // Exclusive directory creation avoids overwriting any previous collection.
    for (int suffix = 0;; ++suffix) {
        report.directory = base / (std::to_string(stamp) + "-" + std::to_string(GetCurrentProcessId()) +
                                   "-" + std::to_string(suffix));
        if (std::filesystem::create_directory(report.directory))
            break;
    }
    std::filesystem::create_directory(report.directory / L"images");
    std::ofstream manifest(report.directory / L"capture.jsonl", std::ios::binary);
    if (!manifest)
        throw std::runtime_error("Cannot create capture manifest");
    std::mutex mutex;
    std::condition_variable_any wait;
    while (report.saved < options.count && !stop.stop_requested()) {
        const auto started = std::chrono::steady_clock::now();
        auto result = session.capture(stop, options.timeout_ms);
        if (!result) {
            if (result.error().code == ar::core::ErrorCode::cancelled) {
                report.cancelled = true;
                return report;
            }
            if (result.error().code == ar::core::ErrorCode::unsupported && report.skipped < options.max_transient_frames) {
                ++report.skipped;
                std::cerr << "Skipping transient frame " << report.skipped << ": " << result.error().message << std::endl;
                std::unique_lock lock(mutex);
                wait.wait_until(lock, stop, started + std::chrono::milliseconds(options.interval_ms), [] { return false; });
                continue;
            }
            throw std::runtime_error("Capture stopped after " + std::to_string(report.saved) +
                                     " frames: " + result.error().message + "; output=" +
                                     ar::core::wide_to_utf8(report.directory.wstring()));
        }
        const auto &frame = result.value();
        std::ostringstream filename;
        filename << "images/" << std::setw(6) << std::setfill('0') << report.saved + 1 << ".png";
        auto written = ar::core::write_image(report.directory / filename.str(), frame.image);
        if (!written)
            throw std::runtime_error(written.error().message);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> json(buffer);
        json.StartObject();
        json.Key("schema_version"); json.Int(1);
        json.Key("language"); json.String(options.language.c_str());
        json.Key("source"); json.String("OpenAR/MuMu/native");
        json.Key("image"); json.String(filename.str().c_str());
        json.Key("utc_unix_ms"); json.Int64(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        json.Key("source_size"); json.StartArray(); json.Int(frame.original.cols); json.Int(frame.original.rows); json.EndArray();
        json.Key("size"); json.StartArray(); json.Int(frame.image.cols); json.Int(frame.image.rows); json.EndArray();
        json.Key("rotation"); json.Int(config.rotation);
        json.Key("viewport"); json.StartArray(); json.Int(config.viewport.x); json.Int(config.viewport.y);
        json.Int(config.viewport.width); json.Int(config.viewport.height); json.EndArray();
        // Non-cryptographic pixel hash for duplicate triage only; corpus uses SHA256.
        json.Key("pixel_hash"); json.String(std::to_string(ar::core::image_hash(frame.image)).c_str());
        json.EndObject();
        manifest << buffer.GetString() << '\n';
        manifest.flush();
        if (!manifest)
            throw std::runtime_error("Cannot write capture manifest");
        ++report.saved;
        std::cout << "capture=" << report.saved << " image="
                  << ar::core::wide_to_utf8((report.directory / filename.str()).wstring()) << std::endl;
        if (report.saved < options.count) {
            std::unique_lock lock(mutex);
            wait.wait_until(lock, stop, started + std::chrono::milliseconds(options.interval_ms), [] { return false; });
        }
    }
    report.cancelled = stop.stop_requested();
    return report;
}
} // namespace ar::cli
