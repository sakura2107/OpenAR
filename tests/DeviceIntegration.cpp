#include "runtime/Tasker.h"
#include "plugin/PluginLoader.h"
#include "device/DeviceController.h"
#include "core/IO.h"
#include "log/ConsoleLogger.h"
#include <iostream>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
namespace {
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
} // namespace
int device_integration(const std::filesystem::path &exe, const std::filesystem::path &root,
                       const std::filesystem::path &mumu, int index, const std::string &serial) {
    try {
        ar::log::ConsoleLogger log(ar::log::Level::warning);
        ar::plugin::load_plugins(exe.parent_path() / L"plugins", &log);
        ar::plugin::freeze_registries();
        const auto out =
            root / L"out/integration" / ar::core::utf8_to_wide("device-" + std::to_string(index));
        std::filesystem::create_directories(out);
        ar::control::ControllerConfig config;
        config.logger = &log;
        config.adb_path = exe.parent_path() / L"adb.exe";
        config.serial = serial;
        config.mumu_path = mumu;
        config.mumu_index = index;
        config.command_timeout_ms = 5000;
        auto lifecycle = ar::device::create_device_controller("mumu", mumu, index, &log);
        require(bool(lifecycle), "Lifecycle backend unavailable");
        require(lifecycle->check_device_state() == ar::device::Status::ok, "Device must already be running");
        auto adb = ar::control::create_controller("adb", config);
        require(bool(adb) && adb->connect(), "ADB connection failed");
        auto stable = [&](cv::Rect region) {
            cv::Mat previous;
            int same = 0;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
            for (;;) {
                require(std::chrono::steady_clock::now() < deadline, "Calibration page did not settle");
                auto image = adb->capture_checked();
                require(bool(image), "Calibration capture failed");
                require(ar::core::valid_roi(region, image.value().size()), "Calibration orientation changed");
                cv::Scalar mean, deviation;
                cv::meanStdDev(image.value()(region), mean, deviation);
                const bool text_visible = deviation[0] > 20;
                if (text_visible && !previous.empty() &&
                    cv::norm(image.value()(region), previous, cv::NORM_L1) / (region.area() * 3) < 0.2)
                    ++same;
                else
                    same = 0;
                previous = image.value()(region).clone();
                if (same >= 3)
                    return image.value();
                Sleep(100);
            }
        };
        require(adb->stop_app("com.android.settings"), "Settings reset failed");
        require(adb->start_app("com.android.settings"), "Settings launch failed");
        Sleep(500);
        // This integration fixture is Android 15 Chinese, landscape 1280x720.
        // Coordinates were inspected on this instance. Only navigation is exercised.
        auto settings = adb->capture_checked();
        require(bool(settings), "Settings capture failed");
        require(settings.value().size() == cv::Size(1280, 720),
                "Use the Android 15 landscape fixture at 1280x720");
        auto stable_settings = stable(cv::Rect(110, 212, 250, 44));
        auto button = stable_settings(cv::Rect(110, 212, 250, 44)).clone();
        require(adb->click({230, 230}), "Network page input failed");
        Sleep(300);
        auto network = stable(cv::Rect(36, 208, 330, 62));
        auto title = network(cv::Rect(36, 208, 330, 62)).clone();
        require(adb->press_key(4), "Return to settings failed");
        Sleep(200);
        require(bool(ar::core::write_image(out / L"open.png", button)) &&
                    bool(ar::core::write_image(out / L"ready.png", title)),
                "Cannot write calibration templates");
        {
            std::ofstream manifest(out / L"manifest.json");
            manifest
                << R"({"schema_version":1,"name":"android-settings-live","version":"android15-zh-landscape","reference_size":[1280,720],"pipeline":"pipeline.json","templates":{"open":"open.png","ready":"ready.png"}})";
        }
        {
            std::ofstream pipeline(out / L"pipeline.json");
            pipeline
                << R"({"entry":"Reset","limits":{"timeout_ms":20000,"poll_ms":100,"max_transitions":10,"max_recoveries":1},"nodes":{"Reset":{"recognition":{"type":"Direct"},"action":{"type":"StopApp","package":"com.android.settings"},"next":["Start"],"timeout_ms":5000},"Start":{"recognition":{"type":"Direct"},"action":{"type":"StartApp","package":"com.android.settings"},"next":["Open"],"timeout_ms":5000},"Open":{"recognition":{"type":"Template","template":"open","roi":[70,175,400,140],"threshold":0.9},"action":{"type":"Click"},"next":["Ready"],"timeout_ms":5000},"Ready":{"recognition":{"type":"Template","template":"ready","roi":[20,180,440,130],"threshold":0.9},"action":{"type":"Key","keycode":4},"next":["Returned"],"timeout_ms":5000},"Returned":{"recognition":{"type":"Template","template":"open","roi":[70,175,400,140],"threshold":0.9},"action":{"type":"None"},"timeout_ms":5000}}})";
        }
        auto loaded = ar::runtime::ResourceBundle::load(out);
        require(bool(loaded), "Live resource invalid");
        adb->disconnect();
        for (const auto backend : {"adb", "mumu"}) {
            auto controller = ar::control::create_controller(backend, config);
            require(bool(controller), "Controller unavailable");
            require(controller->connect(), "Probe connect failed");
            auto probe = controller->capture_checked();
            require(bool(probe), "Probe screenshot failed");
            ar::core::write_image(out / ar::core::utf8_to_wide(std::string(backend) + "-probe.png"),
                                  probe.value());
            std::cout << backend << " screenshot=" << probe.value().cols << "x" << probe.value().rows
                      << std::endl;
            ar::runtime::SessionConfig session_config{serial, {1280, 720}};
            if (probe.value().size() == cv::Size(720, 1280))
                session_config.rotation = 90;
            auto session = std::make_shared<ar::runtime::DeviceSession>(controller, session_config);
            ar::runtime::TaskOptions options;
            options.trace_directory = out / ar::core::utf8_to_wide(backend) / L"traces";
            auto report = ar::runtime::TaskRunner::run(*session, *loaded.value(), {}, {}, {}, options);
            if (report.error)
                std::cerr << report.error->operation << ": " << report.error->message << "\n";
            require(report.status == ar::runtime::RunStatus::completed, "Navigation task failed");
            require(report.transitions == 5, "Unexpected action count");
            auto image = session->capture({}, 5000);
            require(bool(image), "Post-task capture failed");
            require(
                bool(ar::core::write_image(out / ar::core::utf8_to_wide(std::string(backend) + "-final.png"),
                                           image.value().image)),
                "Cannot write final capture");
            std::cout << "PASS live " << backend << " transitions=" << report.transitions
                      << " frames=" << report.frames << " elapsed_ms=" << report.elapsed_ms << "\n";
        }
        // Repeat SDK ownership transitions without starting/stopping the emulator.
        const auto start = std::chrono::steady_clock::now();
        DWORD handles_before = 0, handles_after = 0;
        GetProcessHandleCount(GetCurrentProcess(), &handles_before);
        for (int i = 0; i < 100; ++i) {
            auto controller = ar::control::create_controller("mumu", config);
            require(bool(controller) && controller->connect(), "SDK reconnect failed");
            auto capture = controller->capture_checked();
            require(bool(capture) && capture.value().size() == cv::Size(1280, 720),
                    "SDK repeated capture failed");
            require(controller->disconnect(), "SDK disconnect failed");
        }
        GetProcessHandleCount(GetCurrentProcess(), &handles_after);
        require(handles_after <= handles_before + 5, "Native handle growth detected");
        std::cout << "PASS live SDK cycles=100 elapsed_ms="
                  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                           start)
                         .count()
                  << " handles_before=" << handles_before << " handles_after=" << handles_after << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL integration: " << e.what() << "\n";
        return 1;
    }
}
