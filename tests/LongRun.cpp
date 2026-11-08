#include "runtime/Tasker.h"
#include "runtime/Replay.h"
#include "plugin/PluginLoader.h"
#include "log/ConsoleLogger.h"
#include "examples/Demo.h"
#include <iostream>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
namespace {
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
} // namespace
int multi_device_test(const std::filesystem::path &exe, const std::filesystem::path &mumu) {
    try {
        ar::log::ConsoleLogger log(ar::log::Level::warning);
        ar::plugin::load_plugins(exe.parent_path() / L"plugins", &log);
        ar::plugin::freeze_registries();
        auto worker = [&](int index, const std::string &serial) {
            ar::control::ControllerConfig config;
            config.logger = &log;
            config.adb_path = exe.parent_path() / L"adb.exe";
            config.serial = serial;
            config.mumu_path = mumu;
            config.mumu_index = index;
            auto controller = ar::control::create_controller("adb", config);
            require(bool(controller) && controller->connect(), "Multi-device connect failed");
            auto first = controller->capture_checked();
            require(bool(first), "Multi-device capture failed");
            ar::runtime::ResourceBundle bundle;
            bundle.name = "multi-device-capture";
            bundle.version = "1";
            bundle.entry = "Capture";
            bundle.reference_size = first.value().size();
            bundle.limits.timeout_ms = 5000;
            ar::runtime::Node node;
            node.name = "Capture";
            node.action.type = ar::runtime::ActionType::wait;
            node.action.duration_ms = 5;
            bundle.nodes.emplace(node.name, node);
            ar::runtime::DeviceSession session(controller, {serial, bundle.reference_size});
            for (int i = 0; i < 100; ++i) {
                auto report = ar::runtime::TaskRunner::run(session, bundle, {});
                require(report.status == ar::runtime::RunStatus::completed, "Multi-device task failed");
                auto frame = session.capture({}, 5000);
                require(bool(frame) && frame.value().device_id == serial, "Device frame mixed");
            }
            return std::make_pair(serial, bundle.reference_size);
        };
        auto first = std::async(std::launch::async, worker, 0, "127.0.0.1:16384");
        auto second = std::async(std::launch::async, worker, 1, "127.0.0.1:16416");
        const auto a = first.get(), b = second.get();
        std::cout << "PASS real multi-device tasks=100+100 " << a.first << "=" << a.second.width << "x"
                  << a.second.height << " " << b.first << "=" << b.second.width << "x" << b.second.height
                  << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL real multi-device: " << e.what() << "\n";
        return 1;
    }
}
int soak_test(const std::filesystem::path &exe, const std::filesystem::path &root, int seconds) {
    try {
        require(seconds >= 1 && seconds <= 86400, "Soak duration must be 1..86400s");
        ar::plugin::load_plugins(exe.parent_path() / L"plugins");
        ar::plugin::freeze_registries();
        const auto directory =
            root / L"out/soak" / ar::core::utf8_to_wide(std::to_string(GetCurrentProcessId()));
        require(bool(ar::examples::make_demo(directory)), "Cannot generate soak resource");
        auto resource = ar::runtime::ResourceBundle::load(directory);
        require(bool(resource), "Soak resource invalid");
        auto before = ar::core::read_image(directory / L"before.png").value(),
             after = ar::core::read_image(directory / L"after.png").value();
        ar::runtime::Action click;
        click.type = ar::runtime::ActionType::click;
        click.target = cv::Point(320, 180);
        const auto started = std::chrono::steady_clock::now(),
                   deadline = started + std::chrono::seconds(seconds);
        uint64_t runs = 0;
        DWORD baseline_handles = 0;
        SIZE_T baseline_memory = 0;
        std::ofstream samples(directory / L"samples.csv");
        samples << "runs,wall_seconds,handles,private_bytes\n";
        for (;;) {
            auto clock = std::make_shared<ar::runtime::ManualClock>();
            auto controller = std::make_shared<ar::runtime::ReplayController>(
                std::vector<ar::runtime::ReplayStep>{{before, click}, {after, {}}}, clock);
            ar::runtime::DeviceSession session(controller, {"soak", {640, 360}}, clock);
            auto report = ar::runtime::TaskRunner::run(session, *resource.value(), {});
            require(report.status == ar::runtime::RunStatus::completed && controller->complete(),
                    "Soak task failed");
            ++runs;
            if (runs == 100 || runs % 1000 == 0 || std::chrono::steady_clock::now() >= deadline) {
                DWORD handles = 0;
                PROCESS_MEMORY_COUNTERS_EX memory{};
                memory.cb = sizeof(memory);
                require(GetProcessHandleCount(GetCurrentProcess(), &handles) != FALSE,
                        "Handle count unavailable");
                require(GetProcessMemoryInfo(GetCurrentProcess(),
                                             reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),
                                             sizeof(memory)) != FALSE,
                        "Memory counters unavailable");
                if (runs <= 100) {
                    baseline_handles = handles;
                    baseline_memory = memory.PrivateUsage;
                } else {
                    require(handles <= baseline_handles + 5, "Soak handle growth detected");
                    require(memory.PrivateUsage <= baseline_memory + 128 * 1024 * 1024,
                            "Soak private memory growth exceeds 128MiB");
                }
                const auto elapsed =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
                samples << runs << "," << elapsed << "," << handles << "," << memory.PrivateUsage << "\n";
                samples.flush();
            }
            if (std::chrono::steady_clock::now() >= deadline)
                break;
        }
        std::cout << "PASS in-process soak real_seconds=" << seconds << " runs=" << runs
                  << " samples=" << ar::core::wide_to_utf8((directory / L"samples.csv").wstring()) << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL soak: " << e.what() << "\n";
        return 1;
    }
}
