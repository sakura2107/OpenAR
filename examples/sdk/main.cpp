#include "ar/OpenAR.h"
#include "core/Path.hpp"
#include <iostream>
int wmain(int argc, wchar_t **argv) {
    if (argc != 2) {
        std::cerr << "Usage: OpenARSDKExample <demo-resource-directory>\n";
        return 2;
    }
    ar::plugin::load_plugins(ar::core::exe_dir() / L"plugins");
    ar::plugin::freeze_registries();
    auto resource = ar::runtime::ResourceBundle::load(argv[1]);
    if (!resource) {
        std::cerr << resource.error().message;
        return 2;
    }
    auto clock = std::make_shared<ar::runtime::ManualClock>();
    auto replay = ar::runtime::ReplayController::load(std::filesystem::path(argv[1]) / L"replay.json", clock);
    if (!replay) {
        std::cerr << replay.error().message;
        return 2;
    }
    auto session = std::make_shared<ar::runtime::DeviceSession>(
        replay.value(), ar::runtime::SessionConfig{"example", resource.value()->reference_size}, clock);
    ar::runtime::Tasker tasker(session, resource.value());
    auto report = tasker.post_task()->wait();
    std::cout << "transitions=" << report.transitions
              << " completed=" << (report.status == ar::runtime::RunStatus::completed) << "\n";
    return report.status == ar::runtime::RunStatus::completed ? 0 : 1;
}
